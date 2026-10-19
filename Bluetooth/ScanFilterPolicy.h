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
 * Reproduces the discovery filtering the legacy BTMgr/btrCore stack applied
 * to scan results, for backends (the Bluetooth SDK) that only expose BlueZ's
 * own SetDiscoveryFilter and therefore report every device BlueZ sees.
 *
 * BTMgr drops these devices inside btrCore_AddDeviceToScannedDevicesArr(), so
 * the BTMgr backend needs nothing from this class.
 *
 * Inputs are POD so the policy has no dependency on bluetooth-sdk types.
 */
class ScanFilterPolicy {
public:
    ScanFilterPolicy() = delete;

    // Mirrors BTRMGR_DEVICE_OP_TYPE_* as selected from the startScan "profile" string.
    enum class Mode {
        Default,        // no/unrecognised profile: no BlueZ filter, only the always-on exclusions apply
        AudioOutput,
        AudioInput,     // SMARTPHONE / TABLET
        Hid,
        AudioAndHid,
        Le
    };

    static Mode modeFromProfile(const std::string& profile);

    // RDK remote controls (GAP appearance 0x0180 or a known RCU OUI) are never reported by a scan.
    static bool isRemoteControl(const std::string& mac, uint16_t appearance);

    // True when the legacy stack would not have reported this discovered device
    // for a scan started in `mode`. `deviceType` is a DeviceTypeClassifier string.
    static bool shouldSkipDiscovered(Mode mode,
                                     const std::string& mac,
                                     const std::string& name,
                                     uint16_t appearance,
                                     const std::string& deviceType);

private:
    static bool hasOui(const std::string& mac, const char* const* ouis);
    static bool isAudioType(const std::string& deviceType);
    static bool isKnownGamepad(const std::string& mac, const std::string& name);
};

} // namespace Plugin
} // namespace WPEFramework
