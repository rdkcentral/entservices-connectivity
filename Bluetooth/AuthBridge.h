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
 * Connection request policy (mirrors the BTMgr-based plugin):
 *   - A device the plugin itself is pairing/connecting right now   → accept
 *   - Remote controls and other HID devices that are not gamepads
 *     (paired, not in cooldown)                                    → auto-accept
 *   - Otherwise, once migrated, the persisted autoconnect status resolves the request
 *     (BtAuthCallbacks::getConnectAutoResponse): enabled → accept, disabled/unset → reject,
 *     without notifying the client.
 *   - Otherwise (not migrated) → escalate to the client via onConnectionRequest.
 *     Audio output devices only wait AUDIO_AUTH_TIMEOUT_MS for the client's answer
 *     (BTMgr: 200 ms), gamepads GAMEPAD_AUTH_TIMEOUT_MS (BTMgr: 2 s); every other type
 *     waits AUTH_TIMEOUT_SECONDS.
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
    static constexpr int AUDIO_AUTH_TIMEOUT_MS = 200;
    static constexpr int GAMEPAD_AUTH_TIMEOUT_MS = 2000;
    // LE gamepads connect without an authorization request, so they are asked after connecting (BTMgr: 20 s).
    static constexpr int LE_GAMEPAD_AUTH_TIMEOUT_MS = 20000;

    enum class ConnectDecision {
        AcceptedAuto,      // Resolved by the persisted autoconnect status; the caller must connect the device.
        AcceptedByClient,  // Accepted by the client, which is responsible for connecting the device.
        Rejected,
        TimedOut           // The client did not answer in time.
    };

    explicit AuthBridge(DeviceRegistry& registry, BtAuthCallbacks&& callbacks)
        : m_registry(registry)
        , m_callbacks(std::move(callbacks))
    {}

    // Called by SDK authManagerCallback. May block for AUTH_TIMEOUT_SECONDS on escalated cases.
    bool onAuthRequest(bluetooth::AuthorisationType type, std::shared_ptr<bluetooth::Device> device);

    // Called by setEventResponseWrapper to resolve a pending auth decision.
    // mac is the device's Bluetooth MAC address string.
    bool onRespondToEvent(const std::string& mac, bool accepted);

    // Decides a connection to a device the plugin was not asked to connect, either an incoming request
    // or the startup reconnect. Resolves from the persisted autoconnect status when available, otherwise
    // asks the client and waits up to clientTimeout for its answer.
    ConnectDecision decideConnection(std::shared_ptr<bluetooth::Device> device,
                                     std::chrono::milliseconds clientTimeout);

    // Device type for the device, re-classified from its current properties if the cached type is unknown.
    std::string resolveDeviceType(const std::shared_ptr<bluetooth::Device>& device, const std::string& handleStr);

    static bool isAudioOutputType(const std::string& deviceType);
    // LE gamepad, identified by its BLE appearance as BTMgr does.
    static bool isGamepad(const std::shared_ptr<bluetooth::Device>& device);
    // RDK remote control (not managed by this policy), identified by appearance or its remote service UUID.
    static bool isRemoteControl(const std::shared_ptr<bluetooth::Device>& device);

    // Set by the adapter: true while the plugin itself is pairing or connecting the device with this MAC.
    void setOperationInProgressPredicate(std::function<bool(const std::string& mac)> predicate) {
        m_isOperationInProgress = std::move(predicate);
    }

    // Resolves every waiting request as rejected so blocked callers return (used on shutdown).
    void abortPending();

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
    std::function<bool(const std::string& mac)> m_isOperationInProgress;

    mutable std::mutex m_pendingMutex;
    std::unordered_map<std::string, std::shared_ptr<PendingAuth>> m_pendingAuths;
};

} // namespace Plugin
} // namespace WPEFramework
