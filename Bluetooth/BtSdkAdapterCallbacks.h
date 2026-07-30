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

// SDK-free callback type definitions shared between IBtSdkAdapter, EventBridge, and AuthBridge.
// Using only standard types so this header compiles without bluetooth-sdk in test builds.

#include <cstdint>
#include <functional>
#include <string>

namespace WPEFramework {
namespace Plugin {

struct BtEventCallbacks {
    std::function<void(const std::string& eventId, const std::string& newStatus,
                       const std::string& deviceId, const std::string& name,
                       const std::string& deviceType, uint32_t rawDeviceType,
                       uint16_t rawBleDeviceType, bool paired, bool connected,
                       bool lastConnectedState, bool hasAutoConnect, bool autoConnect)> onStatusChanged;

    std::function<void(const std::string& deviceId, const std::string& name,
                       const std::string& deviceType, uint32_t rawDeviceType,
                       uint16_t rawBleDeviceType, bool paired,
                       bool lastConnectedState, const std::string& discoveryType)> onDiscoveredDevice;

    std::function<void(const std::string& deviceId, const std::string& name,
                       const std::string& deviceType, uint32_t rawDeviceType,
                       uint16_t rawBleDeviceType, bool lastConnectedState)> onDeviceFound;

    std::function<void(const std::string& deviceId, const std::string& name,
                       const std::string& deviceType, uint32_t rawDeviceType,
                       uint16_t rawBleDeviceType, bool lastConnectedState)> onDeviceLost;

    std::function<void(const std::string& newStatus, const std::string& deviceId,
                       const std::string& name, const std::string& deviceType,
                       uint32_t rawDeviceType, uint16_t rawBleDeviceType,
                       bool paired, bool connected)> onRequestFailed;

    std::function<bool(const std::string& handleStr, bool& autoConnect)> getAutoConnect;
};

struct BtAuthCallbacks {
    std::function<void(const std::string& deviceId, const std::string& name,
                       const std::string& deviceType, uint32_t vendorId,
                       const std::string& mac, const std::string& supportedProfile,
                       bool pinRequired, uint32_t pinValue)> onPairingRequest;

    std::function<void(const std::string& deviceId, const std::string& name,
                       const std::string& deviceType, uint32_t vendorId,
                       const std::string& mac, const std::string& supportedProfile)> onConnectionRequest;

    std::function<bool(const std::string& handleStr)> isPaired;
};

} // namespace Plugin
} // namespace WPEFramework
