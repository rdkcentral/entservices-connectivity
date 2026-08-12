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

#include <UtilsLogging.h>

namespace WPEFramework {
namespace Plugin {

namespace {

// Device types that get auto-accepted when paired (mirrors btrMgr_ConnectionInAuthenticationCb).
bool isAudioOutputType(const std::string& deviceType) {
    return deviceType == "HEADPHONES"
        || deviceType == "LOUDSPEAKER"
        || deviceType == "WEARABLE HEADSET"
        || deviceType == "HANDSFREE"
        || deviceType == "HIFI AUDIO DEVICE";
}

bool isHidType(const std::string& deviceType) {
    return deviceType == "HUMAN INTERFACE DEVICE";
}

} // namespace

bool AuthBridge::onAuthRequest(bluetooth::AuthorisationType type, std::shared_ptr<bluetooth::Device> device) {
    if (!device) return false;

    std::string mac;
    device->address(mac);
    const std::string handleStr = m_registry.getHandleForMac(mac);
    const std::string deviceType = m_registry.getDeviceType(
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
        const std::string devId = handleStr.empty() ? DeviceRegistry::deriveHandle(mac) : handleStr;

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

    // Auto-accept policy for audio and HID devices when paired and not in cooldown.
    if (isAutoAcceptDevice(deviceType, handleStr, device)) {
        LOGINFO("AuthBridge: auto-accepting connection from %s (%s)", mac.c_str(), deviceType.c_str());
        return true;
    }

    // Escalate smartphones, tablets, LE, and unknown types to the client.
    if (!m_callbacks.onConnectionRequest) return false;

    std::string name;
    device->name(name);
    std::string deviceAddress;
    device->address(deviceAddress);
    const std::string devId = handleStr.empty() ? DeviceRegistry::deriveHandle(mac) : handleStr;

    auto pending = std::make_shared<PendingAuth>();
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingAuths[mac] = pending;
    }

    m_callbacks.onConnectionRequest(devId, name, deviceType, 0, deviceAddress, "");

    std::unique_lock<std::mutex> lk(m_pendingMutex);
    bool timedOut = !pending->cv.wait_for(lk,
        std::chrono::seconds(AUTH_TIMEOUT_SECONDS),
        [&pending]{ return pending->resolved; });
    m_pendingAuths.erase(mac);

    if (timedOut) {
        LOGERR("Auth timeout for connection request from %s, auto-rejecting", mac.c_str());
        return false;
    }
    return pending->accepted;
}

void AuthBridge::onRespondToEvent(const std::string& mac, bool accepted) {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    auto it = m_pendingAuths.find(mac);
    if (it != m_pendingAuths.end()) {
        it->second->accepted  = accepted;
        it->second->resolved  = true;
        it->second->cv.notify_all();
    }
}

bool AuthBridge::isAutoAcceptDevice(const std::string& deviceType,
                                    const std::string& handleStr,
                                    std::shared_ptr<bluetooth::Device> device) const {
    if (!isAudioOutputType(deviceType) && !isHidType(deviceType)) {
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
