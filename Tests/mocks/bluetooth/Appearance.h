/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
*/

// Stub for bluetooth-sdk bluetooth/Appearance.h — used in SDK L2 test builds.
#pragma once

#include <cstdint>

#define BLUETOOTH_APPEARANCE_CATEGORY_MASK    0xffc0
#define BLUETOOTH_APPEARANCE_SUBCATEGORY_MASK 0x003f

namespace bluetooth {

class Appearance {
public:
    enum class Category : uint16_t {
        Uncategorized        = 0x0000,
        Phone                = 0x0040,
        Computer             = 0x0080,
        Watch                = 0x00c0,
        HumanInterfaceDevice = 0x03c0,
        Tag                  = 0x0200,
        Keyring              = 0x0240,
        // Other categories not used by DeviceTypeClassifier are not listed.
    };

    enum class SubCategory : uint16_t {
        Generic = 0x0000,
        Gamepad = 0x0004,
    };
};

} // namespace bluetooth
