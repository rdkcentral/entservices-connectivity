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
#include <mutex>

template <typename event, typename eventData>
class EventEmitter {
 private:
 std::function<void(event, eventData)> m_eventCallback = nullptr;
 mutable std::mutex m_eventMtx;
 public:
  void registerForEvents(std::function<void(event, eventData)> eventCallback) {
    std::lock_guard<std::mutex> lock(m_eventMtx);
    m_eventCallback = std::move(eventCallback);
  }
  void unregisterForEvents() {
    std::lock_guard<std::mutex> lock(m_eventMtx);
    m_eventCallback = nullptr;
  }
  void emitEvent(event e, eventData data) {
    std::function<void(event, eventData)> cb;
    {
      std::lock_guard<std::mutex> lock(m_eventMtx);
      cb = m_eventCallback;
    }
    if (cb) {
      cb(e, data);
    }
  }
};

