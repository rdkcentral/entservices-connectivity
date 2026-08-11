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

// Stub implementations for bluetooth-sdk types — used in SDK L2 test builds.
//
// bluetooth::Manager delegates all calls through g_managerStub (set by the
// test fixture).  bluetooth::Uuid provides the static ServiceClasses constants.
// This file is compiled into WPEFrameworkBluetooth in place of the real SDK.

#include <bluetooth/Manager.h>
#include <bluetooth/Uuid.h>

namespace bluetooth {

// ── Manager stub ─────────────────────────────────────────────────────────────

IManagerStub* g_managerStub = nullptr;

Manager::Manager(AuthorisationMode, std::function<bool(AuthorisationType, std::shared_ptr<Device>)>,
                 LogLocation, std::variant<std::string, std::unique_ptr<LogRedirect>>) {}

Manager::Manager(std::string /* agentCapability */) {}

Manager::~Manager() {}

Status Manager::getDefaultAdapter(std::shared_ptr<Adapter>& adapter) {
    if (g_managerStub) return g_managerStub->getDefaultAdapter(adapter);
    return Status(StatusCodes::BLUETOOTH_ERROR, "No manager stub set");
}

std::vector<std::shared_ptr<Adapter>> Manager::getAdapters() {
    return {};
}

// ── Uuid static constants ────────────────────────────────────────────────────

const Uuid Uuid::ServiceClasses::AudioSink{uint16_t(0x110B)};
const Uuid Uuid::ServiceClasses::AudioSource{uint16_t(0x110A)};

} // namespace bluetooth
