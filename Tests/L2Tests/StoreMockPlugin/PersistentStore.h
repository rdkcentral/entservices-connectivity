/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 */

#pragma once

#include "Module.h"
#include <interfaces/IStore.h>

namespace WPEFramework {
namespace Plugin {

/**
 * @brief A minimal mock implementation of org.rdk.PersistentStore for L2 tests.
 *
 * This plugin exists solely to satisfy BluetoothDeviceManager's
 * QueryInterfaceByCallsign<Exchange::IStore>("org.rdk.PersistentStore") call
 * during Bluetooth plugin initialization in L2 tests. The real PersistentStore
 * plugin is not built as part of the connectivity L2 test workflow.
 *
 * Behaviour:
 *  - GetValue  → Core::ERROR_NOT_EXIST  (storage is empty; init treats this as a fresh start)
 *  - SetValue  → Core::ERROR_NONE       (write succeeds; init can complete)
 *  - All other IStore methods → Core::ERROR_NONE
 */
class PersistentStore : public PluginHost::IPlugin, public Exchange::IStore {
public:
    PersistentStore() = default;
    ~PersistentStore() override = default;

    PersistentStore(const PersistentStore&) = delete;
    PersistentStore& operator=(const PersistentStore&) = delete;

    BEGIN_INTERFACE_MAP(PersistentStore)
    INTERFACE_ENTRY(PluginHost::IPlugin)
    INTERFACE_ENTRY(Exchange::IStore)
    END_INTERFACE_MAP

    // IPlugin
    const std::string Initialize(PluginHost::IShell* service) override;
    void Deinitialize(PluginHost::IShell* service) override;
    std::string Information() const override;

    // IStore — GetValue reports empty storage so BluetoothDeviceManager starts fresh
    Core::hresult GetValue(const string& ns, const string& key, string& value) override;
    Core::hresult SetValue(const string& ns, const string& key, const string& value) override;
    Core::hresult DeleteKey(const string& ns, const string& key) override;
    Core::hresult DeleteNamespace(const string& ns) override;
    Core::hresult Register(Exchange::IStore::INotification* notification) override;
    Core::hresult Unregister(Exchange::IStore::INotification* notification) override;
};

} // namespace Plugin
} // namespace WPEFramework
