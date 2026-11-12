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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace WPEFramework {
namespace Plugin {

// What a volume or mute operation is for, as the legacy plugin mapped the deviceType a client sent.
enum class VolumeOpType { AudioOutput, AudioInput, Hid, Le, Unknown };

// Unlike the discovery profile there is no audio plus HID combination, and an unrecognised type means audio output.
inline VolumeOpType volumeOpTypeFromDeviceType(const std::string& t) {
    auto has = [&t](const char* what) { return t.find(what) != std::string::npos; };
    if (has("LOUDSPEAKER") || has("HEADPHONES") || has("WEARABLE HEADSET") || has("HIFI AUDIO DEVICE"))
        return VolumeOpType::AudioOutput;
    if (has("SMARTPHONE") || has("TABLET"))
        return VolumeOpType::AudioInput;
    if (has("KEYBOARD") || has("MOUSE") || has("JOYSTICK"))
        return VolumeOpType::Hid;
    if (has("LE TILE") || has("LE"))
        return VolumeOpType::Le;
    if (has("DEFAULT"))
        return VolumeOpType::Unknown;
    return VolumeOpType::AudioOutput;
}

// The plugin volume is 0 to 255 (BTRMGR_SO_MAX_VOLUME) and the SDK's is a float from 0.0 to 1.0.
constexpr uint8_t kVolumeMax = 255;

// What BTMgr compares against before any volume is stored: BTRMGR_SO_MAX_VOLUME - 1.
constexpr uint8_t kVolumeDefaultPrevious = 254;

inline float volumeToSdk(uint8_t volume) {
    return static_cast<float>(volume) / static_cast<float>(kVolumeMax);
}

inline uint8_t volumeFromSdk(float volume) {
    const float clamped = std::min(1.0f, std::max(0.0f, volume));
    return static_cast<uint8_t>(std::lround(clamped * static_cast<float>(kVolumeMax)));
}

// The local steps btrMgr_MediaControl takes for VOLUME_UP and VOLUME_DOWN when AVRCP is not used.
constexpr uint8_t volumeStepUp(uint8_t current) {
    return current < 5 ? 5 : (current <= 245 ? static_cast<uint8_t>(current + 10) : kVolumeMax);
}

constexpr uint8_t volumeStepDown(uint8_t current) {
    return current > 250 ? 250 : (current >= 10 ? static_cast<uint8_t>(current - 10) : 0);
}

// The command of onDeviceMediaStatus, derived as BTRMGR_SetDeviceVolumeMute does: a mute change first, then the
// direction of the volume change.
inline const char* mediaStatusCommand(uint8_t previousVolume, bool previousMute, uint8_t volume, bool mute) {
    if (mute && !previousMute) return "AUDIO_MUTE";
    if (!mute && previousMute) return "AUDIO_UNMUTE";
    if (volume > previousVolume) return "VOLUME_UP";
    if (volume < previousVolume) return "VOLUME_DOWN";
    return "CMD_UNKNOWN";
}

} // namespace Plugin
} // namespace WPEFramework
