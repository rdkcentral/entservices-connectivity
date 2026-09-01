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

// Placeholder for bluetooth-sdk's bluetooth/Uuid.h — see Status.h in the
// parent directory for why this file exists and when to remove it.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bluetooth {

class Uuid {
public:
    explicit Uuid(uint16_t id) : m_id(id), m_str(shortToFull(id)) {}
    explicit Uuid(const std::string& id) : m_id(0), m_str(id) {}

    std::string toString(bool shortForm = false) const {
        if (shortForm && m_id) return "0x" + std::to_string(m_id);
        return m_str;
    }
    bool operator==(const Uuid& other) const { return m_str == other.m_str; }
    bool operator!=(const Uuid& other) const { return !(*this == other); }

    // Service class UUIDs used by BtSdkAdapterImpl::buildScanFilter.
    struct ServiceClasses {
        static const Uuid AudioSink;    // 0x110B
        static const Uuid AudioSource;  // 0x110A
    };

private:
    uint16_t    m_id;
    std::string m_str;

    static std::string shortToFull(uint16_t id) {
        char buf[40];
        snprintf(buf, sizeof(buf), "0000%04x-0000-1000-8000-00805f9b34fb", id);
        return buf;
    }
};

} // namespace bluetooth
