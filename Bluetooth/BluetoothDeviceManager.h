/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2026 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* You may not use this file except in compliance with the License.
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

#pragma once

#include "Module.h"
#include <unordered_map>
#include <interfaces/IStore.h>
#include <core/core.h>
#include "UtilsJsonRpc.h"

#define PERSISTENT_STORE_CALLSIGN "org.rdk.PersistentStore"
#define PERSISTENT_STORE_NAMESPACE "Bluetooth"
#define PERSISTENT_STORE_KEY_DEVICE_INFO "deviceInfo"

namespace WPEFramework {
    namespace Plugin {

        typedef enum _AutoConnectStatus {
            AUTO_CONNECT_STATUS_DISABLED    = 0,
            AUTO_CONNECT_STATUS_ENABLED     = 1,
            AUTO_CONNECT_STATUS_UNSET       = 2
        } AutoConnectStatus;

        typedef struct _BluetoothDeviceInfo {
            AutoConnectStatus   autoConnectStatus   = AUTO_CONNECT_STATUS_UNSET;
            std::string         lastConnectTimeUtc  = "";
        } BluetoothDeviceInfo;

        class BluetoothDeviceManager {

            public:

                BluetoothDeviceManager() = default;
                ~BluetoothDeviceManager() = default;

                void init(PluginHost::IShell* service);
                void deinit();

                Core::hresult getBluetoothDeviceInfo(const std::string& bdAddr, BluetoothDeviceInfo& deviceInfo);
                void setAutoConnect(const std::string& bdAddr, bool enable);
                Core::hresult getAutoConnectStatus(const std::string& bdAddr, AutoConnectStatus& status);
                void setLastConnectTimeUtc(const std::string& bdAddr);
                std::string getLastConnectTimeUtc(const std::string& bdAddr);

            private:

                mutable Core::CriticalSection _adminLock;
                std::unordered_map<std::string /* bdAddr */, BluetoothDeviceInfo /* deviceInfo */> _bluetoothDeviceInfoCache;
                Exchange::IStore* _persistentStore = nullptr;

                Core::hresult updateBluetoothDeviceInfoCache();
                Core::hresult updateBluetoothDeviceInfoPersistentStore();
        };

    } // Plugin
} // WPEFramework
