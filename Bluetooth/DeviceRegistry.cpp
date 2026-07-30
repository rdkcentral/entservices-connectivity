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

#include "DeviceRegistry.h"

#include <cstdlib>

namespace WPEFramework {
namespace Plugin {

// static
std::string DeviceRegistry::deriveHandle(const std::string& mac) {
    if (mac.size() < 17) return "0";
    char hexStr[13] = {};
    hexStr[0]  = mac[0];  hexStr[1]  = mac[1];
    hexStr[2]  = mac[3];  hexStr[3]  = mac[4];
    hexStr[4]  = mac[6];  hexStr[5]  = mac[7];
    hexStr[6]  = mac[9];  hexStr[7]  = mac[10];
    hexStr[8]  = mac[12]; hexStr[9]  = mac[13];
    hexStr[10] = mac[15]; hexStr[11] = mac[16];
    hexStr[12] = '\0';
    uint64_t handle = static_cast<uint64_t>(strtoll(hexStr, nullptr, 16));
    return std::to_string(handle);
}

void DeviceRegistry::registerDevice(const std::string& mac) {
    std::string handle = deriveHandle(mac);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_macToHandle[mac] = handle;
}

void DeviceRegistry::unregisterDevice(const std::string& mac) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_macToHandle.find(mac);
    if (it != m_macToHandle.end()) {
        m_handleToType.erase(it->second);
        m_macToHandle.erase(it);
    }
}

std::string DeviceRegistry::getHandleForMac(const std::string& mac) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_macToHandle.find(mac);
    return (it != m_macToHandle.end()) ? it->second : "";
}

std::string DeviceRegistry::getMacForHandle(const std::string& handleStr) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& pair : m_macToHandle) {
        if (pair.second == handleStr) return pair.first;
    }
    return "";
}

void DeviceRegistry::setDeviceType(const std::string& handleStr, const std::string& typeStr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handleToType[handleStr] = typeStr;
}

std::string DeviceRegistry::getDeviceType(const std::string& handleStr) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_handleToType.find(handleStr);
    return (it != m_handleToType.end()) ? it->second : "";
}

void DeviceRegistry::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_macToHandle.clear();
    m_handleToType.clear();
}

} // namespace Plugin
} // namespace WPEFramework

// static
std::string DeviceRegistry::deriveHandle(const std::string& mac) {
    // Matches BTCore btrCore_GenerateUniqueDeviceID: strip colons, parse as hex.
    if (mac.size() < 17) {
        return "0";
    }
    char hexStr[13] = {};
    hexStr[0]  = mac[0];  hexStr[1]  = mac[1];
    hexStr[2]  = mac[3];  hexStr[3]  = mac[4];
    hexStr[4]  = mac[6];  hexStr[5]  = mac[7];
    hexStr[6]  = mac[9];  hexStr[7]  = mac[10];
    hexStr[8]  = mac[12]; hexStr[9]  = mac[13];
    hexStr[10] = mac[15]; hexStr[11] = mac[16];
    hexStr[12] = '\0';
    uint64_t handle = static_cast<uint64_t>(strtoll(hexStr, nullptr, 16));
    return std::to_string(handle);
}

void DeviceRegistry::registerDevice(const std::string& mac, std::shared_ptr<bluetooth::Device> device) {
    std::string handle = deriveHandle(mac);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handleToDevice[handle] = std::move(device);
    m_macToHandle[mac] = handle;
}

void DeviceRegistry::unregisterDevice(const std::string& mac) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_macToHandle.find(mac);
    if (it != m_macToHandle.end()) {
        m_handleToDevice.erase(it->second);
        m_handleToType.erase(it->second);
        m_macToHandle.erase(it);
    }
}

std::shared_ptr<bluetooth::Device> DeviceRegistry::getDeviceByHandle(const std::string& handleStr) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_handleToDevice.find(handleStr);
    return (it != m_handleToDevice.end()) ? it->second : nullptr;
}

std::shared_ptr<bluetooth::Device> DeviceRegistry::getDeviceByMac(const std::string& mac) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_macToHandle.find(mac);
    if (it == m_macToHandle.end()) return nullptr;
    auto it2 = m_handleToDevice.find(it->second);
    return (it2 != m_handleToDevice.end()) ? it2->second : nullptr;
}

std::string DeviceRegistry::getHandleForMac(const std::string& mac) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_macToHandle.find(mac);
    return (it != m_macToHandle.end()) ? it->second : "";
}

std::string DeviceRegistry::getMacForHandle(const std::string& handleStr) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& pair : m_macToHandle) {
        if (pair.second == handleStr) return pair.first;
    }
    return "";
}

void DeviceRegistry::setDeviceType(const std::string& handleStr, const std::string& typeStr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handleToType[handleStr] = typeStr;
}

std::string DeviceRegistry::getDeviceType(const std::string& handleStr) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_handleToType.find(handleStr);
    return (it != m_handleToType.end()) ? it->second : "";
}

void DeviceRegistry::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handleToDevice.clear();
    m_macToHandle.clear();
    m_handleToType.clear();
}

} // namespace Plugin
} // namespace WPEFramework
