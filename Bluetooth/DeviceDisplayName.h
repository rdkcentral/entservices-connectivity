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

#include <memory>
#include <string>

#include <bluetooth/Device.h>

#include "DiscoveryNameGate.h"

namespace WPEFramework {
namespace Plugin {

// The name to show for a device. Like btrCore, the Alias is preferred over the Name; bluetooth::Device::name()
// is only a fallback, as it returns the literal "Unnamed Advertisement" when BlueZ has no Name.
inline std::string deviceDisplayName(const std::shared_ptr<bluetooth::Device>& device) {
    std::string mac;
    device->address(mac);

    bluetooth::DeviceProperties props;
    if (device->getAllProperties(props)) {
        const std::string alias = props.alias.value_or("");
        const std::string name  = DiscoveryNameGate::effectiveName(props.name.value_or(""), alias);
        if (!name.empty() && !DiscoveryNameGate::isNameSameAsAddress(name, mac)) return name;
        return DiscoveryNameGate::fallbackName(alias, mac, props.classOfDevice.value_or(0));
    }

    std::string name;
    if (device->name(name) && !DiscoveryNameGate::isNameSameAsAddress(name, mac)) return name;
    return DiscoveryNameGate::fallbackName("", mac, 0);
}

} // namespace Plugin
} // namespace WPEFramework
