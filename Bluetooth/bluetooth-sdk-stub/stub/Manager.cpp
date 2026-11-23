/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
*/

/**
 * @file Manager.cpp
 * @brief Fake Manager backend with no BlueZ/D-Bus dependency (see stub/README.md).
 */

#include <bluetooth/Manager.h>

#include <bluetooth/Adapter.h>

namespace bluetooth {

class Manager::Impl {
 public:
  Impl(Manager* owner, AuthorisationMode, std::function<bool(AuthorisationType, std::shared_ptr<Device>)>,
      LogLocation, std::variant<std::string, std::unique_ptr<LogRedirect>>, std::string)
      : m_owner(owner) {
  }

  // The stub has no Bluetooth backend, so it never provides an adapter. Consumers (e.g. the Bluetooth
  // plugin's BtAdapter::init) rely on this failing to detect that the real SDK is not installed.
  Status getDefaultAdapter(std::shared_ptr<bluetooth::Adapter>&) {
    return Status(StatusCodes::BLUETOOTH_ERROR, "bluetooth-sdk stub: no Bluetooth backend available");
  }

  std::vector<std::shared_ptr<bluetooth::Adapter>> getAdapters() { return {}; }

 private:
  Manager* m_owner;
};

Manager::Manager(AuthorisationMode isAuthManager,
                 std::function<bool(AuthorisationType authType, std::shared_ptr<Device>)> authManagerCallback,
                 LogLocation logOutput, std::variant<std::string, std::unique_ptr<LogRedirect>> output,
                 std::string agentCapability)
    : m_impl(std::make_unique<Impl>(this, isAuthManager, std::move(authManagerCallback), logOutput,
                                    std::move(output), std::move(agentCapability))) {}

// Known deviation: the stub keeps NoAuthorisation although the real SDK delegates with AutoAccept
// (stub-only simplification).
Manager::Manager(std::string agentCapability)
    : Manager(AuthorisationMode::NoAuthorisation, {}, LogLocation::Stdout, {}, std::move(agentCapability)) {}

Manager::~Manager() = default;

Status Manager::getDefaultAdapter(std::shared_ptr<bluetooth::Adapter>& adapter) {
  return m_impl->getDefaultAdapter(adapter);
}
std::vector<std::shared_ptr<bluetooth::Adapter>> Manager::getAdapters() { return m_impl->getAdapters(); }

void Manager::unregisterAgent() {}

bool Manager::isDeviceInCooldown(const std::shared_ptr<Device>&) { return false; }

}  // namespace bluetooth
