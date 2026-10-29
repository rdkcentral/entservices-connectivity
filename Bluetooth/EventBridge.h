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

#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <bluetooth/Adapter.h>
#include <bluetooth/Device.h>

#include "BtAdapterCallbacks.h"
#include "DeviceRegistry.h"
#include "DeviceTypeClassifier.h"

namespace WPEFramework {
namespace Plugin {

/**
 * Bridges SDK per-object events to the plugin's flat JSON-RPC notification callbacks.
 *
 * All notification callbacks are plain std::function so the plugin (Bluetooth class)
 * can wire them at initialisation without a virtual interface.
 */
class EventBridge {
public:
    // Status changes that complete a request are sent this long after the request returns, so the client
    // receives the JSON-RPC response first, as it did with the legacy stack.
    // The plugin only sends the response after the handler returns, and the SDK raises its events on its own
    // D-Bus thread while pair(true)/connect(true) is still blocking. Without a delay the notification would
    // overtake the response, and clients that start waiting for it on receipt of the response (e.g. a UI showing
    // "Connecting...") miss it. 250 ms is a heuristic: comfortably longer than returning the response.
    static constexpr std::chrono::milliseconds kCompletionDelay{250};

    explicit EventBridge(DeviceRegistry& registry, BtEventCallbacks&& callbacks)
        : m_registry(registry)
        , m_callbacks(std::move(callbacks))
    {}
    ~EventBridge();

    EventBridge(const EventBridge&) = delete;
    EventBridge& operator=(const EventBridge&) = delete;

    // Called from BtAdapter's adapter event registration callback.
    void onAdapterEvent(bluetooth::AdapterEvent event, bluetooth::AdapterEventData data);

    // Called from per-device event registration callback.
    void onDeviceEvent(bluetooth::DeviceEvent event, std::shared_ptr<bluetooth::Device> device);

    // The legacy stack reported PAIRING_CHANGE / CONNECTION_CHANGE asynchronously, after the pair or connect
    // request had returned, and always reported it when the request succeeded. Clients rely on both.
    // beginDeviceOperation() holds back the device's status changes while the request runs.
    // completeDeviceOperation() releases them kCompletionDelay later and, if the request succeeded but the SDK
    // reported no `event` (e.g. a connect to a device BlueZ already connected while pairing), emits it.
    // Every beginDeviceOperation() must be matched by a completeDeviceOperation().
    void beginDeviceOperation(const std::shared_ptr<bluetooth::Device>& device);
    void completeDeviceOperation(bluetooth::DeviceEvent event, std::shared_ptr<bluetooth::Device> device,
                                 bool succeeded);

    // Stops the worker and drops undelivered status changes. Called before the SDK objects are released.
    void stop();

private:
    struct HeldEvent {
        bluetooth::DeviceEvent event;
        std::shared_ptr<bluetooth::Device> device;
    };
    struct DeviceOperation {
        unsigned active = 0;                 // requests in progress (or awaiting their delayed release)
        unsigned seen = 0;                   // bit mask of DeviceEvent since the latest beginDeviceOperation()
        std::vector<HeldEvent> held;         // status changes withheld until the request's response has gone out
    };
    struct Task {
        std::chrono::steady_clock::time_point due;
        std::function<void()> run;
    };

    void emitDeviceEvent(bluetooth::DeviceEvent event, std::shared_ptr<bluetooth::Device> device);
    void emitDeviceStatusChanged(const std::string& newStatus,
                                 std::shared_ptr<bluetooth::Device> device,
                                 bool paired, bool connected);
    void releaseDeviceOperation(const std::string& mac, bluetooth::DeviceEvent event,
                                std::shared_ptr<bluetooth::Device> device, bool succeeded);

    // Device status changes are delivered in order on one worker thread; a task does not run before its due time.
    // A single FIFO is used (rather than a timer per request) so that a status change arriving after a request
    // completes can never be delivered ahead of the changes held during that request, e.g. a Disconnected must
    // not overtake the Connected it follows. It also keeps the D-Bus property reads done while building each
    // notification off the SDK's event thread.
    void enqueue(std::chrono::steady_clock::time_point due, std::function<void()> run);
    void worker();

    DeviceRegistry& m_registry;
    BtEventCallbacks m_callbacks;

    std::mutex m_opsMutex;
    std::unordered_map<std::string, DeviceOperation> m_ops;   // MAC -> request in progress

    std::mutex m_taskMutex;
    std::condition_variable m_taskCv;
    std::deque<Task> m_tasks;
    std::thread m_worker;
    bool m_stopping = false;
};

} // namespace Plugin
} // namespace WPEFramework
