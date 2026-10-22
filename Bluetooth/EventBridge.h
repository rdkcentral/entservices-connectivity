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

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
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
    explicit EventBridge(DeviceRegistry& registry, BtEventCallbacks&& callbacks)
        : m_registry(registry)
        , m_callbacks(std::move(callbacks))
    {}

    // Called from BtAdapter's adapter event registration callback.
    void onAdapterEvent(bluetooth::AdapterEvent event, bluetooth::AdapterEventData data);

    // Called from per-device event registration callback.
    void onDeviceEvent(bluetooth::DeviceEvent event, std::shared_ptr<bluetooth::Device> device);

    // The legacy stack always reported PAIRING_CHANGE / CONNECTION_CHANGE when a pair or connect request
    // succeeded. BlueZ only signals property changes, so a connect to a device that BlueZ already connected
    // while pairing produces no SDK event. beginDeviceOperation() marks the start of a request and
    // completeDeviceOperation() emits the status change if the SDK did not report it since then.
    void beginDeviceOperation(const std::string& mac);
    void completeDeviceOperation(bluetooth::DeviceEvent event, std::shared_ptr<bluetooth::Device> device);

private:
    void emitDeviceStatusChanged(const std::string& newStatus,
                                 std::shared_ptr<bluetooth::Device> device,
                                 bool paired, bool connected);

    void noteDeviceEvent(bluetooth::DeviceEvent event, const std::shared_ptr<bluetooth::Device>& device);

    DeviceRegistry& m_registry;
    BtEventCallbacks m_callbacks;

    std::mutex m_seenMutex;
    std::unordered_map<std::string, unsigned> m_seenEvents;   // MAC -> bit mask of DeviceEvent since beginDeviceOperation()
};

} // namespace Plugin
} // namespace WPEFramework
