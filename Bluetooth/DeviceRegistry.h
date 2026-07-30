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

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <bluetooth/Device.h>

namespace WPEFramework {
namespace Plugin {

/**
 * Maps Bluetooth device MAC addresses to stable numeric handle strings compatible
 * with the BTMgr-era deviceID values stored by clients and PersistentStore.
 *
 * Handle derivation matches BTCore's btrCore_GenerateUniqueDeviceID exactly:
 *   strip colons from "AA:BB:CC:DD:EE:FF" → parse "AABBCCDDEEFF" as hex → decimal string.
 */
class DeviceRegistry {
public:
    DeviceRegistry() = default;
    ~DeviceRegistry() = default;
    DeviceRegistry(const DeviceRegistry&) = delete;
    DeviceRegistry& operator=(const DeviceRegistry&) = delete;

    // Derives the stable numeric handle string from a MAC address.
    static std::string deriveHandle(const std::string& mac);

    // Register a device and derive its handle from mac.
    void registerDevice(const std::string& mac, std::shared_ptr<bluetooth::Device> device);

    // Unregister a device by MAC address.
    void unregisterDevice(const std::string& mac);

    // Look up a device by its numeric handle string. Returns nullptr if not found.
    std::shared_ptr<bluetooth::Device> getDeviceByHandle(const std::string& handleStr) const;

    // Look up a device by MAC address. Returns nullptr if not found.
    std::shared_ptr<bluetooth::Device> getDeviceByMac(const std::string& mac) const;

    // Get the handle string for a MAC address. Returns "" if not registered.
    std::string getHandleForMac(const std::string& mac) const;

    // Get the MAC address for a handle string. Returns "" if not registered.
    std::string getMacForHandle(const std::string& handleStr) const;

    // Cache the device type string for a handle.
    void setDeviceType(const std::string& handleStr, const std::string& typeStr);

    // Retrieve the cached device type string. Returns "" if not cached.
    std::string getDeviceType(const std::string& handleStr) const;

    // Clear all registered devices and cached types.
    void clear();

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<bluetooth::Device>> m_handleToDevice;
    std::unordered_map<std::string, std::string> m_macToHandle;
    std::unordered_map<std::string, std::string> m_handleToType;
};

} // namespace Plugin
} // namespace WPEFramework
