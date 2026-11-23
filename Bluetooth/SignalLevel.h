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

#include <climits>
#include <cstdint>

namespace WPEFramework {
namespace Plugin {

// btrCore's value for "no RSSI known"; legacy getDeviceInfo reports it verbatim in "rssi".
constexpr int32_t BT_RSSI_UNKNOWN = INT_MIN;

// Reproduces btrMgr_MapSignalStrengthToRSSI (BTRMGR_SIGNAL_GOOD/FAIR/POOR = -60/-70/-90): a signal level of 1 to 4,
// never 0, so an unknown RSSI (BT_RSSI_UNKNOWN) maps to 1.
constexpr int16_t signalLevelFromRssi(int32_t rssi) {
    return rssi >= -60 ? 4 : (rssi >= -70 ? 3 : (rssi >= -90 ? 2 : 1));
}

} // namespace Plugin
} // namespace WPEFramework
