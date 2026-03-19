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
#include <exception>
#include <unordered_set>

#include "BluetoothDeviceManager.h"
#include "btmgr.h"


namespace WPEFramework {
    namespace Plugin {

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

                    BluetoothDeviceInfo deviceInfo;
                    deviceInfo.deviceType = std::move(deviceType);
                    deviceInfo.autoConnectStatus = autoConnectStatus;
                    deviceInfo.lastConnectTimeUtc = std::move(lastConnectTimeUtc);

                    _pairedDeviceCache[deviceID] = std::move(deviceInfo);

                    LOGINFO("Loaded device info for deviceID=%s, autoConnectStatus=%d, lastConnectTimeUtc=%s\n",
                            deviceID.c_str(),
                            static_cast<int>(_pairedDeviceCache[deviceID].autoConnectStatus),
                            _pairedDeviceCache[deviceID].lastConnectTimeUtc.c_str());
                }

                _adminLock.Unlock();
                
            } else if (Core::ERROR_NOT_EXIST != result) {
                LOGERR("Failed to load device info from PersistentStore, hresult=%d\n", result);
            }

            pPersistentStore->Release();

            return result;
        }

        Core::hresult BluetoothDeviceManager::updateCacheFromDevice()
        {
            BTRMGR_PairedDevicesList_t pairedDevices{};

            BTRMGR_Result_t result = BTRMGR_GetPairedDevices(0, &pairedDevices);
            if (BTRMGR_RESULT_SUCCESS != result)
            {
                LOGERR("Failed to get the paired devices");
                return Core::ERROR_GENERAL;
            }

            _adminLock.Lock();
            
            // Add any paired devices not already in cache.

            for (int i=0; i<pairedDevices.m_numOfDevices; i++)
            {
                string deviceId = std::to_string(pairedDevices.m_deviceProperty[i].m_deviceHandle);
                const char* deviceTypeStr = BTRMGR_GetDeviceTypeAsString(pairedDevices.m_deviceProperty[i].m_deviceType);
                string deviceType = string(deviceTypeStr ? deviceTypeStr : "UNKNOWN");

                if (_pairedDeviceCache.find(deviceId) != _pairedDeviceCache.end()) {
                    // Device already exists in cache, ignore.
                    continue;
                } else {
                    // Device found that's not yet cached, add.
                    LOGINFO("Adding device to cache: deviceID=%s, deviceType=%s\n", deviceId.c_str(), deviceType.c_str());
                    BluetoothDeviceInfo deviceInfo;
                    deviceInfo.deviceType = std::move(deviceType);
                    _pairedDeviceCache[deviceId] = std::move(deviceInfo);
                }
            }

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

            _adminLock.Unlock();
            return Core::ERROR_NONE;
        }

        Core::hresult BluetoothDeviceManager::updateStorageFromCache()
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

            pPersistentStore->Release();

            return result;
        }

        const string BluetoothDeviceManager::init(PluginHost::IShell* service)
        {
            if (service == nullptr) {
                return "Service is null";
            }

            _service = service;
            _service->AddRef();

            updateCacheFromStorage();
            updateCacheFromDevice();
            updateStorageFromCache();

            return {};
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

            BluetoothDeviceInfo deviceInfo;

            _adminLock.Lock();

            Core::hresult result = getPairedDeviceInfo(deviceID, deviceInfo);

            if (Core::ERROR_NONE != result) {
                LOGWARN("Device info is not found in cache for deviceID: %s", deviceID.c_str());
            }

            deviceInfo.autoConnectStatus = enable ? AUTO_CONNECT_STATUS_ENABLED : AUTO_CONNECT_STATUS_DISABLED;
            _pairedDeviceCache[deviceID] = std::move(deviceInfo);
            _adminLock.Unlock();
                
            result = updateStorageFromCache();
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
                LOGWARN("Device info is not found in cache for deviceID: %s", deviceID.c_str());
            }

            auto now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            std::tm utc_tm;
            gmtime_r(&now_c, &utc_tm);
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc_tm);
            const std::string currentUtcTime = buffer;

            LOGINFO("deviceID=%s, time=%s\n", deviceID.c_str(), currentUtcTime.c_str());

            deviceInfo.lastConnectTimeUtc = std::move(currentUtcTime);

            _adminLock.Lock();
            _pairedDeviceCache[deviceID] = std::move(deviceInfo);
            _adminLock.Unlock();

            result = updateStorageFromCache();
            if (Core::ERROR_NONE != result) {
                LOGERR("Failed to update storage from cache after setting lastConnectTimeUtc for deviceID=%s", deviceID.c_str());
            }
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

            BTRMGR_DevicesProperty_t deviceProperty;

            BTRMGR_Result_t result = BTRMGR_GetDeviceProperties(0, deviceHandle, &deviceProperty);
            if (BTRMGR_RESULT_SUCCESS != result)
            {
                LOGERR("Failed to get device properties for deviceID: %s", deviceID.c_str());
                return Core::ERROR_NOT_EXIST;
            }

            _adminLock.Lock();

            BluetoothDeviceInfo deviceInfo;
            const char* deviceTypeStr = BTRMGR_GetDeviceTypeAsString(deviceProperty.m_deviceType);
            deviceInfo.deviceType = (deviceTypeStr != nullptr) ? deviceTypeStr : "UNKNOWN";
            _pairedDeviceCache[deviceID] = std::move(deviceInfo);

            _adminLock.Unlock();

            return updateStorageFromCache();
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
            return updateStorageFromCache();
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