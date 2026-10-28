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

#include "AuthBridge.h"

#include <core/core.h>
#include <UtilsLogging.h>

namespace WPEFramework {
namespace Plugin {

namespace {

bool isHidType(const std::string& deviceType) {
    return deviceType == "HUMAN INTERFACE DEVICE";
}

} // namespace

// Device types the BTMgr connection policy treats as audio output (mirrors btrMgr_ConnectionInAuthenticationCb).
bool AuthBridge::isAudioOutputType(const std::string& deviceType) {
    return deviceType == "HEADPHONES"
        || deviceType == "LOUDSPEAKER"
        || deviceType == "WEARABLE HEADSET"
        || deviceType == "HANDSFREE"
        || deviceType == "HIFI AUDIO DEVICE";
}

std::string AuthBridge::resolveDeviceType(const std::shared_ptr<bluetooth::Device>& device, const std::string& handleStr) {
    std::string type = m_registry.getDeviceType(handleStr);
    if (!device || (!type.empty() && type != "UNKNOWN DEVICE")) return type;

    // The type cached when the device was first seen can predate its properties (e.g. a paired device at boot).
    bluetooth::DeviceProperties props;
    if (device->getAllProperties(props)) {
        const std::string classified = DeviceTypeClassifier::classify(props.appearance.value_or(0),
                                                                      props.classOfDevice.value_or(0),
                                                                      props.uuids.value_or(std::vector<std::string>{}));
        if (classified != "UNKNOWN DEVICE") {
            m_registry.setDeviceType(handleStr, classified);
            type = classified;
        }
    }
    return type;
}

bool AuthBridge::onAuthRequest(bluetooth::AuthorisationType type, std::shared_ptr<bluetooth::Device> device) {
    if (!device) return false;

    std::string mac;
    device->address(mac);
    const std::string handleStr = m_registry.getHandleForMac(mac);
    const std::string deviceType = resolveDeviceType(device,
        handleStr.empty() ? DeviceRegistry::deriveHandle(mac) : handleStr);

    // ── PairingRequest: always escalate to client ────────────────────────────
    if (type == bluetooth::AuthorisationType::PairingRequest) {
        if (!m_callbacks.onPairingRequest) return true;

        std::string name;
        device->name(name);
        bluetooth::DeviceProperties props;
        uint32_t vendorId = 0;
        if (device->getAllProperties(props) && props.manufacturerData.has_value()) {
            if (!props.manufacturerData.value().empty()) {
                vendorId = props.manufacturerData.value().begin()->first;
            }
        }
        std::string deviceAddress;
        device->address(deviceAddress);
        const std::string devId = handleStr.empty() ? DeviceRegistry::deriveHandle(mac) : std::move(handleStr);

        auto pending = std::make_shared<PendingAuth>();
        {
            std::lock_guard<std::mutex> lock(m_pendingMutex);
            m_pendingAuths[mac] = pending;
        }

        m_callbacks.onPairingRequest(devId, name, deviceType, vendorId, deviceAddress, "", false, 0);

        std::unique_lock<std::mutex> lk(m_pendingMutex);
        bool timedOut = !pending->cv.wait_for(lk,
            std::chrono::seconds(AUTH_TIMEOUT_SECONDS),
            [&pending]{ return pending->resolved; });
        m_pendingAuths.erase(mac);

        if (timedOut) {
            LOGERR("Auth timeout for pairing request from %s, auto-rejecting", mac.c_str());
            return false;
        }
        return pending->accepted;
    }

    // ── ConnectionRequest ────────────────────────────────────────────────────

    // Auto-accept policy for paired HID devices that are not in cooldown.
    if (isAutoAcceptDevice(deviceType, handleStr, device)) {
        LOGINFO("AuthBridge: auto-accepting connection from %s (%s)", mac.c_str(), deviceType.c_str());
        return true;
    }

    // Everything else follows the BTMgr policy: autoconnect status when migrated, otherwise the client decides.
    // Audio output devices only get a short window, as in BTMgr; the startup reconnect retries later.
    const auto timeout = isAudioOutputType(deviceType)
        ? std::chrono::milliseconds(AUDIO_AUTH_TIMEOUT_MS)
        : std::chrono::milliseconds(std::chrono::seconds(AUTH_TIMEOUT_SECONDS));
    const ConnectDecision decision = decideConnection(device, timeout);
    return decision == ConnectDecision::AcceptedAuto || decision == ConnectDecision::AcceptedByClient;
}

AuthBridge::ConnectDecision AuthBridge::decideConnection(std::shared_ptr<bluetooth::Device> device,
                                                         std::chrono::milliseconds clientTimeout) {
    if (!device) return ConnectDecision::Rejected;

    std::string mac;
    device->address(mac);
    std::string devId = m_registry.getHandleForMac(mac);
    if (devId.empty()) devId = DeviceRegistry::deriveHandle(mac);
    const std::string deviceType = resolveDeviceType(device, devId);

    bool accept = false;
    if (m_callbacks.getConnectAutoResponse && m_callbacks.getConnectAutoResponse(devId, accept)) {
        LOGINFO("AuthBridge: connection from %s (%s) %s by autoconnect status",
                mac.c_str(), deviceType.c_str(), accept ? "accepted" : "rejected");
        return accept ? ConnectDecision::AcceptedAuto : ConnectDecision::Rejected;
    }

    if (!m_callbacks.onConnectionRequest) return ConnectDecision::Rejected;

    std::string name;
    device->name(name);
    std::string deviceAddress;
    device->address(deviceAddress);

    auto pending = std::make_shared<PendingAuth>();
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingAuths[mac] = pending;
    }

    m_callbacks.onConnectionRequest(devId, name, deviceType, 0, deviceAddress, "");

    std::unique_lock<std::mutex> lk(m_pendingMutex);
    bool timedOut = !pending->cv.wait_for(lk, clientTimeout, [&pending]{ return pending->resolved; });
    m_pendingAuths.erase(mac);

    if (timedOut) {
        LOGERR("Auth timeout for connection request from %s, auto-rejecting", mac.c_str());
        return ConnectDecision::TimedOut;
    }
    return pending->accepted ? ConnectDecision::AcceptedByClient : ConnectDecision::Rejected;
}

bool AuthBridge::onRespondToEvent(const std::string& mac, bool accepted) {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    auto it = m_pendingAuths.find(mac);
    if (it == m_pendingAuths.end()) return false;

    it->second->accepted  = accepted;
    it->second->resolved  = true;
    it->second->cv.notify_all();
    return true;
}

bool AuthBridge::isAutoAcceptDevice(const std::string& deviceType,
                                    const std::string& handleStr,
                                    std::shared_ptr<bluetooth::Device> device) const {
    if (!isHidType(deviceType)) {
        return false;
    }

    // Must be in Paired state.
    if (device->state() != bluetooth::DeviceState::Paired &&
        device->state() != bluetooth::DeviceState::Connected) {
        return false;
    }

    // Cooldown check is handled by the SDK's Agent before calling our callback.
    // If we reach here the SDK already passed the cooldown check.
    return true;
}

} // namespace Plugin
} // namespace WPEFramework
