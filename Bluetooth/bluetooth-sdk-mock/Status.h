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

// Placeholder for bluetooth-sdk's Status.h, pending the SDK team's agreed
// sdbus-c++-free refactor of their public headers. Delete this whole
// bluetooth-sdk-mock/ directory and point back at the real installed headers
// once that refactor ships. Matches the real header's public API exactly so
// callers compile unchanged either way.
#pragma once

#include <string>

enum class StatusCodes { SUCCESS, BLUETOOTH_ERROR, INVALID_OPERATION };

class Status {
public:
    explicit Status(StatusCodes code = StatusCodes::SUCCESS, const std::string& message = "Success")
        : m_code(code), m_message(message) {}

    std::string get_message() const { return m_message; }
    operator bool() const { return m_code == StatusCodes::SUCCESS; }

private:
    StatusCodes m_code;
    std::string m_message;
};
