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

// Placeholder for bluetooth-sdk's bluetooth/Manager.h — see Status.h in the
// parent directory for why this file exists and when to remove it.
//
// Manager is declared but not defined here: librdk_bluetooth.so provides the
// real definitions once it's rebuilt against a matching header, which is why
// production builds currently link but cannot yet resolve these symbols.
#pragma once

#include <bluetooth/Adapter.h>
#include <bluetooth/Device.h>
#include <LogRedirect.h>
#include <Status.h>

#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

enum class LogLocation { Stdout, File, LogRedirect };

namespace bluetooth {

enum class AuthorisationMode {
    NoAuthorisation,
    AutoAccept,
    ExternalAuthorisation
};

enum class AuthorisationType {
    PairingRequest,
    ConnectionRequest
};

class Manager {
public:
    Manager(AuthorisationMode mode = AuthorisationMode::NoAuthorisation,
            std::function<bool(AuthorisationType, std::shared_ptr<Device>)> authCb = {},
            LogLocation logLoc = LogLocation::Stdout,
            std::variant<std::string, std::unique_ptr<LogRedirect>> output = {});

    explicit Manager(std::string agentCapability);

    ~Manager();

    Status getDefaultAdapter(std::shared_ptr<Adapter>& adapter);
    std::vector<std::shared_ptr<Adapter>> getAdapters();
};

} // namespace bluetooth
