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
#pragma once
#include <functional>
#include <string>

struct LogRedirect final {
public:
    LogRedirect(std::function<void(std::string&)> debug,
                std::function<void(std::string&)> info,
                std::function<void(std::string&)> warning,
                std::function<void(std::string&)> error)
        : m_debug(debug), m_info(info), m_warning(warning), m_error(error) {}

protected:
    std::function<void(std::string&)> m_debug;
    std::function<void(std::string&)> m_info;
    std::function<void(std::string&)> m_warning;
    std::function<void(std::string&)> m_error;
    friend class Logger; // Logger is the only class that can use LogRedirect functions directly
};
