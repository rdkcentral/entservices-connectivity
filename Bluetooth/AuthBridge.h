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
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <bluetooth/Manager.h>
#include <bluetooth/Device.h>

#include "BtAdapterCallbacks.h"
#include "DeviceRegistry.h"
#include "DeviceTypeClassifier.h"

namespace WPEFramework {
namespace Plugin {

/**
 * Bridges the SDK's synchronous authManagerCallback to the plugin's async
 * respondToEvent flow.
 *
 * Auto-accept policy (mirrors btrMgr_ConnectionInAuthenticationCb):
 *   - Audio devices (paired, not in cooldown) → auto-accept
 *   - HID devices  (paired, not in cooldown)  → auto-accept
 *   - SMARTPHONE / TABLET / LE devices        → escalate to client
 *
 * PairingRequest always escalates to client.
 *
 * Threading: for escalated cases the callback blocks the D-Bus dispatch
 * thread on a per-device condition variable (max AUTH_TIMEOUT_SECONDS).
 * Thunder JSON-RPC is D-Bus-independent so respondToEvent delivery is unaffected.
 */
class AuthBridge {
public:
    static constexpr int AUTH_TIMEOUT_SECONDS = 30;

    explicit AuthBridge(DeviceRegistry& registry, BtAuthCallbacks&& callbacks)
        : m_registry(registry)
        , m_callbacks(std::move(callbacks))
    {}

    // Called by SDK authManagerCallback. May block for AUTH_TIMEOUT_SECONDS on escalated cases.
    bool onAuthRequest(bluetooth::AuthorisationType type, std::shared_ptr<bluetooth::Device> device);

    // Called by setEventResponseWrapper to resolve a pending auth decision.
    // mac is the device's Bluetooth MAC address string.
    bool onRespondToEvent(const std::string& mac, bool accepted);

private:
    bool isAutoAcceptDevice(const std::string& deviceType,
                            const std::string& handleStr,
                            std::shared_ptr<bluetooth::Device> device) const;

    struct PendingAuth {
        bool resolved{false};
        bool accepted{false};
        std::condition_variable cv;
    };

    DeviceRegistry& m_registry;
    BtAuthCallbacks m_callbacks;

    mutable std::mutex m_pendingMutex;
    std::unordered_map<std::string, std::shared_ptr<PendingAuth>> m_pendingAuths;
};

} // namespace Plugin
} // namespace WPEFramework
