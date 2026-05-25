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

#include "PersistentStore.h"

namespace WPEFramework {
namespace Plugin {

SERVICE_REGISTRATION(PersistentStore, 1, 0);

const std::string PersistentStore::Initialize(PluginHost::IShell* service)
{
    // No-op: this is a test-only stub; no real storage is initialised.
    return {};
}

void PersistentStore::Deinitialize(PluginHost::IShell* service)
{
    // No-op
}

std::string PersistentStore::Information() const
{
    return "StoreMockPlugin (test-only PersistentStore stub)";
}

// Return ERROR_NOT_EXIST so that BluetoothDeviceManager::missingFromPersistentStore()
// returns true, and init() treats the store as empty and proceeds normally.
Core::hresult PersistentStore::GetValue(const string& /*ns*/, const string& /*key*/, string& value)
{
    value.clear();
    return Core::ERROR_NOT_EXIST;
}

// Return ERROR_NONE so that BluetoothDeviceManager::writeStorageFromCache() succeeds
// and Bluetooth plugin activation completes without error.
Core::hresult PersistentStore::SetValue(const string& /*ns*/, const string& /*key*/, const string& /*value*/)
{
    return Core::ERROR_NONE;
}

Core::hresult PersistentStore::DeleteKey(const string& /*ns*/, const string& /*key*/)
{
    return Core::ERROR_NONE;
}

Core::hresult PersistentStore::DeleteNamespace(const string& /*ns*/)
{
    return Core::ERROR_NONE;
}

Core::hresult PersistentStore::Register(Exchange::IStore::INotification* /*notification*/)
{
    return Core::ERROR_NONE;
}

Core::hresult PersistentStore::Unregister(Exchange::IStore::INotification* /*notification*/)
{
    return Core::ERROR_NONE;
}

} // namespace Plugin
} // namespace WPEFramework
