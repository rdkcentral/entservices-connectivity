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

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION

        Core::hresult BluetoothDeviceManager::writeCacheFromFilesystemPersistence()
        {
            BluetoothPersistenceAdapter adapter;

            std::vector<BluetoothDeviceInfo> importedDevices;
            const Core::hresult result = adapter.Read(importedDevices);
            if (Core::ERROR_NOT_EXIST == result) {
                // Missing file is treated as having no entries — clear the cache and return success.
                _adminLock.Lock();
                _pairedDeviceCache.clear();
                _adminLock.Unlock();
                return Core::ERROR_NONE;
            }
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

        std::string BluetoothDeviceManager::computeFNV1aChecksum(const std::string& content) const
        {
            // FNV-1a 64-bit
            static constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
            static constexpr uint64_t FNV_PRIME         = 1099511628211ULL;

            uint64_t hash = FNV_OFFSET_BASIS;
            for (const unsigned char c : content) {
                hash ^= static_cast<uint64_t>(c);
                hash *= FNV_PRIME;
            }
            return std::to_string(hash);
        }

        Core::hresult BluetoothDeviceManager::readFsChecksumFromStorage(std::string& checksum) const
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

            const Core::hresult result = pPersistentStore->GetValue(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_FS_CHECKSUM, checksum);
            pPersistentStore->Release();

            if ((Core::ERROR_NONE != result) && !missingFromPersistentStore(result)) {
                LOGERR("Failed to read fsChecksumAtLastSync from PersistentStore, hresult=%d", result);
            }

            return result;
        }

        Core::hresult BluetoothDeviceManager::writeFsChecksumToStorage(const std::string& checksum)
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

            const Core::hresult result = pPersistentStore->SetValue(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_FS_CHECKSUM, checksum);
            pPersistentStore->Release();

            if (Core::ERROR_NONE != result) {
                LOGERR("Failed to write fsChecksumAtLastSync to PersistentStore, hresult=%d", result);
            }

            return result;
        }

        Core::hresult BluetoothDeviceManager::performMigration()
        {
            _migrationLock.Lock();

            std::string storedChecksum;
            const Core::hresult readChecksumResult = readFsChecksumFromStorage(storedChecksum);
            const bool firstTime = missingFromPersistentStore(readChecksumResult);

            if ((Core::ERROR_NONE != readChecksumResult) && !firstTime) {
                LOGERR("performMigration: failed to read stored checksum, hresult=%d", readChecksumResult);
                _migrationLock.Unlock();
                return readChecksumResult;
            }

            // Read the raw AS file content for checksum computation.
            BluetoothPersistenceAdapter adapter;
            std::string rawContent;
            const Core::hresult readRawResult = adapter.ReadRaw(rawContent);
            if (Core::ERROR_NOT_EXIST == readRawResult) {
                LOGINFO("performMigration: AS filesystem persistence source not found, treating as empty");
                // rawContent remains "" — fall through to checksum comparison and migration
            } else if (Core::ERROR_NONE != readRawResult) {
                LOGERR("performMigration: failed to read AS filesystem persistence source, hresult=%d", readRawResult);
                _migrationLock.Unlock();
                return readRawResult;
            }

            const std::string newChecksum = computeFNV1aChecksum(rawContent);

            if (!firstTime && (newChecksum == storedChecksum)) {
                LOGINFO("performMigration: AS file unchanged (checksum match), no sync needed");
                _migrationLock.Unlock();
                return Core::ERROR_NONE;
            }

            // Import devices from the AS file into the cache.
            const Core::hresult importResult = writeCacheFromFilesystemPersistence();
            if (Core::ERROR_NONE != importResult) {
                LOGERR("performMigration: failed to import from filesystem persistence, hresult=%d", importResult);
                _migrationLock.Unlock();
                return importResult;
            }

            // Backfill any missing device fields (e.g. deviceType) from BTRMGR for devices
            // already present in the imported cache. New devices from BTRMGR are intentionally
            // excluded here to preserve "filesystem is authoritative" semantics — in particular,
            // treating a missing AS file as an empty payload must not repopulate from BTRMGR.
            const Core::hresult deviceResult = updateCacheFromDevice(/* backfillOnly= */ true);
            if (Core::ERROR_NONE != deviceResult) {
                LOGWARN("performMigration: updateCacheFromDevice failed (hresult=%d); proceeding with imported data only", deviceResult);
            }

            // Temporarily set _isMigrated so writeStorageFromCache proceeds.
            _isMigrated.store(true);

            const Core::hresult writeResult = writeStorageFromCache();
            if (Core::ERROR_NONE != writeResult) {
                LOGERR("performMigration: failed to persist imported data to PersistentStore, hresult=%d", writeResult);
                _isMigrated.store(false);
                _migrationLock.Unlock();
                return writeResult;
            }

            const Core::hresult writeChecksumResult = writeFsChecksumToStorage(newChecksum);
            if (Core::ERROR_NONE != writeChecksumResult) {
                LOGERR("performMigration: failed to persist checksum, hresult=%d", writeChecksumResult);
                _isMigrated.store(false);
                _migrationLock.Unlock();
                return writeChecksumResult;
            }

            LOGINFO("performMigration: %s succeeded", firstTime ? "initial migration" : "re-sync");
            _migrationLock.Unlock();
            return Core::ERROR_NONE;
        }

        Core::hresult BluetoothDeviceManager::clearMigration()
        {
            _migrationLock.Lock();

            if (_service == nullptr) {
                LOGERR("clearMigration: service is null");
                _migrationLock.Unlock();
                return Core::ERROR_GENERAL;
            }

            Exchange::IStore* pPersistentStore = _service->QueryInterfaceByCallsign<Exchange::IStore>(PERSISTENT_STORE_CALLSIGN);
            if (pPersistentStore == nullptr) {
                LOGERR("clearMigration: failed to get PersistentStore interface");
                _migrationLock.Unlock();
                return Core::ERROR_GENERAL;
            }

            Core::hresult result = pPersistentStore->DeleteKey(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_DEVICE_INFO);
            if ((Core::ERROR_NONE != result) && !missingFromPersistentStore(result)) {
                LOGERR("clearMigration: failed to delete deviceInfo from PersistentStore, hresult=%d", result);
                pPersistentStore->Release();
                _migrationLock.Unlock();
                return result;
            }

            result = pPersistentStore->DeleteKey(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_FS_CHECKSUM);
            if ((Core::ERROR_NONE != result) && !missingFromPersistentStore(result)) {
                LOGERR("clearMigration: failed to delete fsChecksumAtLastSync from PersistentStore, hresult=%d", result);
                pPersistentStore->Release();
                _migrationLock.Unlock();
                return result;
            }

            pPersistentStore->Release();

            _adminLock.Lock();
            _pairedDeviceCache.clear();
            _adminLock.Unlock();

            _isMigrated.store(false);

            LOGINFO("clearMigration: PersistentStore cleared and migration state reset");
            _migrationLock.Unlock();
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
            BTRMGR_PairedDevicesList_t pairedDevices{};

            BTRMGR_Result_t result = BTRMGR_GetPairedDevices(0, &pairedDevices);
            if (BTRMGR_RESULT_SUCCESS != result)
            {
                LOGERR("Failed to get the paired devices");
                return Core::ERROR_GENERAL;
            }

            _adminLock.Lock();

            for (int i=0; i<pairedDevices.m_numOfDevices; i++)
            {
                string deviceId = std::to_string(pairedDevices.m_deviceProperty[i].m_deviceHandle);
                const char* deviceTypeStr = BTRMGR_GetDeviceTypeAsString(pairedDevices.m_deviceProperty[i].m_deviceType);
                string deviceType = string(deviceTypeStr ? deviceTypeStr : "UNKNOWN");
                const std::string deviceAddr = (pairedDevices.m_deviceProperty[i].m_deviceAddress[0] != '\0')
                    ? std::string(pairedDevices.m_deviceProperty[i].m_deviceAddress)
                    : std::string();

                if (_pairedDeviceCache.find(deviceId) != _pairedDeviceCache.end()) {
                    // Device already exists in cache; backfill any fields that are missing.
                    BluetoothDeviceInfo& existing = _pairedDeviceCache[deviceId];
                    if (existing.friendlyName.empty()) {
                        existing.friendlyName = (pairedDevices.m_deviceProperty[i].m_name[0] != '\0') ? std::string(pairedDevices.m_deviceProperty[i].m_name) : deviceId;
                        LOGINFO("Backfilled friendlyName for deviceID=%s\n", deviceId.c_str());
                    }
                    if (existing.deviceAddr.empty()) {
                        existing.deviceAddr = deviceAddr;
                        LOGINFO("Backfilled deviceAddr for deviceID=%s from BTRMGR: %s\n", deviceId.c_str(), deviceAddr.c_str());
                    }
                    if (existing.deviceType.empty() || existing.deviceType == "UNKNOWN") {
                        existing.deviceType = deviceType;
                        LOGINFO("Backfilled deviceType for deviceID=%s from BTRMGR: %s\n", deviceId.c_str(), deviceType.c_str());
                    }
                } else if (!backfillOnly) {
                    // Device found that's not yet cached; add only when not in backfill-only mode.
                    LOGINFO("Adding device to cache: deviceID=%s, deviceType=%s\n", deviceId.c_str(), deviceType.c_str());
                    BluetoothDeviceInfo deviceInfo;
                    deviceInfo.deviceAddr = std::move(deviceAddr);
                    deviceInfo.deviceType = std::move(deviceType);
                    deviceInfo.friendlyName = (pairedDevices.m_deviceProperty[i].m_name[0] != '\0') ? std::string(pairedDevices.m_deviceProperty[i].m_name) : deviceId;
                    _pairedDeviceCache[deviceId] = std::move(deviceInfo);
                } else {
                    LOGINFO("Skipping device not in imported cache (backfill-only mode): deviceID=%s\n", deviceId.c_str());
                }
            }

            if (!backfillOnly) {
                // Scrub cache of any devices that are no longer paired with the platform.

                std::unordered_set<std::string> pairedDeviceIds;
                pairedDeviceIds.reserve(static_cast<size_t>(pairedDevices.m_numOfDevices));
                for (int i = 0; i < pairedDevices.m_numOfDevices; ++i) {
                    pairedDeviceIds.emplace(std::to_string(pairedDevices.m_deviceProperty[i].m_deviceHandle));
                }

                std::vector<std::string> deviceIdsToRemove;
                for (const auto& entry : _pairedDeviceCache) {
                    const std::string& cachedDeviceId = entry.first;
                    if (pairedDeviceIds.find(cachedDeviceId) == pairedDeviceIds.end()) {
                        LOGINFO("Marking device for removal from cache: deviceID=%s\n", cachedDeviceId.c_str());
                        deviceIdsToRemove.push_back(cachedDeviceId);
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
#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
            if (!_isMigrated.load()) {
                LOGINFO("writeStorageFromCache skipped: migration has not been performed yet");
                return Core::ERROR_NONE;
            }
#endif

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
            else {
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

            const Core::hresult storageResult = updateCacheFromStorage();
            if ((Core::ERROR_NONE != storageResult) && !missingFromPersistentStore(storageResult)) {
                LOGERR("PersistentStore read failed (hresult=%d); aborting init to avoid data loss", storageResult);
                _service->Release();
                _service = nullptr;
                return storageResult;
            }

#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
            {
                std::string storedChecksum;
                const Core::hresult checksumResult = readFsChecksumFromStorage(storedChecksum);
                const bool isMigrated = (Core::ERROR_NONE == checksumResult);
                _isMigrated.store(isMigrated);
                LOGINFO("Migration state at init: _isMigrated=%s", isMigrated ? "true" : "false");
            }
#endif

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
                return Core::ERROR_ILLEGAL_STATE;
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
            BTRMgrDeviceHandle deviceHandle;

            LOGINFO("deviceID=%s\n", deviceID.c_str());
            
            try {
                deviceHandle = (BTRMgrDeviceHandle) stoll(deviceID);
            } catch (const std::exception& e) {
                LOGERR("Failed to parse deviceId: %s\n", e.what());
                return Core::ERROR_INVALID_PARAMETER;
            }

            BTRMGR_DevicesProperty_t deviceProperty{};

            BTRMGR_Result_t result = BTRMGR_GetDeviceProperties(0, deviceHandle, &deviceProperty);
            if (BTRMGR_RESULT_SUCCESS != result)
            {
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