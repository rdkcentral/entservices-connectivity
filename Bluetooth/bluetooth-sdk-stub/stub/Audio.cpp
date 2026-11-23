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

#include <bluetooth/Audio.h>

#include <utility>

namespace bluetooth {
namespace {
Status unavailable() {
  return Status(StatusCodes::BLUETOOTH_ERROR,
                "bluetooth-sdk stub: no audio backend available");
}
}  // namespace

Audio::Audio(std::string deviceMacAddress, WpNode* node, WpProxy* device)
    : m_deviceMacAddress(std::move(deviceMacAddress)),
      m_node(node),
      m_device(device) {}

Audio::~Audio() {
  {
    std::lock_guard<std::mutex> lock(m_callbackState->mutex);
    m_callbackState->stop = true;
    m_callbackState->cv.notify_all();
  }
  if (m_callbackWorker.joinable()) {
    m_callbackWorker.join();
  }
}

Status Audio::setVolume(float) { return unavailable(); }
float Audio::getVolume() { return 0.0f; }
Status Audio::setMute(bool) { return unavailable(); }
bool Audio::isMuted() { return false; }
Status Audio::setDelayCompensation(uint32_t) { return unavailable(); }

void Audio::bindSelf(const std::shared_ptr<Audio>& self) {
  m_idleCallbackState->audio = self;
}

void Audio::handleNodePropsChanged() {}

void Audio::dispatchCallback(std::function<void()> task) {
  if (task) {
    task();
  }
}

void Audio::callbackWorkerLoop(std::shared_ptr<CallbackWorker>, std::string) {}

}  // namespace bluetooth
