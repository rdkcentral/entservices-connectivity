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
#include <string>

namespace WPEFramework {
namespace Plugin {

/**
 * Reproduces btrCore's "wait for the device name" gate on discovery, for the Bluetooth SDK backend,
 * which reports a device as soon as BlueZ creates it, usually before BlueZ has read its name.
 *
 * When the name is missing, or is just the MAC address, btrCore (btrCore_BTDeviceStatusUpdateCb) did this:
 *   - speakers / headsets:       report after a few discovery updates, with the MAC-address name
 *   - HID gamepads / joysticks:  hold up to 7 s for a real name, then report as "Game Controller"
 *   - everything else:           do not report until a real name arrives
 *
 * Inputs are POD so the policy has no dependency on bluetooth-sdk types.
 */
class DiscoveryNameGate {
public:
    DiscoveryNameGate() = delete;

    enum class Kind {
        Audio,
        Controller,
        Other
    };

    // btrCore's BTRCORE_HID_NAME_WAIT_TIMEOUT_SEC.
    static constexpr int kControllerWaitMs = 7000;

    // btrCore processes every 5th discovery update of an unnamed audio device (an RSSI update
    // arrives roughly every 500 ms). The SDK has no such update stream, so approximate it with time.
    static constexpr int kAudioWaitMs = 2500;

    // How often the plugin re-reads a held device's properties, in place of btrCore's name-change event.
    static constexpr int kPollIntervalMs = 500;

    // btrCore's BTCORE_DEFAULT_CONTROLLER_NAME.
    static constexpr const char* kDefaultControllerName = "Game Controller";

    // btrCore_IsDevNameSameAsAddress(): "AA-BB-CC-DD-EE-FF" (BlueZ's alias fallback) or "AA:BB:CC:DD:EE:FF".
    static bool isNameSameAsAddress(const std::string& name, const std::string& mac);

    // HID joystick / gamepad per the Bluetooth Class of Device, as btrCore decides it.
    static bool isController(uint32_t classOfDevice);

    // `deviceType` is a DeviceTypeClassifier string.
    static Kind kindFor(const std::string& deviceType, uint32_t classOfDevice);

    // The name btrCore uses: BlueZ's Alias when it has one (it is the Name unless set otherwise, or the
    // dashed MAC address when there is no Name), else the Name.
    static std::string effectiveName(const std::string& name, const std::string& alias);

    // True once the device can be reported: it has a real name, or its wait has run out.
    static bool shouldRelease(const std::string& name,
                              const std::string& alias,
                              const std::string& mac,
                              const std::string& deviceType,
                              uint32_t classOfDevice,
                              int heldMs);

    // Name to report when BlueZ has no real name: the alias (the MAC address) for most devices,
    // "Game Controller" for controllers, as btrCore does after its wait.
    static std::string fallbackName(const std::string& alias,
                                    const std::string& mac,
                                    uint32_t classOfDevice);
};

} // namespace Plugin
} // namespace WPEFramework
