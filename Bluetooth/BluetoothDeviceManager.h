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
#include <atomic>
#include <unordered_map>
#include <chrono>
#include <ctime>
#include <interfaces/IStore.h>
#include <core/core.h>
#include "UtilsJsonRpc.h"

namespace WPEFramework {
    namespace Plugin {

        static constexpr const char* PERSISTENT_STORE_CALLSIGN = "org.rdk.PersistentStore";
        static constexpr const char* PERSISTENT_STORE_NAMESPACE = "Bluetooth";
        static constexpr const char* PERSISTENT_STORE_KEY_DEVICE_INFO = "deviceInfo";
#ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
        static constexpr const char* PERSISTENT_STORE_KEY_MIGRATION_VERSION = "migrationVersion";
        static constexpr const char* BLUETOOTH_MIGRATION_VERSION = "1";
#endif

        // Forward declaration to avoid circular includes.
        class IBtAdapter;

        enum AutoConnectStatus {
            AUTO_CONNECT_STATUS_DISABLED    = 0,
            AUTO_CONNECT_STATUS_ENABLED     = 1,
            AUTO_CONNECT_STATUS_UNSET       = 2
        };

        struct BluetoothDeviceInfo {
            std::string         deviceAddr          = "";
            std::string         deviceType          = "UNKNOWN";
            std::string         friendlyName        = "";
            long long           lastVolumeSetting   = 0;
            AutoConnectStatus   autoConnectStatus   = AUTO_CONNECT_STATUS_UNSET;
            std::string         lastConnectTimeUtc  = "";
            // BTRMGR reports gamepads as "HUMAN INTERFACE DEVICE"; this disambiguates them.
            bool                isGamePad           = false;
        };

        class BluetoothDeviceManager {

            public:

                BluetoothDeviceManager() = default;
                ~BluetoothDeviceManager() = default;

                Core::hresult init(PluginHost::IShell* service);
                void deinit();

                // Inject the SDK adapter before init() is called.
                // Required for addDevice(), updateCacheFromDevice(), and the migration path.
                // Intentionally unchanged: set once from Bluetooth::Initialize() before init() and never changed
                // afterwards, so no synchronisation is needed.
                void setBtAdapter(IBtAdapter* adapter) { _btAdapter = adapter; }

                Core::hresult setAutoConnect(const std::string& deviceID, bool enable);
                Core::hresult getAutoConnect(const std::string& deviceID, AutoConnectStatus& status);
                void setLastConnectTimeUtc(const std::string& deviceID);
                Core::hresult getLastConnectTimeUtc(const std::string& deviceID, std::string& lastConnectTimeUtc);
        #ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
                // True only while not migrated and the legacy (AS) record loaded at init() marks the device for autoconnect.
                bool isLegacyAutoConnect(const std::string& deviceID);
        #endif
                Core::hresult setLastVolumeSetting(const std::string& deviceID, long long volumeSetting);
                Core::hresult addDevice(const std::string& deviceID);
                Core::hresult removeDevice(const std::string& deviceID);
                std::unordered_map<std::string /* deviceID */, BluetoothDeviceInfo /* deviceInfo */> getPairedDeviceInfos();
        #ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
                Core::hresult performMigration();
                Core::hresult clearMigration();
                bool isMigrated() const { return _isMigrated.load(); }
        #endif

            private:

                mutable Core::CriticalSection _adminLock;
                // Serializes writeStorageFromCache(); lock order: _migrationLock -> _persistLock -> _adminLock.
                mutable Core::CriticalSection _persistLock;
                PluginHost::IShell* _service = nullptr;
                std::unordered_map<std::string /* deviceID */, BluetoothDeviceInfo /* deviceInfo */> _pairedDeviceCache;

                Core::hresult getPairedDeviceInfo(const std::string& deviceID, BluetoothDeviceInfo& deviceInfo);
                Core::hresult updateCacheFromStorage();
                Core::hresult updateCacheFromDevice(bool backfillOnly = false);
                Core::hresult writeStorageFromCache();

                // Returns an AddRef()'d _service under lock so callers hold a safe reference even
                // if deinit() concurrently clears _service; caller must Release() when done.
                PluginHost::IShell* AcquireService() const;
                void ReleaseService();
        #ifdef BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
                Core::hresult writeCacheFromFilesystemPersistence(const std::string& rawContent);
                // Read-only: fills the cache from the AS file for a plugin that is not migrated. Best effort.
                void loadCacheFromLegacyFile();
                void writeFilesystemPersistenceFromCache(std::unordered_map<std::string, BluetoothDeviceInfo> cacheSnapshot);
                Core::hresult readMigrationVersionFromStorage(std::string& version) const;
                Core::hresult writeMigrationVersionToStorage();
                std::atomic<bool> _isMigrated{false};
                mutable Core::CriticalSection _migrationLock;
        #endif
                IBtAdapter* _btAdapter = nullptr;
        };

    } // Plugin
} // WPEFramework
