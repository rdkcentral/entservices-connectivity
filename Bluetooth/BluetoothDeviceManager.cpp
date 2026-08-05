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
#include "BtAdapter.h"
#include "IBtAdapter.h"
#include "DeviceRegistry.h"

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

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION

        Core::hresult BluetoothDeviceManager::writeCacheFromFilesystemPersistence(const std::string& rawContent)
        {
            if (rawContent.empty()) {
                _adminLock.Lock();
                _pairedDeviceCache.clear();
                _adminLock.Unlock();
                return Core::ERROR_NONE;
            }

            BluetoothPersistenceAdapter adapter;
            std::vector<BluetoothDeviceInfo> importedDevices;
            const Core::hresult result = adapter.Parse(rawContent, importedDevices);
            if (Core::ERROR_NONE != result) {
                return result;
            }

            // Build a mapping from device address to device handle using the SDK.
            if (!_btAdapter) {
                LOGERR("BtAdapter not set during filesystem persistence import");
                return Core::ERROR_GENERAL;
            }
            auto sdkPairedDevices = _btAdapter->getPairedDevices();

            std::unordered_map<std::string, std::string> addrToDeviceId;
            addrToDeviceId.reserve(sdkPairedDevices.size());
            for (const auto& info : sdkPairedDevices) {
                if (!info.mac.empty()) {
                    addrToDeviceId[info.mac] = info.handleStr;
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

            const Core::hresult result = adapter.Write(cacheSnapshot);
            if (Core::ERROR_NONE != result) {
                LOGERR("Filesystem persistence sync failed: Unable to update persistence payload from cache, hresult=%d cache_size=%zu", result, cacheSnapshot.size());
            } else {
                LOGINFO("Filesystem persistence sync succeeded: Persistence payload updated from cache, cache_size=%zu", cacheSnapshot.size());
            }
        }

        Core::hresult BluetoothDeviceManager::readMigrationVersionFromStorage(std::string& version) const
        {
            if (_service == nullptr) {
                LOGERR("Service is null");
                return Core::ERROR_GENERAL;
            }

            Exchange::IStore* pPersistentStore = _service->QueryInterfaceByCallsign<Exchange::IStore>(PERSISTENT_STORE_CALLSIGN);
            if (pPersistentStore == nullptr) {
                LOGERR("Failed to get PersistentStore interface");
                return Core::ERROR_GENERAL;
            }

            const Core::hresult result = pPersistentStore->GetValue(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_MIGRATION_VERSION, version);
            pPersistentStore->Release();

            if ((Core::ERROR_NONE != result) && !missingFromPersistentStore(result)) {
                LOGERR("Failed to read migrationVersion from PersistentStore, hresult=%d", result);
            }

            return result;
        }

        Core::hresult BluetoothDeviceManager::writeMigrationVersionToStorage()
        {
            if (_service == nullptr) {
                LOGERR("Service is null");
                return Core::ERROR_GENERAL;
            }

            Exchange::IStore* pPersistentStore = _service->QueryInterfaceByCallsign<Exchange::IStore>(PERSISTENT_STORE_CALLSIGN);
            if (pPersistentStore == nullptr) {
                LOGERR("Failed to get PersistentStore interface");
                return Core::ERROR_GENERAL;
            }

            const Core::hresult result = pPersistentStore->SetValue(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_MIGRATION_VERSION, BLUETOOTH_MIGRATION_VERSION);
            pPersistentStore->Release();

            if (Core::ERROR_NONE != result) {
                LOGERR("Failed to write migrationVersion to PersistentStore, hresult=%d", result);
            }

            return result;
        }

        Core::hresult BluetoothDeviceManager::performMigration()
        {
            Core::SafeSyncType<Core::CriticalSection> lock(_migrationLock);

            LOGINFO("migration_attempted");

            std::string storedVersion;
            const Core::hresult readVersionResult = readMigrationVersionFromStorage(storedVersion);
            const bool alreadyMigrated = (Core::ERROR_NONE == readVersionResult) && (storedVersion == BLUETOOTH_MIGRATION_VERSION);

            if (!missingFromPersistentStore(readVersionResult) && (Core::ERROR_NONE != readVersionResult)) {
                LOGERR("failed to read migrationVersion, hresult=%d", readVersionResult);
                return readVersionResult;
            }

            if (alreadyMigrated) {
                LOGINFO("migration already completed (migrationVersion=%s present), no sync needed", BLUETOOTH_MIGRATION_VERSION);
                _isMigrated.store(true);
                return Core::ERROR_NONE;
            }

            // Step 1: Read the AS file and import devices into the cache.
            // NOTE: Do NOT delete existing deviceInfo from PersistentStore before this point.
            // If ReadRaw()/Parse() fails, the existing store data must be preserved to avoid data
            // loss. Any stale deviceInfo written by a prior partial run is safely overwritten by
            // writeStorageFromCache() (SetValue) only after all steps below succeed.
            BluetoothPersistenceAdapter adapter;
            std::string rawContent;
            const Core::hresult readRawResult = adapter.ReadRaw(rawContent);
            if ((Core::ERROR_NONE != readRawResult) && (Core::ERROR_NOT_EXIST != readRawResult)) {
                LOGERR("failed to read AS filesystem persistence source, hresult=%d", readRawResult);
                return readRawResult;
            }

            const Core::hresult importResult = writeCacheFromFilesystemPersistence(rawContent);
            if (Core::ERROR_NONE != importResult) {
                LOGERR("failed to import from filesystem persistence, hresult=%d", importResult);
                return importResult;
            }

            // Step 2: Mandatory BTRMGR enrichment. AS lacks some fields (e.g. deviceType) required
            // by the RDK store schema; enrichment is a hard requirement for migration correctness.
            // Skip when the imported cache is empty — there is nothing to enrich, and an error from
            // BTRMGR_GetPairedDevices() should not abort migration in that case.
            _adminLock.Lock();
            const bool importedCacheEmpty = _pairedDeviceCache.empty();
            _adminLock.Unlock();

            if (!importedCacheEmpty) {
                const Core::hresult deviceResult = updateCacheFromDevice(/* backfillOnly= */ true);
                if (Core::ERROR_NONE != deviceResult) {
                    LOGERR("mandatory BTRMGR enrichment failed (hresult=%d); aborting migration", deviceResult);
                    return deviceResult;
                }
            } else {
                LOGINFO("imported cache is empty, skipping BTRMGR enrichment");
            }

            // Step 3: Write enriched deviceInfo to RDK Persistent Store (before migration marker).
            const Core::hresult writeResult = writeStorageFromCache();
            if (Core::ERROR_NONE != writeResult) {
                LOGERR("failed to persist imported data to PersistentStore, hresult=%d", writeResult);
                return writeResult;
            }

            // Step 4: Write migration marker last so it cannot indicate success unless store is written.
            const Core::hresult writeVersionResult = writeMigrationVersionToStorage();
            if (Core::ERROR_NONE != writeVersionResult) {
                LOGERR("failed to persist migrationVersion, hresult=%d", writeVersionResult);
                return writeVersionResult;
            }

            // Step 5: Transfer AS ownership, set migrated true.
            _isMigrated.store(true);

            LOGINFO("initial migration succeeded");
            return Core::ERROR_NONE;
        }

        Core::hresult BluetoothDeviceManager::clearMigration()
        {
            Core::SafeSyncType<Core::CriticalSection> lock(_migrationLock);

            if (_service == nullptr) {
                LOGERR("service is null");
                return Core::ERROR_GENERAL;
            }

            Exchange::IStore* pPersistentStore = _service->QueryInterfaceByCallsign<Exchange::IStore>(PERSISTENT_STORE_CALLSIGN);
            if (pPersistentStore == nullptr) {
                LOGERR("failed to get PersistentStore interface");
                return Core::ERROR_GENERAL;
            }

            Core::hresult result = pPersistentStore->DeleteKey(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_DEVICE_INFO);
            if ((Core::ERROR_NONE != result) && !missingFromPersistentStore(result)) {
                LOGERR("failed to delete deviceInfo from PersistentStore, hresult=%d", result);
                pPersistentStore->Release();
                return result;
            }

            result = pPersistentStore->DeleteKey(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_MIGRATION_VERSION);
            if ((Core::ERROR_NONE != result) && !missingFromPersistentStore(result)) {
                LOGERR("failed to delete migrationVersion from PersistentStore, hresult=%d", result);
                pPersistentStore->Release();
                return result;
            }

            pPersistentStore->Release();

            _adminLock.Lock();
            _pairedDeviceCache.clear();
            _adminLock.Unlock();

            _isMigrated.store(false);

            LOGINFO("PersistentStore cleared and migration state reset");
            return Core::ERROR_NONE;
        }
#endif

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

        Core::hresult BluetoothDeviceManager::updateCacheFromDevice(bool backfillOnly)
        {
            if (!_btAdapter) {
                LOGERR("BtAdapter not set in updateCacheFromDevice");
                return Core::ERROR_GENERAL;
            }

            auto sdkPairedDevices = _btAdapter->getPairedDevices();

            _adminLock.Lock();

            for (const auto& info : sdkPairedDevices) {
                string deviceId   = info.handleStr;
                string deviceType = info.deviceType;
                string deviceAddr = info.mac;
                string name       = info.name;
                if (name.empty()) name = deviceId;

                if (_pairedDeviceCache.find(deviceId) != _pairedDeviceCache.end()) {
                    BluetoothDeviceInfo& existing = _pairedDeviceCache[deviceId];
                    if (existing.friendlyName.empty()) {
                        existing.friendlyName = name;
                        LOGINFO("Backfilled friendlyName for deviceID=%s\n", deviceId.c_str());
                    }
                    if (existing.deviceAddr.empty()) {
                        existing.deviceAddr = deviceAddr;
                        LOGINFO("Backfilled deviceAddr for deviceID=%s: %s\n", deviceId.c_str(), deviceAddr.c_str());
                    }
                    if (existing.deviceType.empty() || existing.deviceType == "UNKNOWN") {
                        existing.deviceType = deviceType;
                        LOGINFO("Backfilled deviceType for deviceID=%s: %s\n", deviceId.c_str(), deviceType.c_str());
                    }
                } else if (!backfillOnly) {
                    LOGINFO("Adding device to cache: deviceID=%s, deviceType=%s\n", deviceId.c_str(), deviceType.c_str());
                    BluetoothDeviceInfo deviceInfo;
                    deviceInfo.deviceAddr   = deviceAddr;
                    deviceInfo.deviceType   = std::move(deviceType);
                    deviceInfo.friendlyName = std::move(name);
                    _pairedDeviceCache[deviceId] = std::move(deviceInfo);
                } else {
                    LOGINFO("Skipping device not in imported cache (backfill-only mode): deviceID=%s\n", deviceId.c_str());
                }
            }

            if (!backfillOnly) {
                std::unordered_set<std::string> pairedDeviceIds;
                pairedDeviceIds.reserve(sdkPairedDevices.size());
                for (const auto& info : sdkPairedDevices) {
                    if (!info.handleStr.empty()) pairedDeviceIds.emplace(info.handleStr);
                }

                std::vector<std::string> deviceIdsToRemove;
                for (const auto& entry : _pairedDeviceCache) {
                    if (pairedDeviceIds.find(entry.first) == pairedDeviceIds.end()) {
                        LOGINFO("Marking device for removal from cache: deviceID=%s\n", entry.first.c_str());
                        deviceIdsToRemove.push_back(entry.first);
                    }
                }
                for (const auto& deviceId : deviceIdsToRemove) {
                    _pairedDeviceCache.erase(deviceId);
                }
            }

            _adminLock.Unlock();
            return Core::ERROR_NONE;
        }

        Core::hresult BluetoothDeviceManager::writeStorageFromCache()
        {
            if (_service == nullptr) {
                LOGERR("Service is null");
                return Core::ERROR_GENERAL;
            }

            Exchange::IStore* pPersistentStore = _service->QueryInterfaceByCallsign<Exchange::IStore>(PERSISTENT_STORE_CALLSIGN);

            if (pPersistentStore == nullptr) {
                LOGERR("Failed to get PersistentStore interface");
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
            
            LOGINFO("Saving device info JSON: %s", bluetoothDeviceInfoStr.c_str());

            Core::hresult result = pPersistentStore->SetValue(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_DEVICE_INFO, bluetoothDeviceInfoStr);

            if (Core::ERROR_NONE != result) {
                LOGERR("Failed to save device info to PersistentStore, hresult=%d", result);
            }
#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
            else if (_isMigrated.load()) {
                writeFilesystemPersistenceFromCache();
            }
#endif

            pPersistentStore->Release();

            return result;
        }

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

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
            {
                std::string storedVersion;
                const Core::hresult versionResult = readMigrationVersionFromStorage(storedVersion);

                if(Core::ERROR_NONE != versionResult && !missingFromPersistentStore(versionResult)) {
                    LOGERR("Migration state at init: failed to read migrationVersion from PersistentStore, hresult=%d", versionResult);
                }

                const bool isMigrated = (Core::ERROR_NONE == versionResult) && (storedVersion == BLUETOOTH_MIGRATION_VERSION);
                _isMigrated.store(isMigrated);
                LOGINFO("Migration state at init: _isMigrated=%s", isMigrated ? "true" : "false");

                if (isMigrated) {
                    // Valid migration marker present: load the authoritative cache from RDK Persistent Store.
                    const Core::hresult storageResult = updateCacheFromStorage();
                    if ((Core::ERROR_NONE != storageResult) && !missingFromPersistentStore(storageResult)) {
                        LOGERR("PersistentStore read failed (hresult=%d); aborting init to avoid data loss", storageResult);
                        _service->Release();
                        _service = nullptr;
                        return storageResult;
                    }
                } else {
                    // No valid migration marker: any store data is stale/untrusted. Cache stays empty.
                    LOGINFO("migration not yet complete, ignoring any stale store data");
                }

                return Core::ERROR_NONE;
            }
#else
            const Core::hresult storageResult = updateCacheFromStorage();
            if ((Core::ERROR_NONE != storageResult) && !missingFromPersistentStore(storageResult)) {
                LOGERR("PersistentStore read failed (hresult=%d); aborting init to avoid data loss", storageResult);
                _service->Release();
                _service = nullptr;
                return storageResult;
            }

            const Core::hresult deviceResult = updateCacheFromDevice();
            if (Core::ERROR_NONE != deviceResult) {
                // BTRMGR is fundamental to all BT operations — if it's unavailable here it
                // will be unavailable for everything else. Fail init so the plugin is not
                // activated in a broken state.
                LOGERR("Failed to update cache from device (hresult=%d); aborting init", deviceResult);
                _service->Release();
                _service = nullptr;
                return deviceResult;
            }

            const Core::hresult writeResult = writeStorageFromCache();
            if (Core::ERROR_NONE != writeResult) {
                LOGWARN("Failed to write cache to PersistentStore, hresult=%d", writeResult);
                _service->Release();
                _service = nullptr;
                return writeResult;
            }

            return Core::ERROR_NONE;
#endif
        }

        void BluetoothDeviceManager::deinit()
        {
            if (_service != nullptr) {
                _service->Release();
                _service = nullptr;
            }
        }

        Core::hresult BluetoothDeviceManager::getPairedDeviceInfo(const std::string& deviceID, BluetoothDeviceInfo& deviceInfo)
        {
            auto it = _pairedDeviceCache.find(deviceID);
            const bool bFound = (it != _pairedDeviceCache.end());

            if (bFound) {
                deviceInfo = it->second;
            }

            return bFound ? Core::ERROR_NONE : Core::ERROR_NOT_EXIST;
        }

        Core::hresult BluetoothDeviceManager::setAutoConnect(const std::string& deviceID, bool enable)
        {
            LOGINFO("deviceID=%s, enable=%s\n", deviceID.c_str(), enable ? "true" : "false");

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
            if (!_isMigrated.load()) {
                LOGWARN("setAutoConnect rejected: migration has not been performed yet for deviceID=%s", deviceID.c_str());
                return Core::ERROR_GENERAL;
            }
#endif

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

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
            if (!_isMigrated.load()) {
                LOGINFO("migration not complete, returning disabled for deviceID=%s", deviceID.c_str());
                status = AUTO_CONNECT_STATUS_DISABLED;
                return Core::ERROR_NONE;
            }
#endif

            BluetoothDeviceInfo deviceInfo;

            _adminLock.Lock();

            Core::hresult result = getPairedDeviceInfo(deviceID, deviceInfo);

            _adminLock.Unlock();

            if (Core::ERROR_NONE == result) {
                // Consider AUTO_CONNECT_STATUS_UNSET --> AUTO_CONNECT_STATUS_DISABLED
                status = (AUTO_CONNECT_STATUS_UNSET == deviceInfo.autoConnectStatus) ? AUTO_CONNECT_STATUS_DISABLED : deviceInfo.autoConnectStatus;
            }
            
            return result;
        }

        void BluetoothDeviceManager::setLastConnectTimeUtc(const std::string& deviceID)
        {
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

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
            if (!_isMigrated.load()) {
                LOGINFO("migration not complete, skipping persistence write for deviceID=%s", deviceID.c_str());
                return;
            }
#endif
            result = writeStorageFromCache();
            if (Core::ERROR_NONE != result) {
                LOGERR("Failed to update storage from cache after setting lastConnectTimeUtc for deviceID=%s", deviceID.c_str());
            }
        }

        Core::hresult BluetoothDeviceManager::setLastVolumeSetting(const std::string& deviceID, long long volumeSetting)
        {
            LOGINFO("deviceID=%s, volumeSetting=%lld", deviceID.c_str(), volumeSetting);

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

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
            if (!_isMigrated.load()) {
                LOGINFO("migration not complete, skipping persistence write for deviceID=%s", deviceID.c_str());
                return Core::ERROR_NONE;
            }
#endif
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

            if (!_btAdapter) {
                LOGERR("BtAdapter not set in addDevice");
                return Core::ERROR_GENERAL;
            }

            IBtAdapter::BtDeviceProperties props;
            if (!_btAdapter->getDeviceProperties(deviceID, props)) {
                LOGERR("Device not found for deviceID: %s", deviceID.c_str());
                return Core::ERROR_NOT_EXIST;
            }

            _adminLock.Lock();

            BluetoothDeviceInfo deviceInfo;
            deviceInfo.deviceAddr   = props.mac;
            deviceInfo.deviceType   = props.deviceType;
            deviceInfo.friendlyName = props.name.empty() ? deviceID : props.name;
            _pairedDeviceCache[deviceID] = std::move(deviceInfo);

            _adminLock.Unlock();

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
            if (!_isMigrated.load()) {
                LOGINFO("migration not complete, skipping persistence write for deviceID=%s", deviceID.c_str());
                return Core::ERROR_NONE;
            }
#endif
            return writeStorageFromCache();
        }

        Core::hresult BluetoothDeviceManager::removeDevice(const std::string& deviceID)
        {
            LOGINFO("deviceID=%s\n", deviceID.c_str());

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

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
            if (!_isMigrated.load()) {
                LOGINFO("migration not complete, skipping persistence write for deviceID=%s", deviceID.c_str());
                return Core::ERROR_NONE;
            }
#endif
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