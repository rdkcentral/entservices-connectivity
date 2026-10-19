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

#include "ScanFilterPolicy.h"

#include <algorithm>
#include <cctype>

namespace WPEFramework {
namespace Plugin {

namespace {

constexpr uint16_t kRemoteControlAppearance = 0x0180;
constexpr size_t   kOuiLength               = 8; // "AA:BB:CC"

// Same lists as btrCore's BTRCORE_REMOTE_OUI_VALUES / BTRCORE_AMAZON_OUI_VALUES.
const char* const kRemoteOuis[] = {
    "20:44:41", // LC103
    "E8:0F:C8", // EC302
    "98:06:3A", // EC201
    "18:46:44", // EC201
    "20:E7:B6", // Platco
    "E4:A6:34", // Platco
    "B8:F2:55", // Platco
    "1C:41:90", // Platco
    "B4:CB:B8", // P-Xumo XR100
    nullptr
};

const char* const kAmazonOuis[] = {
    "20:A1:71", // Luna Gamepad
    "EC:FD:86", // AmazonController-0B0
    nullptr
};

bool contains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

// static
ScanFilterPolicy::Mode ScanFilterPolicy::modeFromProfile(const std::string& profile) {
    const bool audio = contains(profile, "LOUDSPEAKER")
                    || contains(profile, "HEADPHONES")
                    || contains(profile, "WEARABLE HEADSET")
                    || contains(profile, "HIFI AUDIO DEVICE");
    const bool hid   = contains(profile, "KEYBOARD")
                    || contains(profile, "MOUSE")
                    || contains(profile, "JOYSTICK");
    const bool phone = contains(profile, "SMARTPHONE")
                    || contains(profile, "TABLET");

    // Same precedence as the legacy Bluetooth::startDeviceDiscovery().
    if (audio && hid) return Mode::AudioAndHid;
    if (audio)        return Mode::AudioOutput;
    if (phone)        return Mode::AudioInput;
    if (hid)          return Mode::Hid;
    if (contains(profile, "LE")) return Mode::Le;
    return Mode::Default;
}

// static
bool ScanFilterPolicy::hasOui(const std::string& mac, const char* const* ouis) {
    if (mac.size() < kOuiLength) return false;
    std::string prefix = mac.substr(0, kOuiLength);
    std::transform(prefix.begin(), prefix.end(), prefix.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    for (size_t i = 0; ouis[i] != nullptr; ++i) {
        if (prefix == ouis[i]) return true;
    }
    return false;
}

// static
bool ScanFilterPolicy::isRemoteControl(const std::string& mac, uint16_t appearance) {
    return appearance == kRemoteControlAppearance || hasOui(mac, kRemoteOuis);
}

// static
bool ScanFilterPolicy::isAudioType(const std::string& deviceType) {
    return deviceType == "LOUDSPEAKER"
        || deviceType == "HEADPHONES"
        || deviceType == "WEARABLE HEADSET";
}

// static
bool ScanFilterPolicy::isKnownGamepad(const std::string& mac, const std::string& name) {
    // btrCore classifies these as gamepads even when CoD/appearance/UUIDs say nothing.
    return hasOui(mac, kAmazonOuis) || contains(name, "Luna") || contains(name, "Amazon");
}

// static
std::string ScanFilterPolicy::deviceTypeFromIcon(const std::string& icon) {
    if (icon.rfind("input-", 0) == 0)  return "HUMAN INTERFACE DEVICE";
    if (icon.rfind("audio-", 0) == 0)  return "LOUDSPEAKER";
    if (icon.rfind("phone", 0) == 0)   return "SMARTPHONE";
    if (icon.rfind("computer", 0) == 0) return "TABLET";
    return "UNKNOWN DEVICE";
}

// static
bool ScanFilterPolicy::shouldSkipDiscovered(Mode mode,
                                            const std::string& mac,
                                            const std::string& name,
                                            uint16_t appearance,
                                            const std::string& deviceType,
                                            bool typeSettled) {
    if (isRemoteControl(mac, appearance)) return true;

    // HID scans skip audio devices (btrCore: HID devices have no reliable appearance, so
    // audio devices that BlueZ also reports have to be dropped explicitly).
    if (mode == Mode::Hid && isAudioType(deviceType)) return true;

    // Audio scans skip devices whose type could not be determined.
    if (typeSettled
        && (mode == Mode::AudioOutput || mode == Mode::AudioAndHid)
        && deviceType == "UNKNOWN DEVICE"
        && !isKnownGamepad(mac, name)) {
        return true;
    }
    return false;
}

} // namespace Plugin
} // namespace WPEFramework
