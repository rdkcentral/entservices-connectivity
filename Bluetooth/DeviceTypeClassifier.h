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

#include <string>

#include <bluetooth/Appearance.h>
#include <bluetooth/Device.h>
#include <bluetooth/Uuid.h>

namespace WPEFramework {
namespace Plugin {

/**
 * Infers device type strings from SDK DeviceProperties.
 *
 * Output strings match BTMgr's btrMgr_MapDeviceTypeFromCore exactly, including
 * collapsed mappings (PortableAudio/CarAudio/HIFIAudioDevice → "LOUDSPEAKER",
 * all HID subtypes → "HUMAN INTERFACE DEVICE"). PersistentStore deviceType
 * values and power-policy checks depend on these specific strings.
 *
 * Classification order:
 *   1. BLE Appearance value (for LE devices)
 *   2. Classic BT Class of Device bits
 *   3. Service UUID fallback
 *   4. Default: "UNKNOWN DEVICE"
 */
class DeviceTypeClassifier {
public:
    DeviceTypeClassifier() = delete;

    static std::string classify(const bluetooth::DeviceProperties& props);

private:
    static std::string classifyByAppearance(uint16_t appearance);
    static std::string classifyByCoD(uint32_t cod);
    static std::string classifyByUuids(const std::vector<std::string>& uuids);

    // CoD major device class constants (bits 12-8 of the 24-bit CoD).
    static constexpr uint8_t COD_MAJOR_COMPUTER    = 0x01;
    static constexpr uint8_t COD_MAJOR_PHONE       = 0x02;
    static constexpr uint8_t COD_MAJOR_AUDIO_VIDEO = 0x04;
    static constexpr uint8_t COD_MAJOR_PERIPHERAL  = 0x05;

    // Audio/Video minor device class constants (bits 7-2 of CoD, i.e., cod >> 2 & 0x3F).
    static constexpr uint8_t COD_AV_MINOR_WEARABLE_HEADSET = 0x01;
    static constexpr uint8_t COD_AV_MINOR_HANDSFREE        = 0x02;
    static constexpr uint8_t COD_AV_MINOR_LOUDSPEAKER      = 0x05;
    static constexpr uint8_t COD_AV_MINOR_HEADPHONES       = 0x06;
    static constexpr uint8_t COD_AV_MINOR_PORTABLE_AUDIO   = 0x07; // → "LOUDSPEAKER" (collapsed)
    static constexpr uint8_t COD_AV_MINOR_CAR_AUDIO        = 0x08; // → "LOUDSPEAKER" (collapsed)
    static constexpr uint8_t COD_AV_MINOR_HIFI_AUDIO       = 0x0b; // → "LOUDSPEAKER" (collapsed)

    // Short-form UUID substrings for service class detection (lower-case hex).
    static constexpr const char* UUID_AUDIO_SINK                  = "0000110b";
    static constexpr const char* UUID_AUDIO_SOURCE                = "0000110a";
    static constexpr const char* UUID_ADVANCED_AUDIO_DISTRIBUTION = "0000110d";
    static constexpr const char* UUID_HID                         = "00001124";
};

} // namespace Plugin
} // namespace WPEFramework
