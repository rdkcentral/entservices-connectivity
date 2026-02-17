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

#include "BluetoothDeviceManager.h"


namespace WPEFramework {
    namespace Plugin {

        Core::hresult BluetoothDeviceManager::updateBluetoothDeviceInfoCache()
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

                _bluetoothDeviceInfoCache.clear();

                for (uint16_t i = 0; i < deviceInfoArray.Length(); i++) {
                    JsonObject deviceInfoObj = deviceInfoArray[i].Object();
                    std::string deviceID = deviceInfoObj["deviceID"].String();
                    AutoConnectStatus autoConnectStatus = static_cast<AutoConnectStatus>(deviceInfoObj["autoConnectStatus"].Number());
                    std::string lastConnectTimeUtc = deviceInfoObj["lastConnectTimeUtc"].String();

                    BluetoothDeviceInfo deviceInfo;
                    deviceInfo.autoConnectStatus = autoConnectStatus;
                    deviceInfo.lastConnectTimeUtc = lastConnectTimeUtc;

                    _bluetoothDeviceInfoCache[deviceID] = std::move(deviceInfo);

                    LOGINFO("Loaded device info for deviceID=%s, autoConnectStatus=%d, lastConnectTimeUtc=%s\n",
                            deviceID.c_str(), static_cast<int>(autoConnectStatus), lastConnectTimeUtc.c_str());
                }

                _adminLock.Unlock();
                
            } else {
                LOGERR("Failed to load device info from PersistentStore, hresult=%d\n", result);
            }

            pPersistentStore->Release();

            return result;
        }

        Core::hresult BluetoothDeviceManager::updateBluetoothDeviceInfoPersistentStore()
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

            JsonArray deviceInfoArray;

            _adminLock.Lock();

            for (const auto& entry : _bluetoothDeviceInfoCache) {
                const std::string& deviceID = entry.first;
                const BluetoothDeviceInfo& deviceInfo = entry.second;

                JsonObject deviceInfoObj;
                deviceInfoObj["deviceID"] = deviceID;
                deviceInfoObj["autoConnectStatus"] = static_cast<int>(deviceInfo.autoConnectStatus);
                deviceInfoObj["lastConnectTimeUtc"] = deviceInfo.lastConnectTimeUtc;

                deviceInfoArray.Add(deviceInfoObj);
            }

            string bluetoothDeviceInfoStr;
            deviceInfoArray.ToString(bluetoothDeviceInfoStr);

            _adminLock.Unlock();
            
            LOGINFO("Saving device info JSON: %s\n", bluetoothDeviceInfoStr.c_str());

            Core::hresult result = pPersistentStore->SetValue(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_DEVICE_INFO, bluetoothDeviceInfoStr);

            if (Core::ERROR_NONE != result) {
                LOGERR("Failed to save device info to PersistentStore, hresult=%d\n", result);
            }

            pPersistentStore->Release();

            return result;
        }

        const string BluetoothDeviceManager::init(PluginHost::IShell* service)
        {
            _service = service;
            _service->AddRef();
            updateBluetoothDeviceInfoCache();
            return {};
        }

        void BluetoothDeviceManager::deinit()
        {
            if (_service != nullptr) {
                _service->Release();
                _service = nullptr;
            }
        }

        Core::hresult BluetoothDeviceManager::getBluetoothDeviceInfo(const std::string& deviceID, BluetoothDeviceInfo& deviceInfo)
        {
            auto it = _bluetoothDeviceInfoCache.find(deviceID);
            const bool bFound = (it != _bluetoothDeviceInfoCache.end());

            if (bFound) {
                deviceInfo = it->second;
            }

            return bFound ? Core::ERROR_NONE : Core::ERROR_NOT_EXIST;
        }

        void BluetoothDeviceManager::setAutoConnect(const std::string& deviceID, bool enable)
        {
            LOGINFO("deviceID=%s, enable=%s\n", deviceID.c_str(), enable ? "true" : "false");

            AutoConnectStatus autoConnectStatus = enable ? AUTO_CONNECT_STATUS_ENABLED : AUTO_CONNECT_STATUS_DISABLED;
            BluetoothDeviceInfo deviceInfo;

            _adminLock.Lock();

            getBluetoothDeviceInfo(deviceID, deviceInfo);
            deviceInfo.autoConnectStatus = autoConnectStatus;

            _bluetoothDeviceInfoCache[deviceID] = std::move(deviceInfo);

            _adminLock.Unlock();
            
            updateBluetoothDeviceInfoPersistentStore();
        }

        Core::hresult BluetoothDeviceManager::getAutoConnect(const std::string& deviceID, AutoConnectStatus& status)
        {
            LOGINFO("deviceID=%s\n", deviceID.c_str());
            BluetoothDeviceInfo deviceInfo;

            _adminLock.Lock();

            Core::hresult result = getBluetoothDeviceInfo(deviceID, deviceInfo);

            _adminLock.Unlock();

            if (Core::ERROR_NONE == result) {
                status = deviceInfo.autoConnectStatus;
            }
            
            return result;
        }

        void BluetoothDeviceManager::setLastConnectTimeUtc(const std::string& deviceID)
        {
            // TODO: What resolution do we want for the timestamp?
            // For now, we use seconds precision in UTC formatted as ISO 8601 string.

            auto now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            std::tm utc_tm;
            gmtime_r(&now_c, &utc_tm);
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc_tm);
            const std::string currentUtcTime = buffer;

            LOGINFO("deviceID=%s, time=%s\n", deviceID.c_str(), currentUtcTime.c_str());

            BluetoothDeviceInfo deviceInfo;

            _adminLock.Lock();

            getBluetoothDeviceInfo(deviceID, deviceInfo);
            deviceInfo.lastConnectTimeUtc = std::move(currentUtcTime);
            _bluetoothDeviceInfoCache[deviceID] = std::move(deviceInfo);

            _adminLock.Unlock();

            updateBluetoothDeviceInfoPersistentStore();
        }

        Core::hresult BluetoothDeviceManager::getLastConnectTimeUtc(const std::string& deviceID, std::string& lastConnectTimeUtc)
        {
            LOGINFO("deviceID=%s\n", deviceID.c_str());
            BluetoothDeviceInfo deviceInfo;

            _adminLock.Lock();
        
            Core::hresult result = getBluetoothDeviceInfo(deviceID, deviceInfo);

            _adminLock.Unlock();

            if (Core::ERROR_NONE == result) {
                lastConnectTimeUtc = deviceInfo.lastConnectTimeUtc;
            }
            
            return result;
        }

    } // Plugin
} // WPEFramework