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

// Placeholder for bluetooth-sdk's Events.h — see Status.h in this directory
// for why this file exists and when to remove it.
#pragma once

#include <functional>

template <typename Event, typename EventData>
class EventEmitter {
public:
    void registerForEvents(std::function<void(Event, EventData)> cb) {
        m_eventCallback = std::move(cb);
    }
    void unregisterForEvents() { m_eventCallback = nullptr; }

protected:
    std::function<void(Event, EventData)> m_eventCallback;
};
