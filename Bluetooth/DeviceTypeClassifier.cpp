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

#include "DeviceTypeClassifier.h"

#include <algorithm>
#include <cctype>

namespace WPEFramework {
namespace Plugin {

// static
std::string DeviceTypeClassifier::classify(uint16_t appearance, uint32_t classOfDevice,
                                            const std::vector<std::string>& uuids) {
    // 1. BLE Appearance — most reliable for LE-only devices.
    if (appearance != 0) {
        std::string result = classifyByAppearance(appearance);
        if (result != "UNKNOWN DEVICE") {
            return result;
        }
    }

    // 2. Classic BT Class of Device.
    if (classOfDevice != 0) {
        std::string result = classifyByCoD(classOfDevice);
        if (result != "UNKNOWN DEVICE") {
            return result;
        }
    }

    // 3. Service UUID fallback.
    if (!uuids.empty()) {
        std::string result = classifyByUuids(uuids);
        if (result != "UNKNOWN DEVICE") {
            return result;
        }
    }

    return "UNKNOWN DEVICE";
}

// static
std::string DeviceTypeClassifier::classifyByAppearance(uint16_t appearance) {
    // Category occupies the upper 10 bits per Bluetooth spec.
    const uint16_t category = appearance & BleAppearanceCategory::kMask;

    switch (category) {
        case BleAppearanceCategory::kHumanInterfaceDevice:
            return "HUMAN INTERFACE DEVICE";

        case BleAppearanceCategory::kPhone:
            return "SMARTPHONE";

        case BleAppearanceCategory::kComputer:
            return "TABLET";

        case BleAppearanceCategory::kTag:
        case BleAppearanceCategory::kKeyring:
            return "LE TILE";

        case BleAppearanceCategory::kWatch:
        case BleAppearanceCategory::kUncategorized:
        default:
            return "UNKNOWN DEVICE";
    }
}

// static
std::string DeviceTypeClassifier::classifyByCoD(uint32_t cod) {
    const uint8_t majorClass = static_cast<uint8_t>((cod >> 8) & 0x1Fu);
    const uint8_t minorClass = static_cast<uint8_t>((cod >> 2) & 0x3Fu);

    switch (majorClass) {
        case COD_MAJOR_PHONE:
            return "SMARTPHONE";

        case COD_MAJOR_COMPUTER:
            return "TABLET";

        case COD_MAJOR_PERIPHERAL:
            return "HUMAN INTERFACE DEVICE";

        case COD_MAJOR_AUDIO_VIDEO:
            switch (minorClass) {
                case COD_AV_MINOR_WEARABLE_HEADSET:
                    return "WEARABLE HEADSET";
                case COD_AV_MINOR_HANDSFREE:
                    return "HANDSFREE";
                case COD_AV_MINOR_HEADPHONES:
                    return "HEADPHONES";
                case COD_AV_MINOR_LOUDSPEAKER:
                case COD_AV_MINOR_PORTABLE_AUDIO:
                case COD_AV_MINOR_CAR_AUDIO:
                case COD_AV_MINOR_HIFI_AUDIO:
                    return "LOUDSPEAKER";
                default:
                    // Other A/V minor classes (microphone, STB, VCR, etc.)
                    // default to LOUDSPEAKER to match BTMgr's majority mapping.
                    return "LOUDSPEAKER";
            }

        default:
            return "UNKNOWN DEVICE";
    }
}

// static
std::string DeviceTypeClassifier::classifyByUuids(const std::vector<std::string>& uuids) {
    bool hasAudioSink   = false;
    bool hasAudioSource = false;
    bool hasA2dp        = false;
    bool hasHid         = false;

    for (const auto& uuid : uuids) {
        // UUIDs from BlueZ are full 128-bit strings; match on the 16-bit base portion.
        std::string lower = uuid;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (lower.find(UUID_AUDIO_SINK) != std::string::npos)                  hasAudioSink   = true;
        if (lower.find(UUID_AUDIO_SOURCE) != std::string::npos)                hasAudioSource = true;
        if (lower.find(UUID_ADVANCED_AUDIO_DISTRIBUTION) != std::string::npos) hasA2dp        = true;
        if (lower.find(UUID_HID) != std::string::npos)                         hasHid         = true;
    }

    if (hasHid)                      return "HUMAN INTERFACE DEVICE";
    if (hasAudioSource)              return "SMARTPHONE";
    if (hasAudioSink || hasA2dp)     return "HEADPHONES";

    return "UNKNOWN DEVICE";
}

} // namespace Plugin
} // namespace WPEFramework
