/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

#include <vector>
#include <algorithm>
#include <exception>
#include <functional>
#include <unordered_set>

#include "BluetoothDeviceManager.h"
#include "btmgr.h"

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
#include "BluetoothPersistenceAdapter.h"
#endif


namespace WPEFramework {
    namespace Plugin {

        namespace {
            bool missingFromPersistentStore(Core::hresult result)
            {
                return (Core::ERROR_NOT_EXIST == result) || (Core::ERROR_UNKNOWN_KEY == result);
            }
        } // namespace

        // ---------------------------------------------------------------------------
        // Migration marker helpers
        // ---------------------------------------------------------------------------

        Core::hresult BluetoothDeviceManager::readMigrationVersion(std::string& version)
        {
            if (_service == nullptr) {
                LOGERR("readMigrationVersion: service is null");
                return Core::ERROR_GENERAL;
            }

            Exchange::IStore* pPersistentStore = _service->QueryInterfaceByCallsign<Exchange::IStore>(PERSISTENT_STORE_CALLSIGN);
            if (pPersistentStore == nullptr) {
                LOGERR("readMigrationVersion: failed to get PersistentStore interface");
                return Core::ERROR_GENERAL;
            }

            string ver;
            Core::hresult result = pPersistentStore->GetValue(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_MIGRATION_VERSION, ver);
            pPersistentStore->Release();

            if (Core::ERROR_NONE == result) {
                version = ver;
            }
            return result;
        }

        Core::hresult BluetoothDeviceManager::writeMigrationVersion()
        {
            if (_service == nullptr) {
                LOGERR("writeMigrationVersion: service is null");
                return Core::ERROR_GENERAL;
            }

            Exchange::IStore* pPersistentStore = _service->QueryInterfaceByCallsign<Exchange::IStore>(PERSISTENT_STORE_CALLSIGN);
            if (pPersistentStore == nullptr) {
                LOGERR("writeMigrationVersion: failed to get PersistentStore interface");
                return Core::ERROR_GENERAL;
            }

            Core::hresult result = pPersistentStore->SetValue(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_MIGRATION_VERSION, BLUETOOTH_MIGRATION_VERSION);
            pPersistentStore->Release();

            if (Core::ERROR_NONE != result) {
                LOGERR("writeMigrationVersion: failed to write migration version, hresult=%d", result);
            }
            return result;
        }

        Core::hresult BluetoothDeviceManager::deleteMigrationVersion()
        {
            if (_service == nullptr) {
                LOGERR("deleteMigrationVersion: service is null");
                return Core::ERROR_GENERAL;
            }

            Exchange::IStore* pPersistentStore = _service->QueryInterfaceByCallsign<Exchange::IStore>(PERSISTENT_STORE_CALLSIGN);
            if (pPersistentStore == nullptr) {
                LOGERR("deleteMigrationVersion: failed to get PersistentStore interface");
                return Core::ERROR_GENERAL;
            }

            Core::hresult result = pPersistentStore->DeleteKey(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_MIGRATION_VERSION);
            pPersistentStore->Release();

            // Treat "key not present" as success — idempotent delete.
            if (missingFromPersistentStore(result)) {
                result = Core::ERROR_NONE;
            } else if (Core::ERROR_NONE != result) {
                LOGERR("deleteMigrationVersion: failed, hresult=%d", result);
            }
            return result;
        }

        // ---------------------------------------------------------------------------
        // Migration step helpers
        // ---------------------------------------------------------------------------

        Core::hresult BluetoothDeviceManager::clearBluetoothStoreData()
        {
            if (_service == nullptr) {
                LOGERR("clearBluetoothStoreData: service is null");
                return Core::ERROR_GENERAL;
            }

            Exchange::IStore* pPersistentStore = _service->QueryInterfaceByCallsign<Exchange::IStore>(PERSISTENT_STORE_CALLSIGN);
            if (pPersistentStore == nullptr) {
                LOGERR("clearBluetoothStoreData: failed to get PersistentStore interface");
                return Core::ERROR_GENERAL;
            }

            Core::hresult result = pPersistentStore->DeleteKey(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_DEVICE_INFO);
            pPersistentStore->Release();

            // Treat "key not present" as success — idempotent delete.
            if (missingFromPersistentStore(result)) {
                result = Core::ERROR_NONE;
            } else if (Core::ERROR_NONE != result) {
                LOGERR("clearBluetoothStoreData: failed to delete deviceInfo, hresult=%d", result);
            }
            return result;
        }

        Core::hresult BluetoothDeviceManager::importFromAS()
        {
#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
            BluetoothPersistenceAdapter adapter;
            std::vector<BluetoothDeviceInfo> importedDevices;
            const Core::hresult result = adapter.Read(importedDevices);

            if (Core::ERROR_NOT_EXIST == result) {
                LOGINFO("importFromAS: AS file not found; treating as empty payload");
                // Cache is already clear — enrichCacheFromBTRMGR will populate from BTRMGR.
                return Core::ERROR_NONE;
            }
            if (Core::ERROR_NONE != result) {
                LOGERR("importFromAS: failed to read AS file, hresult=%d", result);
                return result;
            }

            // Stage by deviceAddr (we don't have device handles yet).
            _adminLock.Lock();
            _pairedDeviceCache.clear();
            for (auto& info : importedDevices) {
                if (!info.deviceAddr.empty()) {
                    _pairedDeviceCache[info.deviceAddr] = std::move(info);
                } else {
                    LOGWARN("importFromAS: skipping entry with empty deviceAddr");
                }
            }
            _adminLock.Unlock();

            LOGINFO("importFromAS: staged %zu entries keyed by deviceAddr", importedDevices.size());
            return Core::ERROR_NONE;
#else
            // No AS support compiled in; treat as empty.
            return Core::ERROR_NONE;
#endif
        }

        Core::hresult BluetoothDeviceManager::enrichCacheFromBTRMGR()
        {
            BTRMGR_PairedDevicesList_t pairedDevices{};

            if (BTRMGR_GetPairedDevices(0, &pairedDevices) != BTRMGR_RESULT_SUCCESS) {
                LOGERR("enrichCacheFromBTRMGR: failed to get paired devices from BTRMGR");
                return Core::ERROR_GENERAL;
            }

            // Re-key the cache from deviceAddr (AS stage) to deviceHandle string,
            // backfilling BTRMGR-sourced fields.
            _adminLock.Lock();

            std::unordered_map<std::string, BluetoothDeviceInfo> enrichedCache;

            for (int i = 0; i < pairedDevices.m_numOfDevices; ++i) {
                const std::string deviceId = std::to_string(pairedDevices.m_deviceProperty[i].m_deviceHandle);
                const std::string deviceAddr = (pairedDevices.m_deviceProperty[i].m_deviceAddress[0] != '\0')
                    ? std::string(pairedDevices.m_deviceProperty[i].m_deviceAddress)
                    : std::string();
                const char* typeStr = BTRMGR_GetDeviceTypeAsString(pairedDevices.m_deviceProperty[i].m_deviceType);
                const std::string deviceType = typeStr ? typeStr : "UNKNOWN";
                const std::string friendlyName = (pairedDevices.m_deviceProperty[i].m_name[0] != '\0')
                    ? std::string(pairedDevices.m_deviceProperty[i].m_name)
                    : deviceId;

                // Pull any AS-imported data staged under deviceAddr.
                BluetoothDeviceInfo info;
                if (!deviceAddr.empty()) {
                    auto it = _pairedDeviceCache.find(deviceAddr);
                    if (it != _pairedDeviceCache.end()) {
                        info = std::move(it->second);
                        LOGINFO("enrichCacheFromBTRMGR: merged AS data for deviceID=%s (addr=%s)",
                                deviceId.c_str(), deviceAddr.c_str());
                    }
                }

                // Overwrite/set BTRMGR-authoritative fields.
                info.deviceAddr   = deviceAddr;
                info.deviceType   = deviceType;
                info.friendlyName = friendlyName;

                enrichedCache[deviceId] = std::move(info);
            }

            _pairedDeviceCache = std::move(enrichedCache);
            _adminLock.Unlock();

            LOGINFO("enrichCacheFromBTRMGR: enriched cache has %d entries", pairedDevices.m_numOfDevices);
            return Core::ERROR_NONE;
        }

        // ---------------------------------------------------------------------------
        // AS filesystem persistence helpers (only under migration compile flag)
        // ---------------------------------------------------------------------------

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION

        Core::hresult BluetoothDeviceManager::writeCacheFromFilesystemPersistence()
        {
            BluetoothPersistenceAdapter adapter;

            std::vector<BluetoothDeviceInfo> importedDevices;
            const Core::hresult result = adapter.Read(importedDevices);
            if (Core::ERROR_NONE != result) {
                return result;
            }

            // Build a mapping from device address to device handle using BTRMGR.
            BTRMGR_PairedDevicesList_t pairedDevices{};
            if (BTRMGR_GetPairedDevices(0, &pairedDevices) != BTRMGR_RESULT_SUCCESS) {
                LOGERR("Failed to get paired devices from BTRMGR during filesystem persistence import");
                return Core::ERROR_GENERAL;
            }

            std::unordered_map<std::string, std::string> addrToDeviceId;
            addrToDeviceId.reserve(static_cast<size_t>(pairedDevices.m_numOfDevices));
            for (int i = 0; i < pairedDevices.m_numOfDevices; ++i) {
                if (pairedDevices.m_deviceProperty[i].m_deviceAddress[0] != '\0') {
                    const std::string deviceAddr(pairedDevices.m_deviceProperty[i].m_deviceAddress);
                    const std::string deviceId = std::to_string(pairedDevices.m_deviceProperty[i].m_deviceHandle);
                    addrToDeviceId[deviceAddr] = std::move(deviceId);
                }
            }

            std::unordered_map<std::string, BluetoothDeviceInfo> importedCache;
            for (BluetoothDeviceInfo& info : importedDevices) {
                auto it = addrToDeviceId.find(info.deviceAddr);
                if (it != addrToDeviceId.end()) {
                    importedCache[it->second] = std::move(info);
                } else {
                    LOGWARN("No paired device handle found for addr=%s during filesystem persistence import, skipping", info.deviceAddr.c_str());
                }
            }

            _adminLock.Lock();
            _pairedDeviceCache = std::move(importedCache);
            _adminLock.Unlock();

            return Core::ERROR_NONE;
        }

        void BluetoothDeviceManager::writeFilesystemPersistenceFromCache()
        {
            BluetoothPersistenceAdapter adapter;
            std::unordered_map<std::string, BluetoothDeviceInfo> cacheSnapshot = getPairedDeviceInfos();

            // Filter out Human Interface Devices — The legacy behavior doesn't persist them to the filesystem.
            for (auto it = cacheSnapshot.begin(); it != cacheSnapshot.end(); ) {
                if (it->second.deviceType == "HUMAN INTERFACE DEVICE") {
                    it = cacheSnapshot.erase(it);
                } else {
                    ++it;
                }
            }

            // Build a deterministic canonical string from the snapshot for change detection.
            std::vector<std::string> keys;
            keys.reserve(cacheSnapshot.size());
            for (const auto& entry : cacheSnapshot) {
                keys.push_back(entry.first);
            }
            std::sort(keys.begin(), keys.end());

            std::string canonical;
            for (const auto& key : keys) {
                const BluetoothDeviceInfo& info = cacheSnapshot.at(key);
                canonical += key;
                canonical += '|';
                canonical += info.deviceAddr;
                canonical += '|';
                canonical += info.deviceType;
                canonical += '|';
                canonical += info.friendlyName;
                canonical += '|';
                canonical += std::to_string(info.lastVolumeSetting);
                canonical += '|';
                canonical += std::to_string(static_cast<int>(info.autoConnectStatus));
                canonical += '|';
                canonical += info.lastConnectTimeUtc;
                canonical += ';';
            }

            const std::size_t newHash = std::hash<std::string>{}(canonical);

            _adminLock.Lock();
            const bool unchanged = (newHash == _lastFilesystemPersistenceHash);
            _adminLock.Unlock();

            if (unchanged) {
                LOGINFO("Filesystem persistence sync skipped: cache unchanged since last write, cache_size=%zu", cacheSnapshot.size());
                return;
            }

            const Core::hresult result = adapter.Write(cacheSnapshot);
            if (Core::ERROR_NONE != result) {
                LOGERR("Filesystem persistence sync failed: Unable to update persistence payload from cache, hresult=%d cache_size=%zu", result, cacheSnapshot.size());
            } else {
                _adminLock.Lock();
                _lastFilesystemPersistenceHash = newHash;
                _adminLock.Unlock();
                LOGINFO("Filesystem persistence sync succeeded: Persistence payload updated from cache, cache_size=%zu", cacheSnapshot.size());
            }
        }

#endif // BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION

        // ---------------------------------------------------------------------------
        // RDK PersistentStore read / write helpers
        // ---------------------------------------------------------------------------

        Core::hresult BluetoothDeviceManager::updateCacheFromStorage()
        {
            if (_service == nullptr) {
                LOGERR("Service is null\n");
                return Core::ERROR_GENERAL;
            }

            Exchange::IStore* pPersistentStore = _service->QueryInterfaceByCallsign<Exchange::IStore>(PERSISTENT_STORE_CALLSIGN);
            if (pPersistentStore == nullptr) {
                LOGERR("Failed to get PersistentStore interface\n");
                return Core::ERROR_GENERAL;
            }

            string bluetoothDeviceInfoStr;
            Core::hresult result = pPersistentStore->GetValue(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_DEVICE_INFO, bluetoothDeviceInfoStr);
            pPersistentStore->Release();

            if (Core::ERROR_NONE == result) {
                LOGINFO("Loaded device info JSON: %s\n", bluetoothDeviceInfoStr.c_str());
                JsonArray deviceInfoArray;
                deviceInfoArray.FromString(bluetoothDeviceInfoStr);

                _adminLock.Lock();

                _pairedDeviceCache.clear();

                for (uint16_t i = 0; i < deviceInfoArray.Length(); i++) {
                    JsonObject deviceInfoObj = deviceInfoArray[i].Object();
                    std::string deviceID = deviceInfoObj["deviceID"].String();
                    std::string deviceType = deviceInfoObj["deviceType"].String();

                    AutoConnectStatus autoConnectStatus = AUTO_CONNECT_STATUS_UNSET;
                    if (deviceInfoObj.HasLabel("autoconnect")) {
                        autoConnectStatus = static_cast<AutoConnectStatus>(deviceInfoObj["autoconnect"].Number());
                    }

                    std::string lastConnectTimeUtc = deviceInfoObj.HasLabel("lastConnectTimeUtc") ? deviceInfoObj["lastConnectTimeUtc"].String() : "";

                    long long lastVolumeSetting = 0;
                    if (deviceInfoObj.HasLabel("lastVolumeSetting")) {
                        lastVolumeSetting = static_cast<long long>(deviceInfoObj["lastVolumeSetting"].Number());
                    }

                    BluetoothDeviceInfo deviceInfo;
                    deviceInfo.deviceType = std::move(deviceType);
                    deviceInfo.autoConnectStatus = autoConnectStatus;
                    deviceInfo.lastConnectTimeUtc = std::move(lastConnectTimeUtc);
                    deviceInfo.lastVolumeSetting = lastVolumeSetting;

                    _pairedDeviceCache[deviceID] = std::move(deviceInfo);

                    LOGINFO("Loaded device info for deviceID=%s, autoConnectStatus=%d, lastConnectTimeUtc=%s, lastVolumeSetting=%lld\n",
                            deviceID.c_str(),
                            static_cast<int>(_pairedDeviceCache[deviceID].autoConnectStatus),
                            _pairedDeviceCache[deviceID].lastConnectTimeUtc.c_str(),
                            _pairedDeviceCache[deviceID].lastVolumeSetting);
                }

                _adminLock.Unlock();

            } else if (!missingFromPersistentStore(result)) {
                LOGERR("Failed to load device info from PersistentStore, hresult=%d\n", result);
            }

            return result;
        }

        Core::hresult BluetoothDeviceManager::writeDeviceInfoToStore()
        {
            if (_service == nullptr) {
                LOGERR("writeDeviceInfoToStore: service is null");
                return Core::ERROR_GENERAL;
            }

            Exchange::IStore* pPersistentStore = _service->QueryInterfaceByCallsign<Exchange::IStore>(PERSISTENT_STORE_CALLSIGN);
            if (pPersistentStore == nullptr) {
                LOGERR("writeDeviceInfoToStore: failed to get PersistentStore interface");
                return Core::ERROR_GENERAL;
            }

            JsonArray deviceInfoArray;

            _adminLock.Lock();

            for (const auto& entry : _pairedDeviceCache) {
                const std::string& deviceID = entry.first;
                const BluetoothDeviceInfo& deviceInfo = entry.second;

                JsonObject deviceInfoObj;
                deviceInfoObj["deviceID"] = deviceID;
                deviceInfoObj["deviceType"] = deviceInfo.deviceType;
                deviceInfoObj["autoconnect"] = static_cast<int>(deviceInfo.autoConnectStatus);
                deviceInfoObj["lastConnectTimeUtc"] = deviceInfo.lastConnectTimeUtc;
                deviceInfoObj["lastVolumeSetting"] = deviceInfo.lastVolumeSetting;

                deviceInfoArray.Add(deviceInfoObj);
            }

            string bluetoothDeviceInfoStr;
            deviceInfoArray.ToString(bluetoothDeviceInfoStr);

            _adminLock.Unlock();

            LOGINFO("writeDeviceInfoToStore: Saving device info JSON: %s", bluetoothDeviceInfoStr.c_str());

            Core::hresult result = pPersistentStore->SetValue(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_DEVICE_INFO, bluetoothDeviceInfoStr);
            pPersistentStore->Release();

            if (Core::ERROR_NONE != result) {
                LOGERR("writeDeviceInfoToStore: failed, hresult=%d", result);
            }
            return result;
        }

        Core::hresult BluetoothDeviceManager::writeStorageFromCache()
        {
            // Pre-migration: no ownership of store yet.
            if (!_isMigrated) {
                LOGINFO("writeStorageFromCache: pre-migration, skipping store write");
                return Core::ERROR_NONE;
            }

            const Core::hresult result = writeDeviceInfoToStore();
            if (Core::ERROR_NONE != result) {
                LOGERR("writeStorageFromCache: writeDeviceInfoToStore failed, hresult=%d", result);
                return result;
            }

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
            writeFilesystemPersistenceFromCache();
#endif

            return Core::ERROR_NONE;
        }

        // ---------------------------------------------------------------------------
        // Lifecycle
        // ---------------------------------------------------------------------------

        Core::hresult BluetoothDeviceManager::init(PluginHost::IShell* service)
        {
            LOGINFO("BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION is %s",
#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
                "enabled"
#else
                "disabled"
#endif
            );

            LOGINFO("BLUETOOTH_PERSISTENT_FILE_PATH is '%s'",
#ifdef BLUETOOTH_PERSISTENT_FILE_PATH
                BLUETOOTH_PERSISTENT_FILE_PATH
#else
                "unavailable"
#endif
            );

            if (service == nullptr) {
                return Core::ERROR_GENERAL;
            }

            _service = service;
            _service->AddRef();

            // Migration truth: check for explicit marker.
            std::string version;
            const Core::hresult versionResult = readMigrationVersion(version);

            if (Core::ERROR_NONE == versionResult && version == BLUETOOTH_MIGRATION_VERSION) {
                LOGINFO("init: valid migration marker found (version=%s); loading from RDK store", version.c_str());
                const Core::hresult storageResult = updateCacheFromStorage();
                if (Core::ERROR_NONE != storageResult && !missingFromPersistentStore(storageResult)) {
                    LOGERR("init: RDK store read failed (hresult=%d); aborting init", storageResult);
                    return storageResult;
                }
                _isMigrated = true;
            } else {
                LOGINFO("init: migration marker absent or invalid (hresult=%d, version='%s'); not migrated — ignoring any stale store data",
                        versionResult, version.c_str());
                _adminLock.Lock();
                _pairedDeviceCache.clear();
                _adminLock.Unlock();
                _isMigrated = false;
            }

            return Core::ERROR_NONE;
        }

        void BluetoothDeviceManager::deinit()
        {
            if (_service != nullptr) {
                _service->Release();
                _service = nullptr;
            }
        }

        // ---------------------------------------------------------------------------
        // Migration public API
        // ---------------------------------------------------------------------------

        Core::hresult BluetoothDeviceManager::performMigration()
        {
            // Check if already migrated.
            std::string version;
            const Core::hresult versionResult = readMigrationVersion(version);
            if (Core::ERROR_NONE == versionResult && version == BLUETOOTH_MIGRATION_VERSION) {
                LOGINFO("performMigration: already migrated (version=%s), no-op", version.c_str());
                return Core::ERROR_NONE;
            }

            LOGINFO("performMigration: starting migration");

            // 1. Clear stale Bluetooth store data.
            Core::hresult result = clearBluetoothStoreData();
            if (Core::ERROR_NONE != result) {
                LOGERR("performMigration: failed to clear stale store data, hresult=%d", result);
                return result;
            }

            // 2. Clear cache before AS import.
            {
                _adminLock.Lock();
                _pairedDeviceCache.clear();
                _adminLock.Unlock();
            }

            // 3. Import AS storage (treat missing as empty payload).
            result = importFromAS();
            if (Core::ERROR_NONE != result) {
                LOGERR("performMigration: AS import failed, hresult=%d", result);
                return result;
            }

            // 4. Mandatory enrichment from BTRMGR — re-keys cache and backfills fields.
            result = enrichCacheFromBTRMGR();
            if (Core::ERROR_NONE != result) {
                LOGERR("performMigration: BTRMGR enrichment failed, hresult=%d", result);
                return result;
            }

            // 5. Write deviceInfo to RDK store (FIRST — before marker).
            result = writeDeviceInfoToStore();
            if (Core::ERROR_NONE != result) {
                LOGERR("performMigration: failed to write device info to store, hresult=%d", result);
                return result;
            }

            // 6. Write migration marker (LAST).
            result = writeMigrationVersion();
            if (Core::ERROR_NONE != result) {
                LOGERR("performMigration: failed to write migration version, hresult=%d", result);
                return result;
            }

            _isMigrated = true;
            LOGINFO("performMigration: migration complete");
            return Core::ERROR_NONE;
        }

        Core::hresult BluetoothDeviceManager::clearMigration()
        {
            LOGINFO("clearMigration: clearing RDK store data and migration marker; AS preserved");

            // 1. Delete deviceInfo from RDK store.
            Core::hresult result = clearBluetoothStoreData();
            if (Core::ERROR_NONE != result) {
                LOGERR("clearMigration: failed to clear store data, hresult=%d", result);
                return result;
            }

            // 2. Delete migration marker.
            result = deleteMigrationVersion();
            if (Core::ERROR_NONE != result) {
                LOGERR("clearMigration: failed to delete migration version, hresult=%d", result);
                return result;
            }

            // 3. Clear RAM cache.
            _adminLock.Lock();
            _pairedDeviceCache.clear();
            _adminLock.Unlock();

            _isMigrated = false;
            LOGINFO("clearMigration: complete");
            return Core::ERROR_NONE;
        }

        bool BluetoothDeviceManager::isMigrated() const
        {
            return _isMigrated;
        }

        // ---------------------------------------------------------------------------
        // Internal cache lookup
        // ---------------------------------------------------------------------------

        Core::hresult BluetoothDeviceManager::getPairedDeviceInfo(const std::string& deviceID, BluetoothDeviceInfo& deviceInfo)
        {
            auto it = _pairedDeviceCache.find(deviceID);
            const bool bFound = (it != _pairedDeviceCache.end());

            if (bFound) {
                deviceInfo = it->second;
            }

            return bFound ? Core::ERROR_NONE : Core::ERROR_NOT_EXIST;
        }

        // ---------------------------------------------------------------------------
        // Device persistence API (with pre-migration guards)
        // ---------------------------------------------------------------------------

        Core::hresult BluetoothDeviceManager::setAutoConnect(const std::string& deviceID, bool enable)
        {
            LOGINFO("deviceID=%s, enable=%s\n", deviceID.c_str(), enable ? "true" : "false");

            if (!_isMigrated) {
                LOGERR("setAutoConnect: rejected — migration not complete");
                return Core::ERROR_ILLEGAL_STATE;
            }

            BluetoothDeviceInfo deviceInfo;

            _adminLock.Lock();

            Core::hresult result = getPairedDeviceInfo(deviceID, deviceInfo);

            if (Core::ERROR_NONE != result) {
                LOGERR("Device info is not found in cache for deviceID: %s", deviceID.c_str());
                _adminLock.Unlock();
                return Core::ERROR_NOT_EXIST;
            }

            deviceInfo.autoConnectStatus = enable ? AUTO_CONNECT_STATUS_ENABLED : AUTO_CONNECT_STATUS_DISABLED;
            _pairedDeviceCache[deviceID] = std::move(deviceInfo);
            _adminLock.Unlock();

            result = writeStorageFromCache();
            if (Core::ERROR_NONE != result) {
                LOGERR("Failed to update storage from cache after setting autoConnect for deviceID=%s", deviceID.c_str());
            }

            return result;
        }

        Core::hresult BluetoothDeviceManager::getAutoConnect(const std::string& deviceID, AutoConnectStatus& status)
        {
            LOGINFO("deviceID=%s\n", deviceID.c_str());

            if (!_isMigrated) {
                // Pre-migration: report disabled gracefully.
                status = AUTO_CONNECT_STATUS_DISABLED;
                return Core::ERROR_NONE;
            }

            BluetoothDeviceInfo deviceInfo;

            _adminLock.Lock();

            Core::hresult result = getPairedDeviceInfo(deviceID, deviceInfo);

            _adminLock.Unlock();

            if (Core::ERROR_NONE == result) {
                status = deviceInfo.autoConnectStatus;
            }

            return result;
        }

        void BluetoothDeviceManager::setLastConnectTimeUtc(const std::string& deviceID)
        {
            if (!_isMigrated) {
                return; // pre-migration no-op
            }

            BluetoothDeviceInfo deviceInfo;
            _adminLock.Lock();
            Core::hresult result = getPairedDeviceInfo(deviceID, deviceInfo);
            _adminLock.Unlock();

            if (Core::ERROR_NONE != result) {
                LOGERR("Device info is not found in cache for deviceID: %s", deviceID.c_str());
                return;
            }

            auto now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            const std::string currentUtcTime = std::to_string(static_cast<long long>(now_c));

            LOGINFO("deviceID=%s, time=%s\n", deviceID.c_str(), currentUtcTime.c_str());

            deviceInfo.lastConnectTimeUtc = std::move(currentUtcTime);

            _adminLock.Lock();
            _pairedDeviceCache[deviceID] = std::move(deviceInfo);
            _adminLock.Unlock();

            result = writeStorageFromCache();
            if (Core::ERROR_NONE != result) {
                LOGERR("Failed to update storage from cache after setting lastConnectTimeUtc for deviceID=%s", deviceID.c_str());
            }
        }

        Core::hresult BluetoothDeviceManager::setLastVolumeSetting(const std::string& deviceID, long long volumeSetting)
        {
            LOGINFO("deviceID=%s, volumeSetting=%lld", deviceID.c_str(), volumeSetting);

            if (!_isMigrated) {
                return Core::ERROR_NONE; // pre-migration no-op
            }

            BluetoothDeviceInfo deviceInfo;

            _adminLock.Lock();

            Core::hresult result = getPairedDeviceInfo(deviceID, deviceInfo);

            if (Core::ERROR_NONE != result) {
                LOGERR("Device info is not found in cache for deviceID: %s", deviceID.c_str());
                _adminLock.Unlock();
                return Core::ERROR_NOT_EXIST;
            }

            deviceInfo.lastVolumeSetting = volumeSetting;
            _pairedDeviceCache[deviceID] = std::move(deviceInfo);
            _adminLock.Unlock();

            result = writeStorageFromCache();
            if (Core::ERROR_NONE != result) {
                LOGERR("Failed to update storage from cache after setting lastVolumeSetting for deviceID=%s", deviceID.c_str());
            }

            return result;
        }

        Core::hresult BluetoothDeviceManager::getLastConnectTimeUtc(const std::string& deviceID, std::string& lastConnectTimeUtc)
        {
            LOGINFO("deviceID=%s\n", deviceID.c_str());
            BluetoothDeviceInfo deviceInfo;

            _adminLock.Lock();

            Core::hresult result = getPairedDeviceInfo(deviceID, deviceInfo);

            _adminLock.Unlock();

            if (Core::ERROR_NONE == result) {
                lastConnectTimeUtc = deviceInfo.lastConnectTimeUtc;
            }

            return result;
        }

        Core::hresult BluetoothDeviceManager::addDevice(const std::string& deviceID)
        {
            LOGINFO("deviceID=%s\n", deviceID.c_str());

            if (!_isMigrated) {
                // Pre-migration: the pairing itself succeeds but we don't own persistence yet.
                LOGINFO("addDevice: pre-migration, skipping persistence for deviceID=%s", deviceID.c_str());
                return Core::ERROR_NONE;
            }

            BTRMgrDeviceHandle deviceHandle;

            try {
                deviceHandle = (BTRMgrDeviceHandle) stoll(deviceID);
            } catch (const std::exception& e) {
                LOGERR("Failed to parse deviceId: %s\n", e.what());
                return Core::ERROR_INVALID_PARAMETER;
            }

            BTRMGR_DevicesProperty_t deviceProperty{};

            BTRMGR_Result_t result = BTRMGR_GetDeviceProperties(0, deviceHandle, &deviceProperty);
            if (BTRMGR_RESULT_SUCCESS != result) {
                LOGERR("Failed to get device properties for deviceID: %s", deviceID.c_str());
                return Core::ERROR_NOT_EXIST;
            }

            _adminLock.Lock();

            BluetoothDeviceInfo deviceInfo;
            deviceInfo.deviceAddr = (deviceProperty.m_deviceAddress[0] != '\0') ? std::string(deviceProperty.m_deviceAddress) : std::string();
            const char* deviceTypeStr = BTRMGR_GetDeviceTypeAsString(deviceProperty.m_deviceType);
            deviceInfo.deviceType = (deviceTypeStr != nullptr) ? deviceTypeStr : "UNKNOWN";
            deviceInfo.friendlyName = (deviceProperty.m_name[0] != '\0') ? std::string(deviceProperty.m_name) : deviceID;
            _pairedDeviceCache[deviceID] = std::move(deviceInfo);

            _adminLock.Unlock();

            return writeStorageFromCache();
        }

        Core::hresult BluetoothDeviceManager::removeDevice(const std::string& deviceID)
        {
            LOGINFO("deviceID=%s\n", deviceID.c_str());

            if (!_isMigrated) {
                // Pre-migration: the unpairing itself succeeds but we don't own persistence yet.
                LOGINFO("removeDevice: pre-migration, skipping persistence for deviceID=%s", deviceID.c_str());
                return Core::ERROR_NONE;
            }

            _adminLock.Lock();

            auto it = _pairedDeviceCache.find(deviceID);
            if (it != _pairedDeviceCache.end()) {
                _pairedDeviceCache.erase(it);
            } else {
                LOGWARN("Device info is not found in cache for deviceID: %s", deviceID.c_str());
                _adminLock.Unlock();
                return Core::ERROR_NOT_EXIST;
            }

            _adminLock.Unlock();
            return writeStorageFromCache();
        }

        std::unordered_map<std::string /* deviceID */, BluetoothDeviceInfo /* deviceInfo */> BluetoothDeviceManager::getPairedDeviceInfos()
        {
            _adminLock.Lock();

            std::unordered_map<std::string /* deviceID */, BluetoothDeviceInfo /* deviceInfo */> deviceInfos;

            try {
                deviceInfos = _pairedDeviceCache;
            } catch (...) {
                LOGERR("Failed to copy paired device infos\n");
            }

            _adminLock.Unlock();
            return deviceInfos;
        }

    } // Plugin
} // WPEFramework
