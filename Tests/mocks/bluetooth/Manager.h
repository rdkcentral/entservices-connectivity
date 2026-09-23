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

// Stub for bluetooth-sdk bluetooth/Manager.h — used in SDK L2 test builds.
//
// bluetooth::Manager is a concrete class whose methods delegate to a static
// IManagerStub pointer, following the same pattern as entservices-testframework's
// btmgr.h mock.  Tests set bluetooth::g_managerStub before activating the plugin.
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

// Abstract interface implemented by the test mock.
struct IManagerStub {
    virtual ~IManagerStub() = default;
    virtual Status getDefaultAdapter(std::shared_ptr<Adapter>& adapter) = 0;
};

// Set by the test fixture before activating the Bluetooth plugin.
// Cleared to nullptr in test teardown.
extern IManagerStub* g_managerStub;

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
