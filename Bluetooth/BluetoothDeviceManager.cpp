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
            _adminLock.Lock();
            
            string bluetoothDeviceInfoStr;
            Core::hresult result = _persistentStore->GetValue(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_DEVICE_INFO, bluetoothDeviceInfoStr);

            if (Core::ERROR_NONE == result) {
                printf("*** _DEBUG: BluetoothDeviceManager::updateBluetoothDeviceInfoCache: Loaded device info JSON: %s\n", bluetoothDeviceInfoStr.c_str());
                JsonArray deviceInfoArray;
                deviceInfoArray.FromString(bluetoothDeviceInfoStr);
                for (uint16_t i = 0; i < deviceInfoArray.Length(); i++) {
                    JsonObject deviceInfoObj = deviceInfoArray[i].Object();
                    std::string bdAddr = deviceInfoObj["bdAddr"].String();
                    AutoConnectStatus autoConnectStatus = static_cast<AutoConnectStatus>(deviceInfoObj["autoConnectStatus"].Number());
                    std::string lastConnectTimeUtc = deviceInfoObj["lastConnectTimeUtc"].String();

                    BluetoothDeviceInfo deviceInfo;
                    deviceInfo.autoConnectStatus = autoConnectStatus;
                    deviceInfo.lastConnectTimeUtc = lastConnectTimeUtc;

                    _bluetoothDeviceInfoCache[bdAddr] = deviceInfo;

                    printf("*** _DEBUG: BluetoothDeviceManager::updateBluetoothDeviceInfoCache: Loaded device info for bdAddr=%s, autoConnectStatus=%d, lastConnectTimeUtc=%s\n",
                            bdAddr.c_str(), static_cast<int>(autoConnectStatus), lastConnectTimeUtc.c_str());
                }
            } else {
                printf("*** _DEBUG: BluetoothDeviceManager::updateBluetoothDeviceInfoCache: No existing device info in PersistentStore or failed to load (hresult=%d)\n", result);
                LOGERR("Failed to load device info from PersistentStore, hresult=%d\n", result);
            }

            _adminLock.Unlock();

            return result;
        }

        Core::hresult BluetoothDeviceManager::updateBluetoothDeviceInfoPersistentStore()
        {
            JsonArray deviceInfoArray;

            _adminLock.Lock();

            for (const auto& entry : _bluetoothDeviceInfoCache) {
                const std::string& bdAddr = entry.first;
                const BluetoothDeviceInfo& deviceInfo = entry.second;

                JsonObject deviceInfoObj;
                deviceInfoObj["bdAddr"] = bdAddr;
                deviceInfoObj["autoConnectStatus"] = static_cast<int>(deviceInfo.autoConnectStatus);
                deviceInfoObj["lastConnectTimeUtc"] = deviceInfo.lastConnectTimeUtc;

                deviceInfoArray.Add(deviceInfoObj);
            }

            string bluetoothDeviceInfoStr;
            deviceInfoArray.ToString(bluetoothDeviceInfoStr);
            
            printf("*** _DEBUG: BluetoothDeviceManager::updateBluetoothDeviceInfoPersistentStore: Saving device info JSON: %s\n", bluetoothDeviceInfoStr.c_str());

            Core::hresult result = _persistentStore->SetValue(PERSISTENT_STORE_NAMESPACE, PERSISTENT_STORE_KEY_DEVICE_INFO, bluetoothDeviceInfoStr);

            if (Core::ERROR_NONE != result) {
                printf("*** _DEBUG: BluetoothDeviceManager::updateBluetoothDeviceInfoPersistentStore: Failed to save device info to PersistentStore (hresult=%d)\n", result);
                LOGERR("Failed to save device info to PersistentStore, hresult=%d\n", result);
            }

            _adminLock.Unlock();

            return result;
        }

        void BluetoothDeviceManager::init(PluginHost::IShell* service)
        {
            _persistentStore = service->QueryInterfaceByCallsign<Exchange::IStore>(PERSISTENT_STORE_CALLSIGN);

            if (_persistentStore != nullptr) {
                printf("*** _DEBUG: BluetoothDeviceManager::init: Successfully got PersistentStore interface\n");
                if (Core::ERROR_NONE != updateBluetoothDeviceInfoCache()) {
                    LOGERR("Failed to update Bluetooth device info cache from PersistentStore\n");
                }
            } else {
                printf("*** _DEBUG: BluetoothDeviceManager::init: Failed to get PersistentStore interface\n");
                LOGERR("Failed to get PersistentStore interface\n");
            }
        }

        void BluetoothDeviceManager::deinit()
        {
            if (_persistentStore != nullptr) {
                _persistentStore->Release();
                _persistentStore = nullptr;
            }
        }

        Core::hresult BluetoothDeviceManager::getBluetoothDeviceInfo(const std::string& bdAddr, BluetoothDeviceInfo& deviceInfo)
        {
            _adminLock.Lock();
            
            auto it = _bluetoothDeviceInfoCache.find(bdAddr);
            if (it != _bluetoothDeviceInfoCache.end()) {
                deviceInfo = it->second;
            }

            _adminLock.Unlock();

            return (it != _bluetoothDeviceInfoCache.end()) ? Core::ERROR_NONE : Core::ERROR_GENERAL;
        }

        void BluetoothDeviceManager::setAutoConnect(const std::string& bdAddr, bool enable)
        {
            printf("*** _DEBUG: BluetoothDeviceManager::setAutoConnect: bdAddr=%s, enable=%s\n", bdAddr.c_str(), enable ? "true" : "false");

            AutoConnectStatus autoConnectStatus = enable ? AUTO_CONNECT_STATUS_ENABLED : AUTO_CONNECT_STATUS_DISABLED;
            BluetoothDeviceInfo deviceInfo;
            getBluetoothDeviceInfo(bdAddr, deviceInfo);
            deviceInfo.autoConnectStatus = autoConnectStatus;

            _adminLock.Lock();
            _bluetoothDeviceInfoCache[bdAddr] = deviceInfo;
            _adminLock.Unlock();
            
            updateBluetoothDeviceInfoPersistentStore();
        }

        AutoConnectStatus BluetoothDeviceManager::getAutoConnectStatus(const std::string& bdAddr)
        {
            printf("*** _DEBUG: BluetoothDeviceManager::getAutoConnectStatus: bdAddr=%s\n", bdAddr.c_str());
            BluetoothDeviceInfo deviceInfo;
            getBluetoothDeviceInfo(bdAddr, deviceInfo);
            return deviceInfo.autoConnectStatus;
        }

        void BluetoothDeviceManager::setLastConnectTimeUtc(const std::string& bdAddr)
        {
            // TODO: What resolution do we want for the timestamp?
            // For now, we use seconds precision in UTC formatted as ISO 8601 string.

            auto now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            std::tm* utc_tm = std::gmtime(&now_c);
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", utc_tm);
            const std::string currentUtcTime = buffer;

            printf("*** _DEBUG: BluetoothDeviceManager::setLastConnectTimeUtc: bdAddr=%s, time=%s\n", bdAddr.c_str(), currentUtcTime.c_str());

            BluetoothDeviceInfo deviceInfo;
            getBluetoothDeviceInfo(bdAddr, deviceInfo);
            deviceInfo.lastConnectTimeUtc = currentUtcTime;

            updateBluetoothDeviceInfoPersistentStore();
        }

        std::string BluetoothDeviceManager::getLastConnectTimeUtc(const std::string& bdAddr)
        {
            printf("*** _DEBUG: BluetoothDeviceManager::getLastConnectTimeUtc: bdAddr=%s\n", bdAddr.c_str());
            BluetoothDeviceInfo deviceInfo;
            getBluetoothDeviceInfo(bdAddr, deviceInfo);
            return deviceInfo.lastConnectTimeUtc;
        }

    } // Plugin
} // WPEFramework