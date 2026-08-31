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

#include "BtSdkCApiAdapterImpl.h"

#include "DeviceTypeClassifier.h"

namespace WPEFramework {
namespace Plugin {

namespace {
// Category::HumanInterfaceDevice (0x03c0) | SubCategory::Gamepad (0x0004).
constexpr uint16_t kAppearanceHidGamepad = 0x03c4;
} // namespace

std::string BtSdkCApiAdapterImpl::init(PluginHost::IShell* /* service */,
                                        BtEventCallbacks&& eventCallbacks,
                                        BtAuthCallbacks&& authCallbacks) {
    if (!m_api || m_api->abiVersion != BTSDK_CAPI_ABI_VERSION) {
        return "Bluetooth SDK C API table missing or ABI version mismatch";
    }
    m_eventCallbacks = std::move(eventCallbacks);
    m_authCallbacks  = std::move(authCallbacks);

    m_manager = m_api->ManagerCreate(&BtSdkCApiAdapterImpl::onAuthRequestThunk, this);
    if (!m_manager) {
        return "Failed to create Bluetooth SDK manager";
    }

    if (m_api->ManagerGetDefaultAdapter(m_manager, &m_adapter) != BTSDK_OK || !m_adapter) {
        return "No Bluetooth adapter found";
    }

    m_api->AdapterRegisterEvents(m_adapter, &BtSdkCApiAdapterImpl::onAdapterEventThunk, this);
    return "";
}

void BtSdkCApiAdapterImpl::deinit() {
    if (m_api && m_adapter) {
        m_api->AdapterUnregisterEvents(m_adapter);
        m_adapter = nullptr;
    }
    if (m_api && m_manager) {
        m_api->ManagerDestroy(m_manager);
        m_manager = nullptr;
    }
    m_registry.clear();
}

// ── Adapter operations ────────────────────────────────────────────────────────

bool BtSdkCApiAdapterImpl::getAdapterPowered(bool& powered) const {
    if (!m_api || !m_adapter) return false;
    int value = 0;
    if (m_api->AdapterGetPowered(m_adapter, &value) != BTSDK_OK) return false;
    powered = (value != 0);
    return true;
}
bool BtSdkCApiAdapterImpl::setAdapterPowered(bool powered) {
    if (!m_api || !m_adapter) return false;
    return m_api->AdapterSetPowered(m_adapter, powered ? 1 : 0) == BTSDK_OK;
}
// Not in the draft facade; matches BtSdkAdapterImpl's existing unsupported status (T-1.3).
bool BtSdkCApiAdapterImpl::getAdapterName(std::string&) const { return false; }
bool BtSdkCApiAdapterImpl::setAdapterName(const std::string&) { return false; }
bool BtSdkCApiAdapterImpl::isAdapterDiscoverable(bool&) const { return false; }
bool BtSdkCApiAdapterImpl::setAdapterDiscoverable(bool, int) { return false; }

bool BtSdkCApiAdapterImpl::startScan(const std::string& profile) {
    if (!m_api || !m_adapter) return false;
    // Draft facade takes the raw profile string; porting BtSdkAdapterImpl's
    // buildScanFilter() logic (LE/Classic + UUID filters) needs a structured
    // filter parameter, to be added once the SDK team scopes it.
    return m_api->AdapterStartScan(m_adapter, profile.c_str()) == BTSDK_OK;
}
bool BtSdkCApiAdapterImpl::stopScan() {
    if (!m_api || !m_adapter) return false;
    return m_api->AdapterStopScan(m_adapter) == BTSDK_OK;
}

IBtAdapter::BtDeviceInfo BtSdkCApiAdapterImpl::toDeviceInfo(const BtSdkDeviceInfo& info) const {
    BtDeviceInfo out;
    out.mac       = info.mac ? info.mac : "";
    out.handleStr = DeviceRegistry::deriveHandle(out.mac);
    out.name      = info.name ? info.name : "";
    out.classOfDevice = info.classOfDevice;
    out.appearance    = info.appearance;
    out.uuids.reserve(info.uuidCount > 0 ? static_cast<size_t>(info.uuidCount) : 0);
    for (int i = 0; i < info.uuidCount; ++i) {
        if (info.uuids && info.uuids[i]) out.uuids.emplace_back(info.uuids[i]);
    }
    out.deviceType = DeviceTypeClassifier::classify(out.appearance, out.classOfDevice, out.uuids);
    out.isGamePad  = (out.appearance == kAppearanceHidGamepad);
    out.paired     = (info.paired != 0);
    out.connected  = (info.connected != 0);

    m_registry.registerDevice(out.mac);
    m_registry.setDeviceType(out.handleStr, out.deviceType);
    return out;
}

std::vector<IBtAdapter::BtDeviceInfo> BtSdkCApiAdapterImpl::getDevices(BtSdkDeviceState state) const {
    std::vector<BtDeviceInfo> result;
    if (!m_api || !m_adapter) return result;

    // Draft facade returns into a caller-allocated array; revisit the bound
    // with the SDK team if device lists can exceed this in practice.
    constexpr int kMaxDevices = 64;
    BtSdkDeviceInfo raw[kMaxDevices];
    int count = 0;
    if (m_api->AdapterGetDevices(m_adapter, state, raw, kMaxDevices, &count) != BTSDK_OK) {
        return result;
    }
    result.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        result.push_back(toDeviceInfo(raw[i]));
    }
    return result;
}

std::vector<IBtAdapter::BtDeviceInfo> BtSdkCApiAdapterImpl::getDiscoveredDevices() const {
    return getDevices(BTSDK_DEVICE_STATE_DISCOVERED);
}
std::vector<IBtAdapter::BtDeviceInfo> BtSdkCApiAdapterImpl::getPairedDevices() const {
    return getDevices(BTSDK_DEVICE_STATE_PAIRED);
}
std::vector<IBtAdapter::BtDeviceInfo> BtSdkCApiAdapterImpl::getConnectedDevices() const {
    return getDevices(BTSDK_DEVICE_STATE_CONNECTED);
}

// ── Device operations ─────────────────────────────────────────────────────────

bool BtSdkCApiAdapterImpl::pairDevice(const std::string& handleStr) {
    if (!m_api || !m_adapter) return false;
    const std::string mac = m_registry.getMacForHandle(handleStr);
    if (mac.empty()) return false;
    return m_api->DevicePair(m_adapter, mac.c_str()) == BTSDK_OK;
}
bool BtSdkCApiAdapterImpl::unpairDevice(const std::string& handleStr) {
    if (!m_api || !m_adapter) return false;
    const std::string mac = m_registry.getMacForHandle(handleStr);
    if (mac.empty()) return false;
    return m_api->DeviceUnpair(m_adapter, mac.c_str()) == BTSDK_OK;
}
bool BtSdkCApiAdapterImpl::connectDevice(const std::string& handleStr, const std::string& deviceType) {
    if (!m_api || !m_adapter) return false;
    const std::string mac = m_registry.getMacForHandle(handleStr);
    if (mac.empty()) return false;
    return m_api->DeviceConnect(m_adapter, mac.c_str(), deviceType.c_str()) == BTSDK_OK;
}
bool BtSdkCApiAdapterImpl::disconnectDevice(const std::string& handleStr, const std::string& deviceType) {
    if (!m_api || !m_adapter) return false;
    const std::string mac = m_registry.getMacForHandle(handleStr);
    if (mac.empty()) return false;
    return m_api->DeviceDisconnect(m_adapter, mac.c_str(), deviceType.c_str()) == BTSDK_OK;
}

bool BtSdkCApiAdapterImpl::getDeviceProperties(const std::string& handleStr,
                                                BtDeviceProperties& props) const {
    if (!m_api || !m_adapter) return false;
    const std::string mac = m_registry.getMacForHandle(handleStr);
    if (mac.empty()) return false;

    BtSdkDeviceInfo raw{};
    if (m_api->DeviceGetProperties(m_adapter, mac.c_str(), &raw) != BTSDK_OK) return false;

    const BtDeviceInfo info = toDeviceInfo(raw);
    props.handleStr     = handleStr;
    props.mac           = info.mac;
    props.name          = info.name;
    props.deviceType    = info.deviceType;
    props.isGamePad     = info.isGamePad;
    props.classOfDevice = info.classOfDevice;
    props.appearance    = info.appearance;
    props.uuids         = info.uuids;
    // rssi/signalLevel/vendorId/batteryLevel/modalias: not yet in the draft facade.
    return true;
}

std::string BtSdkCApiAdapterImpl::getMacForHandle(const std::string& handleStr) const {
    return m_registry.getMacForHandle(handleStr);
}

bool BtSdkCApiAdapterImpl::respondToEvent(const std::string& mac, bool accepted) {
    std::shared_ptr<PendingAuth> pending;
    {
        std::lock_guard<std::mutex> lock(m_pendingAuthMutex);
        auto it = m_pendingAuths.find(mac);
        if (it == m_pendingAuths.end()) return false;
        pending = it->second;
    }
    {
        std::lock_guard<std::mutex> lock(pending->mutex);
        pending->accepted = accepted;
        pending->resolved = true;
    }
    pending->cv.notify_all();
    return true;
}

// Audio stubs — implemented when BLUETOOTH_AUDIO_SUPPORT / SDK AUDIO_SUPPORT module is available (T-7).
bool BtSdkCApiAdapterImpl::setAudioStream(long long int, const std::string&)                         { return false; }
bool BtSdkCApiAdapterImpl::setAudioControlCommand(long long int, const std::string&)                  { return false; }
bool BtSdkCApiAdapterImpl::setDeviceVolumeMute(long long int, const std::string&, uint8_t, bool)      { return false; }
IBtAdapter::BtDeviceVolumeMute BtSdkCApiAdapterImpl::getDeviceVolumeMute(long long int, const std::string&) const { return {}; }
IBtAdapter::BtMediaTrackInfo   BtSdkCApiAdapterImpl::getMediaTrackInfo(long long int) const           { return {}; }

// ── Private helpers ──────────────────────────────────────────────────────────

// static
void BtSdkCApiAdapterImpl::onAdapterEventThunk(void* userdata, BtSdkAdapterEvent event,
                                                const BtSdkDeviceInfo* device) {
    auto* self = static_cast<BtSdkCApiAdapterImpl*>(userdata);
    if (self) self->handleAdapterEvent(event, device);
}

// static
int BtSdkCApiAdapterImpl::onAuthRequestThunk(void* userdata, BtSdkAuthorisationType type,
                                              const char* mac, const char* deviceType) {
    auto* self = static_cast<BtSdkCApiAdapterImpl*>(userdata);
    if (!self || !mac) return 0;
    return self->handleAuthRequest(type, mac, deviceType ? deviceType : "") ? 1 : 0;
}

void BtSdkCApiAdapterImpl::handleAdapterEvent(BtSdkAdapterEvent event, const BtSdkDeviceInfo* device) {
    // TODO: forward to m_eventCallbacks (onStatusChanged/onDiscoveredDevice/etc.),
    // mirroring EventBridge, once the facade's event data shape is finalized.
    (void)event;
    if (device && device->mac) {
        m_registry.registerDevice(device->mac);
    }
}

bool BtSdkCApiAdapterImpl::handleAuthRequest(BtSdkAuthorisationType type, const std::string& mac,
                                              const std::string& deviceType) {
    // TODO: port AuthBridge's auto-accept policy (paired audio/HID devices skip
    // escalation); this draft always escalates to the client.
    auto pending = std::make_shared<PendingAuth>();
    {
        std::lock_guard<std::mutex> lock(m_pendingAuthMutex);
        m_pendingAuths[mac] = pending;
    }

    const std::string handleStr = m_registry.getHandleForMac(mac);
    const std::string devId = handleStr.empty() ? DeviceRegistry::deriveHandle(mac) : handleStr;

    if (type == BTSDK_AUTH_PAIRING_REQUEST) {
        if (m_authCallbacks.onPairingRequest) {
            m_authCallbacks.onPairingRequest(devId, "", deviceType, 0, mac, "", false, 0);
        }
    } else if (m_authCallbacks.onConnectionRequest) {
        m_authCallbacks.onConnectionRequest(devId, "", deviceType, 0, mac, "");
    }

    std::unique_lock<std::mutex> lock(pending->mutex);
    const bool timedOut = !pending->cv.wait_for(lock, std::chrono::seconds(AUTH_TIMEOUT_SECONDS),
                                                 [&pending] { return pending->resolved; });
    {
        std::lock_guard<std::mutex> lk(m_pendingAuthMutex);
        m_pendingAuths.erase(mac);
    }
    return !timedOut && pending->accepted;
}

} // namespace Plugin
} // namespace WPEFramework
