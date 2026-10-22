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

#include "DiscoveryNameGate.h"

#include <cctype>

namespace WPEFramework {
namespace Plugin {

namespace {

constexpr size_t kMacLength = 17; // "AA:BB:CC:DD:EE:FF"

// Class of Device: major class Peripheral (0x500) with the Joystick / Gamepad minor class.
constexpr uint32_t kCodPeripheral = 0x500u;
constexpr uint32_t kCodJoystick   = 0x504u;
constexpr uint32_t kCodGamepad    = 0x508u;

bool isSeparator(char c) {
    return c == ':' || c == '-';
}

bool sameHexChar(char a, char b) {
    return std::toupper(static_cast<unsigned char>(a)) == std::toupper(static_cast<unsigned char>(b));
}

} // namespace

// static
bool DiscoveryNameGate::isNameSameAsAddress(const std::string& name, const std::string& mac) {
    if (name.size() != kMacLength || mac.size() != kMacLength) return false;
    for (size_t i = 0; i < kMacLength; ++i) {
        if ((i % 3) == 2) {
            if (!isSeparator(name[i])) return false;
        } else if (!sameHexChar(name[i], mac[i])) {
            return false;
        }
    }
    return true;
}

// static
bool DiscoveryNameGate::isController(uint32_t classOfDevice) {
    if ((classOfDevice & kCodPeripheral) != kCodPeripheral) return false;
    const uint32_t cls = classOfDevice & 0xFFFu;
    return cls == kCodJoystick || cls == kCodGamepad;
}

// static
DiscoveryNameGate::Kind DiscoveryNameGate::kindFor(const std::string& deviceType, uint32_t classOfDevice) {
    if (deviceType == "LOUDSPEAKER" || deviceType == "HEADPHONES"
        || deviceType == "WEARABLE HEADSET" || deviceType == "HANDSFREE") {
        return Kind::Audio;
    }
    return isController(classOfDevice) ? Kind::Controller : Kind::Other;
}

// static
std::string DiscoveryNameGate::effectiveName(const std::string& name, const std::string& alias) {
    return alias.empty() ? name : alias;
}

// static
bool DiscoveryNameGate::shouldRelease(const std::string& name,
                                      const std::string& alias,
                                      const std::string& mac,
                                      const std::string& deviceType,
                                      uint32_t classOfDevice,
                                      int heldMs) {
    const std::string effective = effectiveName(name, alias);
    if (!effective.empty() && !isNameSameAsAddress(effective, mac)) return true;

    switch (kindFor(deviceType, classOfDevice)) {
        case Kind::Audio:      return heldMs >= kAudioWaitMs;
        case Kind::Controller: return heldMs >= kControllerWaitMs;
        case Kind::Other:      break;
    }
    return false;
}

// static
std::string DiscoveryNameGate::fallbackName(const std::string& alias,
                                            const std::string& mac,
                                            uint32_t classOfDevice) {
    const bool aliasIsAddress = alias.empty() || isNameSameAsAddress(alias, mac);
    if (isController(classOfDevice) && aliasIsAddress) return kDefaultControllerName;
    if (!alias.empty()) return alias;

    // BlueZ's own alias fallback: the address with dashes.
    std::string dashed = mac;
    for (auto& c : dashed) {
        if (c == ':') c = '-';
    }
    return dashed;
}

} // namespace Plugin
} // namespace WPEFramework
