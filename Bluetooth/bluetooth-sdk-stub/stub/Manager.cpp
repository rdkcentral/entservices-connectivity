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
       LogLocation, std::variant<std::string, std::unique_ptr<LogRedirect>>)
      : m_owner(owner) {
    m_adapters.push_back(std::make_shared<Adapter>(0));
  }

  Status getDefaultAdapter(std::shared_ptr<bluetooth::Adapter>& adapter) {
    if (m_adapters.empty()) {
      return Status(StatusCodes::BLUETOOTH_ERROR, "NO ADAPTER FOUND");
    }
    adapter = m_adapters[0];
    return Status();
  }

  std::vector<std::shared_ptr<bluetooth::Adapter>> getAdapters() { return m_adapters; }

 private:
  Manager* m_owner;
  std::vector<std::shared_ptr<bluetooth::Adapter>> m_adapters;
};

Manager::Manager(AuthorisationMode isAuthManager,
                 std::function<bool(AuthorisationType authType, std::shared_ptr<Device>)> authManagerCallback,
                 LogLocation logOutput, std::variant<std::string, std::unique_ptr<LogRedirect>> output)
    : m_impl(std::make_unique<Impl>(this, isAuthManager, std::move(authManagerCallback), logOutput,
                                    std::move(output))) {}

Manager::Manager(std::string /* agentCapability */)
    : Manager(AuthorisationMode::NoAuthorisation, {}, LogLocation::Stdout, {}) {}

Manager::~Manager() = default;

Status Manager::getDefaultAdapter(std::shared_ptr<bluetooth::Adapter>& adapter) {
  return m_impl->getDefaultAdapter(adapter);
}
std::vector<std::shared_ptr<bluetooth::Adapter>> Manager::getAdapters() { return m_impl->getAdapters(); }

#ifdef AUDIO_SUPPORT
WpNode* Manager::findWirePlumberAudioNode(const std::string&) { return nullptr; }
#endif

}  // namespace bluetooth
