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

#include "Module.h"
#include "BtSdkAdapterImpl.h"

#include <bluetooth/Appearance.h>
#include <bluetooth/Uuid.h>
#include <core/core.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <algorithm>
#include <utility>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <UtilsLogging.h>
#include <UtilsTelemetry.h>
#include <LogRedirect.h>

#include "DeviceDisplayName.h"
#include "SignalLevel.h"

namespace WPEFramework {
namespace Plugin {

// T2 markers matching the legacy BTMgr stack; the telemetry configuration on the server depends on these
// names and on the "name,class,appearance,modalias" value format.
namespace {

// BTRMGR_NAME_LEN_MAX (64) less the terminator.
constexpr size_t ADAPTER_NAME_MAX_BYTES = 63;

void sendTelemetry(const char* marker) {
    Utils::Telemetry::SendEvent(const_cast<char*>(marker), 1);
}

void sendTelemetry(const char* marker, const std::string& value) {
    Utils::Telemetry::SendEvent(const_cast<char*>(marker), const_cast<char*>(value.c_str()));
}

// "name,class,appearance,vVVVVpPPPPdDDDD", as built by BTMgr from the device details.
std::string telemetryDeviceInfo(const std::shared_ptr<bluetooth::Device>& device) {
    bluetooth::DeviceProperties props;
    device->getAllProperties(props);

    unsigned int vendor = 0, product = 0, version = 0;
    const std::string modalias = props.modalias.value_or("");
    if (std::sscanf(modalias.c_str(), "usb:v%4Xp%4Xd%4X", &vendor, &product, &version) != 3) {
        std::sscanf(modalias.c_str(), "bluetooth:v%4Xp%4Xd%4X", &vendor, &product, &version);
    }

    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%s,%u,%u,v%04Xp%04Xd%04X",
                  deviceDisplayName(device).c_str(),
                  static_cast<unsigned int>(props.classOfDevice.value_or(0)),
                  static_cast<unsigned int>(props.appearance.value_or(0)),
                  vendor, product, version);
    return buffer;
}

// The SDK Manager constructor both enumerates the adapter and registers the agent, so a throw does not
// say which one failed. Retry without an agent: if that works, agent activation was the failure.
void reportManagerConstructionFailure() {
    try {
        bluetooth::Manager probe(bluetooth::AuthorisationMode::NoAuthorisation);
        sendTelemetry("BT_ERR_AgentActFailed");
    } catch (const std::exception&) {
        sendTelemetry("BT_ERR_AdaptorNotFound");
    }
}

} // namespace

BtSdkAdapterImpl::~BtSdkAdapterImpl() {
    deinit();
    stopAutoConnectWorker();
    stopPendingWorker();
}

std::string BtSdkAdapterImpl::init(PluginHost::IShell* /* service */,
                                        BtEventCallbacks&& eventCallbacks,
                                        BtAuthCallbacks&& authCallbacks) {
    LOGINFO("BtSdkAdapterImpl::init: entry");
    // Cleared before the SDK callbacks are registered below, so events from this init are not dropped.
    m_shuttingDown = false;
    if (m_callbackGate) m_callbackGate->closeAndDrain();
    m_callbackGate = std::make_shared<CallbackGate>();
    auto gate = m_callbackGate;
    m_eventBridge = std::make_unique<EventBridge>(m_registry, std::move(eventCallbacks));
    m_authBridge  = std::make_unique<AuthBridge>(m_registry, std::move(authCallbacks));
    m_authBridge->setOperationInProgressPredicate([this](const std::string& mac) { return isOperationInProgress(mac); });
    // Reset here, as the power handler applies the current state again after the plugin is activated.
    m_gamepadStandby = false;
    m_authBridge->setGamepadStandbyPredicate([this] { return m_gamepadStandby.load(); });
    {
        // Likewise: the plugin applies the current power state again after activation.
        std::lock_guard<std::mutex> lock(m_autoConnectMutex);
        m_systemPowerOn = true;
    }

    auto authCb = [this, gate](bluetooth::AuthorisationType type,
                               std::shared_ptr<bluetooth::Device> device) -> bool {
        CallbackGate::Entry entry(*gate);
        if (!entry) return false;
        return m_authBridge->onAuthRequest(type, std::move(device));
    };

    // <pca> debug - Strip-out noise for now
    // auto logRedirect = std::make_unique<LogRedirect>(
    //     [](std::string& msg) { LOGINFO("%s", msg.c_str()); },
    //     [](std::string& msg) { LOGINFO("%s", msg.c_str()); },
    //     [](std::string& msg) { LOGWARN("%s", msg.c_str()); },
    //     [](std::string& msg) { LOGERR("%s", msg.c_str()); });
    auto logRedirect = std::make_unique<LogRedirect>(
        [](std::string& msg) { LOGINFO("%s", msg.c_str()); },
        [](std::string& msg) { LOGINFO("%s", msg.c_str()); },
        [](std::string& msg) {
            if (msg.find("D-Bus battery read failed") == std::string::npos) {
                LOGWARN("%s", msg.c_str());
            }
        },
        [](std::string& msg) {
            if (msg.find("Unable to read battery level") == std::string::npos) {
                LOGERR("%s", msg.c_str());
            }
        });
    // </pca>

    try {
        m_manager = std::make_unique<bluetooth::Manager>(
            bluetooth::AuthorisationMode::ExternalAuthorisation,
            std::move(authCb),
            LogLocation::LogRedirect,
            std::move(logRedirect)
        );
    } catch (const std::exception& e) {
        std::string error = std::string("Failed to construct Bluetooth Manager: ") + e.what();
        reportManagerConstructionFailure();
        LOGERR("BtSdkAdapterImpl::init: exit with error: %s", error.c_str());
        return error;
    }

    Status s = m_manager->getDefaultAdapter(m_adapter);
    if (!s) {
        std::string error = std::string("No Bluetooth adapter found: ") + s.get_message();
        sendTelemetry("BT_ERR_GetBTAdapterFail");
        LOGERR("BtSdkAdapterImpl::init: exit with error: %s", error.c_str());
        return error;
    }

    m_adapter->registerForEvents(
        [this, gate = std::move(gate)](bluetooth::AdapterEvent ev, bluetooth::AdapterEventData data) {
            CallbackGate::Entry entry(*gate);
            if (!entry) return;
            onAdapterEvent(ev, std::move(data));
        });

    for (auto& device : m_adapter->getDevices()) {
        registerDeviceEvents(device);
    }

    LOGINFO("BtSdkAdapterImpl::init: exit successfully");
    return "";
}

void BtSdkAdapterImpl::deinit() {
    LOGINFO("BtSdkAdapterImpl::deinit: entry");
    m_shuttingDown = true;
    // Unregister the SDK callbacks first: a late DeviceDiscovered event would otherwise restart the
    // pending worker (holdDevice) while the bridge and registry are being torn down.
    if (m_adapter) {
        for (auto& device : m_adapter->getDevices()) {
            unregisterDeviceEvents(device);
        }
        m_adapter->unregisterForEvents();
    }
    // Unregistering does not stop callbacks already running (the SDK copies them before invoking), and the
    // Manager may still hold the auth callback. Close the gate and wait so none touches the bridges or this
    // object once they are reset below; later callbacks are rejected without dereferencing the adapter.
    // Must not be called from an SDK callback.
    // A callback may be blocked in the auth bridge waiting for the client (up to AUTH_TIMEOUT_SECONDS), which
    // would stall the drain; cancel those waits first so they return promptly with a rejection.
    if (m_authBridge) m_authBridge->shutdown();
    if (m_callbackGate) m_callbackGate->closeAndDrain();
    // The workers emit through the event bridge and read devices, so they must stop before it and the adapter go.
    stopAutoConnectWorker();
    stopPendingWorker();
    m_scanning = false;
    m_scanMode = ScanFilterPolicy::Mode::Default;
    // The bridge's worker also uses SDK devices; stop it before they are released.
    if (m_eventBridge) m_eventBridge->stop();
    // Intentionally unchanged: no extra lock around these resets. Thunder quiesces JSON-RPC before Deinitialize()
    // and the SDK callbacks are unregistered and workers stopped above; a shared_mutex on every entry point is not worth the cost.
    m_adapter.reset();
    m_manager.reset();
    m_eventBridge.reset();
    m_authBridge.reset();
    m_registry.clear();
    {
        std::lock_guard<std::mutex> lock(m_suppressedMutex);
        m_suppressedMacs.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_standbyHeldMutex);
        m_standbyHeld.clear();
    }
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    m_devicesByHandle.clear();
    LOGINFO("BtSdkAdapterImpl::deinit: exit successfully");
}

// ── Adapter operations ────────────────────────────────────────────────────────

bool BtSdkAdapterImpl::getAdapterPowered(bool& powered) const {
    LOGINFO("BtSdkAdapterImpl::getAdapterPowered: entry");
    if (!m_adapter) {
        LOGERR("BtSdkAdapterImpl::getAdapterPowered: exit with error: adapter is unavailable");
        return false;
    }
    Status status = m_adapter->getPowered(powered);
    if (!status) {
        LOGERR("BtSdkAdapterImpl::getAdapterPowered: exit with error: %s", status.get_message().c_str());
        return false;
    }
    LOGINFO("BtSdkAdapterImpl::getAdapterPowered: exit powered=%d", powered);
    return true;
}
bool BtSdkAdapterImpl::setAdapterPowered(bool powered) {
    LOGINFO("BtSdkAdapterImpl::setAdapterPowered: entry powered=%d", powered);
    if (!m_adapter) {
        LOGERR("BtSdkAdapterImpl::setAdapterPowered: exit with error: adapter is unavailable");
        return false;
    }
    Status status = m_adapter->setPowered(powered);
    if (!status) {
        LOGERR("BtSdkAdapterImpl::setAdapterPowered: exit with error: %s", status.get_message().c_str());
        return false;
    }
    LOGINFO("BtSdkAdapterImpl::setAdapterPowered: exit successfully");
    return true;
}
// getAdapterName and the two discoverable operations below are a known gap versus the BTMgr backend: the SDK exposes
// no getter for the name and nothing for discoverable mode, so they stay explicit 'unsupported' results.
bool BtSdkAdapterImpl::getAdapterName(std::string& name) const {
    LOGINFO("BtSdkAdapterImpl::getAdapterName: entry");
    (void)name;
    LOGERR("BtSdkAdapterImpl::getAdapterName: exit with error: operation is unsupported");
    return false;
}
// BTMgr writes the BlueZ Alias, as Adapter::setName does, and copies at most BTRMGR_NAME_LEN_MAX - 1 bytes of the name.
bool BtSdkAdapterImpl::setAdapterName(const std::string& name) {
    LOGINFO("BtSdkAdapterImpl::setAdapterName: entry name=%s", name.c_str());
    if (!m_adapter) {
        LOGERR("BtSdkAdapterImpl::setAdapterName: exit with error: adapter is unavailable");
        return false;
    }
    Status status = m_adapter->setName(name.substr(0, ADAPTER_NAME_MAX_BYTES));
    if (!status) {
        LOGERR("BtSdkAdapterImpl::setAdapterName: exit with error: %s", status.get_message().c_str());
        return false;
    }
    LOGINFO("BtSdkAdapterImpl::setAdapterName: exit successfully");
    return true;
}
bool BtSdkAdapterImpl::isAdapterDiscoverable(bool& discoverable) const {
    LOGINFO("BtSdkAdapterImpl::isAdapterDiscoverable: entry");
    (void)discoverable;
    LOGERR("BtSdkAdapterImpl::isAdapterDiscoverable: exit with error: operation is unsupported");
    return false;
}
bool BtSdkAdapterImpl::setAdapterDiscoverable(bool discoverable, int timeoutSeconds) {
    LOGINFO("BtSdkAdapterImpl::setAdapterDiscoverable: entry discoverable=%d timeoutSeconds=%d", discoverable, timeoutSeconds);
    (void)discoverable;
    (void)timeoutSeconds;
    LOGERR("BtSdkAdapterImpl::setAdapterDiscoverable: exit with error: operation is unsupported");
    return false;
}

// ── Discovery ────────────────────────────────────────────────────────────────

bool BtSdkAdapterImpl::startScan(const std::string& profile) {
    LOGINFO("BtSdkAdapterImpl::startScan: entry profile=%s", profile.c_str());
    if (!m_adapter) {
        LOGERR("BtSdkAdapterImpl::startScan: exit with error: adapter is unavailable");
        return false;
    }
    
    const ScanFilterPolicy::Mode mode = ScanFilterPolicy::modeFromProfile(profile);
    
    // Each legacy scan starts from an empty discovered list.
    clearHeldDevices();
    pruneDiscoveredDevices();

    // Set before the scan starts so early discovery events are judged against the new mode.
    m_scanMode = mode;
    Status status = m_adapter->startScan(buildScanFilter(mode));
    if (!status) {
        sendTelemetry("BT_ERR_DiscStartFail");
        LOGERR("BtSdkAdapterImpl::startScan: exit with error: %s", status.get_message().c_str());
        return false;
    }
    m_scanning = true;
    LOGINFO("BtSdkAdapterImpl::startScan: exit successfully");
    return true;
}
bool BtSdkAdapterImpl::stopScan() {
    LOGINFO("BtSdkAdapterImpl::stopScan: entry");
    if (!m_adapter) {
        LOGERR("BtSdkAdapterImpl::stopScan: exit with error: adapter is unavailable");
        return false;
    }
    Status status = m_adapter->stopScan();
    if (!status) {
        sendTelemetry("BT_ERR_DiscStopFail");
        LOGERR("BtSdkAdapterImpl::stopScan: exit with error: %s", status.get_message().c_str());
        return false;
    }
    m_scanning = false;
    LOGINFO("BtSdkAdapterImpl::stopScan: exit successfully");
    return true;
}

// ── Device lists ─────────────────────────────────────────────────────────────

IBtAdapter::BtDeviceInfo BtSdkAdapterImpl::deviceToInfo(
    std::shared_ptr<bluetooth::Device> device) const
{
    LOGINFO("BtSdkAdapterImpl::deviceToInfo: entry");
    BtDeviceInfo info;
    std::string mac;
    device->address(mac);
    info.mac = mac;
    info.handleStr = DeviceRegistry::deriveHandle(mac);
    info.name = deviceDisplayName(device);
    info.deviceType = m_registry.getDeviceType(info.handleStr);
    const bluetooth::DeviceState state = device->state();
    info.connected  = (state == bluetooth::DeviceState::Connected);
    info.paired     = (state == bluetooth::DeviceState::Paired
                     || state == bluetooth::DeviceState::Connected);
    bluetooth::DeviceProperties props;
    if (device->getAllProperties(props)) {
        info.classOfDevice = props.classOfDevice.value_or(0);
        info.appearance    = props.appearance.value_or(0);
        info.isGamePad     = AuthBridge::isGamepad(mac, props);
        if (props.uuids.has_value()) info.uuids = props.uuids.value();
    }
    // BTMgr only ever reports BTRMGR_DEVICE_POWER_ACTIVE (1) for a connected device, never LOW or STANDBY.
    info.powerStatus = info.connected ? 1 : 0;
    LOGINFO("BtSdkAdapterImpl::deviceToInfo: exit handle=%s mac=%s", info.handleStr.c_str(), info.mac.c_str());
    return info;
}

std::vector<IBtAdapter::BtDeviceInfo> BtSdkAdapterImpl::getDiscoveredDevices() const {
    LOGINFO("BtSdkAdapterImpl::getDiscoveredDevices: entry");
    std::vector<BtDeviceInfo> result;
    if (!m_adapter) {
        sendTelemetry("BT_ERR_GetDiscListFail");
        LOGERR("BtSdkAdapterImpl::getDiscoveredDevices: exit with error: adapter is unavailable");
        return result;
    }
    for (auto& d : m_adapter->getDevices(bluetooth::DeviceState::Discovered)) {
        if (isFilteredFromDiscovery(d)) continue;
        std::string mac;
        d->address(mac);
        if (isHeld(mac)) continue;
        BtDeviceInfo info = deviceToInfo(d);
        if (!DeviceRegistry::isValidHandle(info.handleStr)) continue;
        result.push_back(std::move(info));
    }
    LOGINFO("BtSdkAdapterImpl::getDiscoveredDevices: exit count=%zu", result.size());
    return result;
}
std::vector<IBtAdapter::BtDeviceInfo> BtSdkAdapterImpl::getPairedDevices() const {
    std::vector<BtDeviceInfo> result;
    queryPairedDevices(result);
    return result;
}
bool BtSdkAdapterImpl::queryPairedDevices(std::vector<BtDeviceInfo>& out) const {
    LOGINFO("BtSdkAdapterImpl::getPairedDevices: entry");
    out.clear();
    if (!m_adapter) {
        LOGERR("BtSdkAdapterImpl::getPairedDevices: exit with error: adapter is unavailable");
        return false;
    }
    for (auto& d : m_adapter->getDevices(bluetooth::DeviceState::Paired)) {
        BtDeviceInfo info = deviceToInfo(d);
        if (!DeviceRegistry::isValidHandle(info.handleStr)) continue;
        out.push_back(std::move(info));
    }
    LOGINFO("BtSdkAdapterImpl::getPairedDevices: exit count=%zu", out.size());
    return true;
}
std::vector<IBtAdapter::BtDeviceInfo> BtSdkAdapterImpl::getConnectedDevices() const {
    LOGINFO("BtSdkAdapterImpl::getConnectedDevices: entry");
    std::vector<BtDeviceInfo> result;
    if (!m_adapter) {
        LOGERR("BtSdkAdapterImpl::getConnectedDevices: exit with error: adapter is unavailable");
        return result;
    }
    for (auto& d : m_adapter->getDevices(bluetooth::DeviceState::Connected)) {
        BtDeviceInfo info = deviceToInfo(d);
        if (!DeviceRegistry::isValidHandle(info.handleStr)) continue;
        result.push_back(std::move(info));
    }
    LOGINFO("BtSdkAdapterImpl::getConnectedDevices: exit count=%zu", result.size());
    return result;
}

// ── Device operations ─────────────────────────────────────────────────────────

std::shared_ptr<bluetooth::Device> BtSdkAdapterImpl::findDevice(const std::string& handleStr) const {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = m_devicesByHandle.find(handleStr);
    return (it != m_devicesByHandle.end()) ? it->second : nullptr;
}

BtSdkAdapterImpl::OperationScope::OperationScope(BtSdkAdapterImpl& owner, const std::shared_ptr<bluetooth::Device>& device)
    : m_owner(owner)
{
    device->address(m_mac);
    std::lock_guard<std::mutex> lock(m_owner.m_operationsMutex);
    ++m_owner.m_operationsInProgress[m_mac];
}

BtSdkAdapterImpl::OperationScope::~OperationScope() {
    std::lock_guard<std::mutex> lock(m_owner.m_operationsMutex);
    auto it = m_owner.m_operationsInProgress.find(m_mac);
    if (it != m_owner.m_operationsInProgress.end() && --it->second == 0) m_owner.m_operationsInProgress.erase(it);
}

bool BtSdkAdapterImpl::isOperationInProgress(const std::string& mac) const {
    std::lock_guard<std::mutex> lock(m_operationsMutex);
    return m_operationsInProgress.count(mac) > 0;
}

bool BtSdkAdapterImpl::pairDevice(const std::string& handleStr) {
    LOGINFO("BtSdkAdapterImpl::pairDevice: entry handle=%s", handleStr.c_str());
    // findDevice() does not hold m_devicesMutex across the blocking SDK call: device events fire while it runs.
    const std::shared_ptr<bluetooth::Device> device = findDevice(handleStr);
    if (!device) {
        LOGERR("BtSdkAdapterImpl::pairDevice: exit with error: device handle was not found");
        return false;
    }
    // Hold this device's status changes so the PAIRING_CHANGE reaches the client after the pair response.
    const EventBridge::OperationId operationId = m_eventBridge ? m_eventBridge->beginDeviceOperation(device) : 0;
    OperationScope operation(*this, device);
    Status status = device->pair(true);
    // Called before the error check: a failed request must release its held changes too.
    if (m_eventBridge) m_eventBridge->completeDeviceOperation(operationId, bluetooth::DeviceEvent::Paired, device, bool(status));
    if (!status) {
        sendTelemetry("BTPairFail_split", telemetryDeviceInfo(device));
        LOGERR("BtSdkAdapterImpl::pairDevice: exit with error: %s", status.get_message().c_str());
        return false;
    }
    sendTelemetry("BTPairSucc_split", telemetryDeviceInfo(device));
    LOGINFO("BtSdkAdapterImpl::pairDevice: exit successfully");
    return true;
}
bool BtSdkAdapterImpl::unpairDevice(const std::string& handleStr) {
    LOGINFO("BtSdkAdapterImpl::unpairDevice: entry handle=%s", handleStr.c_str());
    const std::shared_ptr<bluetooth::Device> device = findDevice(handleStr);
    if (!device) {
        // As BTMgr does for a device that is not in the paired list ("Not a Paired device").
        LOGERR("BtSdkAdapterImpl::unpairDevice: exit with error: device handle was not found");
        return false;
    }
    // Not under m_devicesMutex: the SDK emits DeviceDisappeared synchronously from unpair(), and
    // onAdapterEvent takes that mutex.
    // Read before unpair(): the device is gone from BlueZ afterwards.
    const std::string telemetryInfo = telemetryDeviceInfo(device);
    Status status = device->unpair();
    if (!status) {
        sendTelemetry("BTUnpairFail_split", telemetryInfo);
        LOGERR("BtSdkAdapterImpl::unpairDevice: exit with error: %s", status.get_message().c_str());
        return false;
    }
    sendTelemetry("BTUnpairSucc_split", telemetryInfo);
    LOGINFO("BtSdkAdapterImpl::unpairDevice: exit successfully");
    return true;
}
bool BtSdkAdapterImpl::connectDevice(const std::string& handleStr, const std::string& /*deviceType*/) {
    LOGINFO("BtSdkAdapterImpl::connectDevice: entry handle=%s", handleStr.c_str());
    const std::shared_ptr<bluetooth::Device> device = findDevice(handleStr);
    if (!device) {
        LOGERR("BtSdkAdapterImpl::connectDevice: exit with error: device handle was not found");
        return false;
    }
    // Hold this device's status changes so the CONNECTION_CHANGE reaches the client after the connect response.
    const EventBridge::OperationId operationId = m_eventBridge ? m_eventBridge->beginDeviceOperation(device) : 0;
    OperationScope operation(*this, device);
    Status status = device->connect(true);
    // A device BlueZ connected while pairing is already connected, so no new SDK event arrives.
    // Called before the error check: a failed request must release its held changes too.
    if (m_eventBridge) m_eventBridge->completeDeviceOperation(operationId, bluetooth::DeviceEvent::Connected, device, bool(status));
    if (!status) {
        sendTelemetry("BTConnFail_split", telemetryDeviceInfo(device));
        LOGERR("BtSdkAdapterImpl::connectDevice: exit with error: %s", status.get_message().c_str());
        return false;
    }
    sendTelemetry("BTConnSucc_split", telemetryDeviceInfo(device));
    LOGINFO("BtSdkAdapterImpl::connectDevice: exit successfully");
    return true;
}
bool BtSdkAdapterImpl::disconnectDevice(const std::string& handleStr, const std::string& /*deviceType*/) {
    LOGINFO("BtSdkAdapterImpl::disconnectDevice: entry handle=%s", handleStr.c_str());
    const std::shared_ptr<bluetooth::Device> device = findDevice(handleStr);
    if (!device) {
        LOGERR("BtSdkAdapterImpl::disconnectDevice: exit with error: device handle was not found");
        return false;
    }
    Status status = device->disconnect(true);
    if (!status) {
        sendTelemetry("BTDiscFail_split", telemetryDeviceInfo(device));
        LOGERR("BtSdkAdapterImpl::disconnectDevice: exit with error: %s", status.get_message().c_str());
        return false;
    }
    sendTelemetry("BTDiscSucc_split", telemetryDeviceInfo(device));
    LOGINFO("BtSdkAdapterImpl::disconnectDevice: exit successfully");
    return true;
}

bool BtSdkAdapterImpl::getDeviceProperties(const std::string& handleStr,
                                                 BtDeviceProperties& props) const {
    LOGINFO("BtSdkAdapterImpl::getDeviceProperties: entry handle=%s", handleStr.c_str());
    const std::shared_ptr<bluetooth::Device> device = findDevice(handleStr);
    if (!device) {
        LOGERR("BtSdkAdapterImpl::getDeviceProperties: exit with error: device handle was not found");
        return false;
    }
    std::string mac;
    device->address(mac);
    props.handleStr = handleStr;
    props.mac = mac;
    props.name = deviceDisplayName(device);
    props.deviceType = m_registry.getDeviceType(handleStr);
    bluetooth::DeviceProperties sdkProps;
    if (!device->getAllProperties(sdkProps)) {
        LOGERR("BtSdkAdapterImpl::getDeviceProperties: exit with error: SDK could not read device properties");
        return false;
    }
    props.classOfDevice = sdkProps.classOfDevice.value_or(0);
    props.appearance    = sdkProps.appearance.value_or(0);
    props.isGamePad     = AuthBridge::isGamepad(std::move(mac), sdkProps);
    // Mirrors BTRMGR_GetDeviceProperties: a paired device is answered from the paired list, which carries no RSSI,
    // so its rssi and signalLevel stay 0. Only a device that is not paired has them, from the scan results.
    // BlueZ's Paired flag decides, as a connected device is not necessarily paired.
    const bluetooth::DeviceState sdkState = device->state();
    const bool paired = sdkProps.paired.value_or(sdkState == bluetooth::DeviceState::Paired
                                                 || sdkState == bluetooth::DeviceState::Connected);
    if (paired) {
        props.rssi        = 0;
        props.signalLevel = 0;
    } else {
        props.rssi        = sdkProps.rssi.has_value() ? static_cast<int32_t>(sdkProps.rssi.value()) : BT_RSSI_UNKNOWN;
        props.signalLevel = signalLevelFromRssi(props.rssi);
    }
    props.batteryLevel  = sdkProps.batteryLevel.value_or(0);
    props.modalias      = sdkProps.modalias.value_or("");
    if (sdkProps.uuids.has_value())  props.uuids = sdkProps.uuids.value();
    if (sdkProps.manufacturerData.has_value() && !sdkProps.manufacturerData.value().empty())
        props.vendorId = sdkProps.manufacturerData.value().begin()->first;
    LOGINFO("BtSdkAdapterImpl::getDeviceProperties: exit successfully");
    return true;
}

std::string BtSdkAdapterImpl::getMacForHandle(const std::string& handleStr) const {
    LOGINFO("BtSdkAdapterImpl::getMacForHandle: entry handle=%s", handleStr.c_str());
    std::string mac = m_registry.getMacForHandle(handleStr);
    if (mac.empty()) {
        LOGERR("BtSdkAdapterImpl::getMacForHandle: exit with error: no MAC address is registered for the handle");
        return mac;
    }
    LOGINFO("BtSdkAdapterImpl::getMacForHandle: exit mac=%s", mac.c_str());
    return mac;
}

bool BtSdkAdapterImpl::respondToEvent(const std::string& handleStr, const std::string& /*eventType*/, bool accepted) {
    LOGINFO("BtSdkAdapterImpl::respondToEvent: entry handle=%s accepted=%d", handleStr.c_str(), accepted);
    if (!m_authBridge) {
        LOGERR("BtSdkAdapterImpl::respondToEvent: exit with error: authorization bridge is unavailable");
        return false;
    }
    const std::string mac = m_registry.getMacForHandle(handleStr);
    if (mac.empty()) {
        LOGERR("BtSdkAdapterImpl::respondToEvent: exit with error: no MAC address is registered for the handle");
        return false;
    }
    if (!m_authBridge->onRespondToEvent(mac, accepted)) {
        LOGERR("BtSdkAdapterImpl::respondToEvent: exit with error: authorization response failed");
        return false;
    }
    LOGINFO("BtSdkAdapterImpl::respondToEvent: exit successfully");
    return true;
}

// Audio stubs — implemented when BLUETOOTH_AUDIO_SUPPORT / SDK AUDIO_SUPPORT module is available (T-7).
bool BtSdkAdapterImpl::setAudioStream(long long int deviceID, const std::string& streamName) {
    LOGINFO("BtSdkAdapterImpl::setAudioStream: entry deviceID=%lld streamName=%s", deviceID, streamName.c_str());
    LOGERR("BtSdkAdapterImpl::setAudioStream: exit with error: audio support is unavailable");
    return false;
}
bool BtSdkAdapterImpl::setAudioControlCommand(long long int deviceID, const std::string& cmd) {
    LOGINFO("BtSdkAdapterImpl::setAudioControlCommand: entry deviceID=%lld command=%s", deviceID, cmd.c_str());
    LOGERR("BtSdkAdapterImpl::setAudioControlCommand: exit with error: audio support is unavailable");
    return false;
}
bool BtSdkAdapterImpl::setDeviceVolumeMute(long long int deviceID, const std::string& profile, uint8_t volume, bool mute) {
    LOGINFO("BtSdkAdapterImpl::setDeviceVolumeMute: entry deviceID=%lld profile=%s volume=%u mute=%d", deviceID, profile.c_str(), static_cast<unsigned>(volume), mute);
    LOGERR("BtSdkAdapterImpl::setDeviceVolumeMute: exit with error: audio support is unavailable");
    return false;
}
IBtAdapter::BtDeviceVolumeMute BtSdkAdapterImpl::getDeviceVolumeMute(long long int deviceID, const std::string& profile) const {
    LOGINFO("BtSdkAdapterImpl::getDeviceVolumeMute: entry deviceID=%lld profile=%s", deviceID, profile.c_str());
    LOGERR("BtSdkAdapterImpl::getDeviceVolumeMute: exit with error: audio support is unavailable");
    return {};
}
IBtAdapter::BtMediaTrackInfo BtSdkAdapterImpl::getMediaTrackInfo(long long int deviceID) const {
    LOGINFO("BtSdkAdapterImpl::getMediaTrackInfo: entry deviceID=%lld", deviceID);
    LOGERR("BtSdkAdapterImpl::getMediaTrackInfo: exit with error: audio support is unavailable");
    return {};
}

// ── Startup auto-connect ─────────────────────────────────────────────────────

// Mirrors BTRMGR_StartAudioStreamingOut_StartUp: BTMgr asks for each previously used audio output device
// (RECEIVED_EXTERNAL_CONNECT_REQUEST), waits briefly for the answer and, if the client is not up yet,
// asks again once after kAutoConnectRetrySeconds.
// Unlike BTMgr, the plugin connects the device itself once the autoconnect status accepts it, and stops after the
// first device that connects. A device the client accepts is left to the client, which is expected to follow its
// answer with its own connect request; the plugin does not connect it, as that would overlap the client's connect.
// The pass stops after the first device the client accepts. A client that never answers is overridden on the second
// pass only for a device the legacy (AS) record marks for autoconnect; an explicit rejection is always honoured.
// A connect to a device that is not up yet fails with "Host is down" (page timeout). BTMgr probes with
// l2ping (2 s apart) and retries the connect BTRMGR_CONNECT_RETRY_ATTEMPTS (2) times; here the connect
// attempt itself is the probe, so it is retried the same number of times, kAutoConnectAttemptDelaySeconds apart.
namespace {
constexpr int kAutoConnectRetrySeconds        = 40;
constexpr int kAutoConnectMaxAttempts         = 3;
constexpr int kAutoConnectAttemptDelaySeconds = 2;
}

void BtSdkAdapterImpl::startAutoConnect() {
    LOGINFO("BtSdkAdapterImpl::startAutoConnect: entry");
    std::lock_guard<std::mutex> lock(m_autoConnectMutex);
    if (!m_adapter || !m_authBridge) {
        LOGERR("BtSdkAdapterImpl::startAutoConnect: exit with error: adapter is unavailable");
        return;
    }
    if (m_autoConnectRunning) {
        LOGINFO("BtSdkAdapterImpl::startAutoConnect: exit, a pass is already running");
        return;
    }
    if (m_autoConnectThread.joinable()) m_autoConnectThread.join();
    m_autoConnectStop    = false;
    m_autoConnectRunning = true;
    m_autoConnectThread  = std::thread([this] { autoConnectWorker(); });
    LOGINFO("BtSdkAdapterImpl::startAutoConnect: exit successfully");
}

void BtSdkAdapterImpl::stopAutoConnectWorker() {
    {
        std::lock_guard<std::mutex> lock(m_autoConnectMutex);
        m_autoConnectStop = true;
    }
    m_autoConnectCv.notify_all();
    if (m_autoConnectThread.joinable()) m_autoConnectThread.join();
    std::lock_guard<std::mutex> lock(m_autoConnectMutex);
    m_autoConnectRunning = false;
}

void BtSdkAdapterImpl::setSystemPowerOn(bool on) {
    LOGINFO("BtSdkAdapterImpl::setSystemPowerOn: on=%d", on);
    {
        std::lock_guard<std::mutex> lock(m_autoConnectMutex);
        m_systemPowerOn = on;
    }
    // Ends the waits of a pass that is already running, which then stops without trying again.
    if (!on) m_autoConnectCv.notify_all();
}

void BtSdkAdapterImpl::autoConnectWorker() {
    if (runAutoConnectPass(false)) {
        bool cancelled = false;
        {
            std::unique_lock<std::mutex> lock(m_autoConnectMutex);
            cancelled = m_autoConnectCv.wait_for(lock, std::chrono::seconds(kAutoConnectRetrySeconds),
                                                 [this] { return m_autoConnectStop || !m_systemPowerOn; });
        }
        if (!cancelled) runAutoConnectPass(true);
    }
    std::lock_guard<std::mutex> lock(m_autoConnectMutex);
    m_autoConnectRunning = false;
}

bool BtSdkAdapterImpl::runAutoConnectPass(bool secondAttempt) {
    LOGINFO("entry: secondAttempt=%d", secondAttempt);
    auto cancelRequested = [this] {
        std::lock_guard<std::mutex> lock(m_autoConnectMutex);
        return m_autoConnectStop || !m_systemPowerOn;
    };
    if (!m_adapter || !m_authBridge) return false;

    if (cancelRequested()) {
        LOGINFO("exit: stopped, or the box is not in power state ON");
        return false;
    }

    bool powered = false;
    Status powerStatus = m_adapter->getPowered(powered);
    if (powerStatus && !powered) {
        LOGINFO("exit: adapter is powered off");
        return false;
    }

    // Like BTMgr (nothing is streaming), skip when an audio output device is already connected.
    for (auto& device : m_adapter->getDevices(bluetooth::DeviceState::Connected)) {
        std::string mac;
        device->address(mac);
        const std::string handle = DeviceRegistry::deriveHandle(mac);
        if (!DeviceRegistry::isValidHandle(handle)) continue;
        if (AuthBridge::isAudioOutputType(m_authBridge->resolveDeviceType(device, handle))) {
            LOGINFO("exit: an audio output device is already connected");
            return false;
        }
    }

    // Only audio output devices with a persisted last-connect time are candidates, most recently used first.
    struct Candidate {
        std::shared_ptr<bluetooth::Device> device;
        std::string mac;
        std::string handle;
        std::string type;
        long long lastConnectTimeUtc;
    };
    std::vector<Candidate> candidates;
    for (auto& device : m_adapter->getDevices(bluetooth::DeviceState::Paired)) {
        std::string mac;
        device->address(mac);
        std::string handle = DeviceRegistry::deriveHandle(mac);
        if (!DeviceRegistry::isValidHandle(handle)) continue;
        std::string type = m_authBridge->resolveDeviceType(device, handle);
        LOGINFO("paired device: mac=%s handle=%s type=%s audioOutput=%d",
                mac.c_str(), handle.c_str(), type.c_str(), AuthBridge::isAudioOutputType(type));
        if (!AuthBridge::isAudioOutputType(type)) continue;

        long long lastConnectTimeUtc = 0;
        if (!m_authBridge->getLastConnectTime(handle, lastConnectTimeUtc)) continue;
        candidates.push_back({device, std::move(mac), std::move(handle), std::move(type), lastConnectTimeUtc});
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate& a, const Candidate& b) { return a.lastConnectTimeUtc > b.lastConnectTimeUtc; });

    // Connects the candidate, retrying like BTMgr. True once it is connected (also if it already was), false if
    // every attempt failed or a stop was requested.
    auto connectCandidate = [&](const Candidate& candidate) {
        bluetooth::DeviceProperties props;
        if (candidate.device->getAllProperties(props) && props.connected.value_or(false)) {
            LOGINFO("%s is already connected", candidate.mac.c_str());
            return true;
        }
        for (int attempt = 1; attempt <= kAutoConnectMaxAttempts; ++attempt) {
            if (connectDevice(candidate.handle, candidate.type)) {
                m_authBridge->onAutoConnected(candidate.handle);
                return true;
            }
            if (attempt == kAutoConnectMaxAttempts) break;
            LOGWARN("BtSdkAdapterImpl::runAutoConnectPass: connect attempt %d/%d failed for %s, retrying",
                    attempt, kAutoConnectMaxAttempts, candidate.mac.c_str());
            std::unique_lock<std::mutex> lock(m_autoConnectMutex);
            if (m_autoConnectCv.wait_for(lock, std::chrono::seconds(kAutoConnectAttemptDelaySeconds),
                                         [this] { return m_autoConnectStop || !m_systemPowerOn; })) {
                return false;
            }
        }
        return false;
    };

    for (auto& candidate : candidates) {
        if (cancelRequested()) return false;

        const std::string& mac = candidate.mac;

        const auto decision = m_authBridge->decideConnection(
            candidate.device, std::chrono::milliseconds(AuthBridge::AUDIO_AUTH_TIMEOUT_MS));
        switch (decision) {
            case AuthBridge::ConnectDecision::AcceptedAuto:
                if (connectCandidate(candidate)) return false;
                break;
            case AuthBridge::ConnectDecision::AcceptedByClient:
                LOGINFO("exit: the client accepted %s and is responsible for connecting it", mac.c_str());
                return false;
            case AuthBridge::ConnectDecision::TimedOut:
                if (!secondAttempt) {
                    LOGINFO("exit no response from the client for %s, retrying later", mac.c_str());
                    return true;
                }
                if (m_authBridge->isLegacyAutoConnect(candidate.handle)) {
                    LOGINFO("no response from the client for %s, connecting it as the legacy record marks it for autoconnect",
                            mac.c_str());
                    if (connectCandidate(candidate)) return false;
                }
                break;
            case AuthBridge::ConnectDecision::Rejected:
                break;
        }
    }
    LOGINFO("exit: nothing to connect");
    return false;
}

// ── Gamepad standby mode ─────────────────────────────────────────────────────

// Mirrors BTMgr's reaction to the box entering standby (STANDBY / LIGHT_SLEEP) and returning to ON:
//   standby: gamepads are kept from reconnecting (BTRCore_clearLEActionListForGamepads), disconnected, and
//            neither their discovery nor their incoming connections are reported or accepted;
//   on:      the suppression ends, gamepads may reconnect (BTRCore_refreshLEActionListForGamepads), and the
//            found events suppressed for gamepads that are connected again are reported (BTRMGR_ConnectGamepads_StartUp).
// Remote controls are never touched: they are needed to wake the box.
void BtSdkAdapterImpl::setGamepadStandbyMode(bool standby) {
    LOGINFO("BtSdkAdapterImpl::setGamepadStandbyMode: entry standby=%d", standby);
    std::lock_guard<std::mutex> modeLock(m_gamepadStandbyModeMutex);
    if (m_shuttingDown || !m_adapter || !m_authBridge) {
        LOGERR("BtSdkAdapterImpl::setGamepadStandbyMode: exit with error: adapter is unavailable");
        return;
    }

    // Before the loops below, so a gamepad connecting meanwhile is already rejected.
    if (standby) m_gamepadStandby = true;

    // BTMgr's auto-connect list only holds LE devices (no Class of Device).
    struct Managed { std::shared_ptr<bluetooth::Device> device; std::string handle; std::string type; bool le; };
    std::vector<Managed> managed;
    for (auto& device : managedHidDevices()) {
        std::string mac;
        device->address(mac);
        bluetooth::DeviceProperties props;
        const bool le = device->getAllProperties(props) && props.classOfDevice.value_or(0) == 0;
        std::string handle = DeviceRegistry::deriveHandle(mac);
        if (!DeviceRegistry::isValidHandle(handle)) continue;
        std::string type = m_authBridge->resolveDeviceType(device, handle);
        managed.push_back({device, std::move(handle), std::move(type), le});
    }

    if (standby) {
        for (auto& m : managed) {
            if (!m.le) continue;
            const Status status = m.device->setAutoconnectOff();
            if (!status) LOGWARN("BtSdkAdapterImpl::setGamepadStandbyMode: could not clear auto-connect for %s: %s",
                                 m.handle.c_str(), status.get_message().c_str());
        }
        for (auto& m : managed) {
            if (m.device->state() != bluetooth::DeviceState::Connected) continue;
            if (!disconnectDevice(m.handle, m.type)) {
                LOGERR("BtSdkAdapterImpl::setGamepadStandbyMode: failed to disconnect %s", m.handle.c_str());
            }
        }
    } else {
        m_gamepadStandby = false;
        for (auto& m : managed) {
            if (!m.le) continue;
            const Status status = m.device->setAutoconnectOn();
            if (!status) LOGWARN("BtSdkAdapterImpl::setGamepadStandbyMode: could not restore auto-connect for %s: %s",
                                 m.handle.c_str(), status.get_message().c_str());
        }

        std::unordered_map<std::string, bluetooth::AdapterEventData> held;
        {
            std::lock_guard<std::mutex> lock(m_standbyHeldMutex);
            held.swap(m_standbyHeld);
        }
        // Only gamepads that are connected again are reported; the rest are found again by the next scan.
        for (auto& entry : held) {
            const auto& device = entry.second.device;
            if (!device || device->state() != bluetooth::DeviceState::Connected) continue;
            if (m_eventBridge) m_eventBridge->onAdapterEvent(bluetooth::AdapterEvent::DeviceDiscovered, std::move(entry.second));
        }
    }
    LOGINFO("BtSdkAdapterImpl::setGamepadStandbyMode: exit successfully");
}

std::vector<std::shared_ptr<bluetooth::Device>> BtSdkAdapterImpl::managedHidDevices() {
    std::vector<std::shared_ptr<bluetooth::Device>> result;
    if (!m_adapter || !m_authBridge) return result;
    // Paired includes connected devices.
    for (auto& device : m_adapter->getDevices(bluetooth::DeviceState::Paired)) {
        std::string mac;
        device->address(mac);
        bluetooth::DeviceProperties props;
        if (!device->getAllProperties(props)) continue;
        const std::string handle = DeviceRegistry::deriveHandle(mac);
        if (!DeviceRegistry::isValidHandle(handle)) continue;
        const std::string type = m_authBridge->resolveDeviceType(device, handle);
        if (AuthBridge::isManagedHidDevice(type, mac, props)) result.push_back(device);
    }
    return result;
}

bool BtSdkAdapterImpl::suppressInStandby(bluetooth::AdapterEventData& event) {
    if (!m_gamepadStandby || !event.device) return false;
    std::string mac;
    event.device->address(mac);
    bluetooth::DeviceProperties props;
    if (!event.device->getAllProperties(props) || !AuthBridge::isGamepad(mac, props)) return false;
    std::lock_guard<std::mutex> lock(m_standbyHeldMutex);
    m_standbyHeld[mac] = std::move(event);
    return true;
}

// ── Private helpers ──────────────────────────────────────────────────────────

void BtSdkAdapterImpl::onAdapterEvent(bluetooth::AdapterEvent event,
                                            bluetooth::AdapterEventData data) {
    LOGINFO("BtSdkAdapterImpl::onAdapterEvent: entry event=%d hasDevice=%d", static_cast<int>(event), data.device != nullptr);
    if (m_shuttingDown) {
        LOGINFO("BtSdkAdapterImpl::onAdapterEvent: exit event dropped, shutting down");
        return;
    }
    bool filtered = false;
    if (event == bluetooth::AdapterEvent::DeviceDiscovered && data.device) {
        registerDeviceEvents(data.device);
        filtered = isFilteredFromDiscovery(data.device, /*typeSettled=*/false);
        if (filtered) {
            std::string mac;
            data.device->address(mac);
            std::lock_guard<std::mutex> lock(m_suppressedMutex);
            m_suppressedMacs.insert(std::move(mac));
        }
        if (!filtered) {
            std::string mac;
            data.device->address(mac);
            DiscoveryNameGate::Kind kind = DiscoveryNameGate::Kind::Other;
            if (!isReadyToReport(data.device, mac, 0, kind)) {
                // BlueZ has not named it yet; the worker reports it once it has a name or its wait expires.
                LOGINFO("BtSdkAdapterImpl::onAdapterEvent: exit device held until it is named mac=%s", mac.c_str());
                holdDevice(std::move(mac), std::move(data));
                return;
            }
            if (suppressInStandby(data)) {
                LOGINFO("BtSdkAdapterImpl::onAdapterEvent: exit gamepad discovery suppressed in standby");
                return;
            }
        }
    } else if (event == bluetooth::AdapterEvent::DeviceDisappeared && data.device) {
        std::string mac;
        data.device->address(mac);
        {
            // Never announced (filtered out) or removed by our own pruning: don't report it as lost.
            std::lock_guard<std::mutex> lock(m_suppressedMutex);
            filtered = (m_suppressedMacs.erase(mac) > 0);
        }
        {
            // Still held by the name gate: never announced either.
            std::lock_guard<std::mutex> lock(m_pendingMutex);
            if (m_pending.erase(mac) > 0) filtered = true;
        }
        {
            // Suppressed in standby: never announced either.
            std::lock_guard<std::mutex> lock(m_standbyHeldMutex);
            if (m_standbyHeld.erase(mac) > 0) filtered = true;
        }
        m_registry.unregisterDevice(mac);
        const std::string handle = DeviceRegistry::deriveHandle(mac);
        if (DeviceRegistry::isValidHandle(handle)) {
            std::lock_guard<std::mutex> lock(m_devicesMutex);
            m_devicesByHandle.erase(handle);
        }
    }
    if (filtered) {
        LOGINFO("BtSdkAdapterImpl::onAdapterEvent: exit device suppressed by legacy scan filter");
        return;
    }
    if (m_eventBridge) m_eventBridge->onAdapterEvent(event, std::move(data));
    LOGINFO("BtSdkAdapterImpl::onAdapterEvent: exit successfully");
}

void BtSdkAdapterImpl::registerDeviceEvents(std::shared_ptr<bluetooth::Device> device) {
    LOGINFO("BtSdkAdapterImpl::registerDeviceEvents: entry hasDevice=%d", device != nullptr);
    if (!device) {
        LOGERR("BtSdkAdapterImpl::registerDeviceEvents: exit with error: device is null");
        return;
    }
    std::string mac;
    device->address(mac);
    std::string handle = DeviceRegistry::deriveHandle(mac);
    if (!DeviceRegistry::isValidHandle(handle)) {
        LOGERR("BtSdkAdapterImpl::registerDeviceEvents: exit with error: invalid handle for mac=%s", mac.c_str());
        return;
    }

    if (m_registry.getDeviceType(handle).empty()) {
        bluetooth::DeviceProperties props;
        if (device->getAllProperties(props))
            m_registry.setDeviceType(handle,
                DeviceTypeClassifier::classify(props.appearance.value_or(0),
                                               props.classOfDevice.value_or(0),
                                               props.uuids.value_or(std::vector<std::string>{})));
    }
    m_registry.registerDevice(mac);

    {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        m_devicesByHandle[handle] = device;
    }

    device->registerForEvents(
        [this, gate = m_callbackGate](bluetooth::DeviceEvent ev, std::shared_ptr<bluetooth::Device> dev) {
            if (!gate) return;
            CallbackGate::Entry entry(*gate);
            if (!entry) return;
            if (m_eventBridge) m_eventBridge->onDeviceEvent(ev, std::move(dev));
        });
    LOGINFO("BtSdkAdapterImpl::registerDeviceEvents: exit handle=%s", handle.c_str());
}

void BtSdkAdapterImpl::unregisterDeviceEvents(std::shared_ptr<bluetooth::Device> device) {
    LOGINFO("BtSdkAdapterImpl::unregisterDeviceEvents: entry hasDevice=%d", device != nullptr);
    if (!device) {
        LOGERR("BtSdkAdapterImpl::unregisterDeviceEvents: exit with error: device is null");
        return;
    }
    device->unregisterForEvents();
    LOGINFO("BtSdkAdapterImpl::unregisterDeviceEvents: exit successfully");
}

bluetooth::ScanFilter BtSdkAdapterImpl::buildScanFilter(ScanFilterPolicy::Mode mode) const {
    LOGINFO("BtSdkAdapterImpl::buildScanFilter: entry mode=%d", static_cast<int>(mode));
    // Mirrors the legacy BTMgr/btrCore discovery mapping (BTRMGR_DEVICE_OP_TYPE_* -> BlueZ SetDiscoveryFilter):
    //   audio output, audio input -> Transport=bredr
    //   LE                        -> Transport=le + LE service UUIDs
    //   audio + HID, HID, default -> no filter
    // Classic and HID discovery must not filter on service UUIDs: most devices don't advertise them
    // in inquiry/advertising data, so BlueZ would silently drop them. Whatever BlueZ can't be asked
    // to exclude is removed afterwards by isFilteredFromDiscovery().
    bluetooth::ScanFilter filter;
    switch (mode) {
        case ScanFilterPolicy::Mode::AudioOutput:
        case ScanFilterPolicy::Mode::AudioInput:
            filter.type = bluetooth::ScanType::ClassicOnly;
            break;
        case ScanFilterPolicy::Mode::Le:
            filter.type = bluetooth::ScanType::LeOnly;
            // Same UUIDs btrCore filters LE discovery on: XBB (gdbus_bluez5 build) and Tile (dbus_bluez5 build).
            filter.uuids = {
                bluetooth::Uuid(0x0a18),
                bluetooth::Uuid(0xfeed),
                bluetooth::Uuid(0xfeec),
                bluetooth::Uuid(0xfebe),
            };
            break;
        case ScanFilterPolicy::Mode::AudioAndHid:
        case ScanFilterPolicy::Mode::Hid:
        case ScanFilterPolicy::Mode::Default:
        default:
            filter.type = bluetooth::ScanType::AllDevices;
            break;
    }
    LOGINFO("BtSdkAdapterImpl::buildScanFilter: exit type=%d uuidCount=%zu",
            static_cast<int>(filter.type), filter.uuids.size());
    return filter;
}

void BtSdkAdapterImpl::pruneDiscoveredDevices() {
    // BlueZ reports the removals asynchronously, so record them before pruning
    // so the resulting DeviceDisappeared events can be recognised and swallowed.
    {
        std::lock_guard<std::mutex> lock(m_suppressedMutex);
        for (auto& d : m_adapter->getDevices(bluetooth::DeviceState::Discovered)) {
            std::string mac;
            d->address(mac);
            m_suppressedMacs.insert(std::move(mac));
        }
    }
    m_adapter->pruneDiscoveredDevices(nullptr, 0);
}

bool BtSdkAdapterImpl::isFilteredFromDiscovery(const std::shared_ptr<bluetooth::Device>& device, bool typeSettled) const {
    if (!device) return false;

    // Paired/connected devices are not scan results; they are never hidden.
    const bluetooth::DeviceState state = device->state();
    if (state == bluetooth::DeviceState::Paired || state == bluetooth::DeviceState::Connected) return false;

    std::string mac;
    std::string name;
    device->address(mac);
    device->name(name);

    uint16_t appearance = 0;
    uint32_t classOfDevice = 0;
    std::vector<std::string> uuids;
    std::string icon;
    bluetooth::DeviceProperties props;
    if (device->getAllProperties(props)) {
        appearance    = props.appearance.value_or(0);
        classOfDevice = props.classOfDevice.value_or(0);
        uuids         = props.uuids.value_or(std::vector<std::string>{});
        icon          = props.icon.value_or("");
    }

    // Classify afresh: the type cached in the registry can predate the device's properties being populated.
    std::string deviceType = DeviceTypeClassifier::classify(appearance, classOfDevice, uuids);
    if (deviceType == "UNKNOWN DEVICE") {
        deviceType = ScanFilterPolicy::deviceTypeFromIcon(icon);
    }
    const bool skip = ScanFilterPolicy::shouldSkipDiscovered(m_scanMode.load(), mac, name, appearance,
                                                             deviceType, typeSettled);
    if (skip) {
        LOGINFO("BtSdkAdapterImpl::isFilteredFromDiscovery: skipping mac=%s name=%s appearance=0x%04x cod=0x%06x icon=%s type=%s",
                mac.c_str(), name.c_str(), appearance, classOfDevice, icon.c_str(), deviceType.c_str());
    }
    return skip;
}

bool BtSdkAdapterImpl::isReadyToReport(const std::shared_ptr<bluetooth::Device>& device, const std::string& mac,
                                       int heldMs, DiscoveryNameGate::Kind& kind) const {
    kind = DiscoveryNameGate::Kind::Other;
    const bluetooth::DeviceState state = device->state();
    // Paired/connected devices are not scan results, and already have a name.
    if (state == bluetooth::DeviceState::Paired || state == bluetooth::DeviceState::Connected) return true;

    bluetooth::DeviceProperties props;
    if (!device->getAllProperties(props)) {
        // Nothing to judge the device by; do not hold it back.
        return true;
    }
    const uint32_t classOfDevice = props.classOfDevice.value_or(0);
    const std::string deviceType = DeviceTypeClassifier::classify(props.appearance.value_or(0), classOfDevice,
                                                                  props.uuids.value_or(std::vector<std::string>{}));
    kind = DiscoveryNameGate::kindFor(deviceType, classOfDevice);
    return DiscoveryNameGate::shouldRelease(props.name.value_or(""), props.alias.value_or(""), mac,
                                            deviceType, classOfDevice, heldMs);
}

void BtSdkAdapterImpl::holdDevice(const std::string& mac, bluetooth::AdapterEventData&& event) {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    // Checked under m_pendingMutex, which stopPendingWorker() also takes, so the worker cannot restart after deinit() stops it.
    if (m_shuttingDown) return;
    auto it = m_pending.find(mac);
    if (it == m_pending.end()) {
        m_pending.emplace(mac, PendingDevice{std::move(event), std::chrono::steady_clock::now(), false});
    } else {
        it->second.event = std::move(event);
    }
    if (!m_pendingThread.joinable()) {
        m_pendingStop = false;
        m_pendingThread = std::thread(&BtSdkAdapterImpl::pendingWorker, this);
    }
    m_pendingCv.notify_all();
}

bool BtSdkAdapterImpl::isHeld(const std::string& mac) const {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    return m_pending.find(mac) != m_pending.end();
}

void BtSdkAdapterImpl::clearHeldDevices() {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    m_pending.clear();
}

void BtSdkAdapterImpl::stopPendingWorker() {
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingStop = true;
        m_pending.clear();
    }
    m_pendingCv.notify_all();
    if (m_pendingThread.joinable()) m_pendingThread.join();
}

// BlueZ updates a device's properties after announcing it, and the SDK does not report property changes,
// so re-read the held devices periodically instead of waiting for btrCore's name-change event.
void BtSdkAdapterImpl::pendingWorker() {
    const auto pollInterval = std::chrono::milliseconds(DiscoveryNameGate::kPollIntervalMs);
    for (;;) {
        std::vector<std::pair<std::string, PendingDevice>> snapshot;
        {
            std::unique_lock<std::mutex> lock(m_pendingMutex);
            m_pendingCv.wait(lock, [this] { return m_pendingStop || !m_pending.empty(); });
            if (m_pendingStop) return;
            if (m_pendingCv.wait_for(lock, pollInterval, [this] { return m_pendingStop; })) return;
            snapshot.assign(m_pending.begin(), m_pending.end());
        }

        struct Polled { std::string mac; std::shared_ptr<bluetooth::Device> device; bool ready; bool timed; };
        std::vector<Polled> polled;
        const auto now = std::chrono::steady_clock::now();
        const bool scanning = m_scanning.load();
        for (auto& entry : snapshot) {
            // With no scan running BlueZ learns nothing new; only the waits that expire by themselves matter.
            if (!scanning && !entry.second.timed) continue;
            const auto heldMs = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.second.since).count());
            DiscoveryNameGate::Kind kind = DiscoveryNameGate::Kind::Other;
            const bool ready = isReadyToReport(entry.second.event.device, entry.first, heldMs, kind);
            polled.push_back({entry.first, entry.second.event.device, ready, kind != DiscoveryNameGate::Kind::Other});
        }

        std::vector<bluetooth::AdapterEventData> released;
        {
            std::lock_guard<std::mutex> lock(m_pendingMutex);
            for (auto& p : polled) {
                auto it = m_pending.find(p.mac);
                // Gone, or replaced by a newer discovery of the same MAC, while it was being read.
                if (it == m_pending.end() || it->second.event.device != p.device) continue;
                if (p.ready) {
                    released.push_back(std::move(it->second.event));
                    m_pending.erase(it);
                } else {
                    it->second.timed = p.timed;
                }
            }
        }
        for (auto& event : released) releaseDevice(std::move(event));
    }
}

void BtSdkAdapterImpl::releaseDevice(bluetooth::AdapterEventData&& event) {
    const std::shared_ptr<bluetooth::Device> device = event.device;
    // Removed from BlueZ while it was being released.
    if (!device || device->state() == bluetooth::DeviceState::Invalid) return;

    std::string mac;
    device->address(mac);

    // The type cached when the device was first seen predates its properties; classify again now they are populated.
    bluetooth::DeviceProperties props;
    if (device->getAllProperties(props)) {
        const std::string type = DeviceTypeClassifier::classify(props.appearance.value_or(0),
                                                                props.classOfDevice.value_or(0),
                                                                props.uuids.value_or(std::vector<std::string>{}));
        const std::string handle = DeviceRegistry::deriveHandle(mac);
        if (type != "UNKNOWN DEVICE" && DeviceRegistry::isValidHandle(handle)) m_registry.setDeviceType(handle, type);
    }

    // The scan filter could not judge the type yet when the device was first seen.
    if (isFilteredFromDiscovery(device, /*typeSettled=*/true)) {
        std::lock_guard<std::mutex> lock(m_suppressedMutex);
        m_suppressedMacs.insert(std::move(mac));
        return;
    }

    if (suppressInStandby(event)) {
        LOGINFO("BtSdkAdapterImpl::releaseDevice: gamepad discovery suppressed in standby mac=%s", mac.c_str());
        return;
    }

    LOGINFO("BtSdkAdapterImpl::releaseDevice: reporting held device mac=%s", mac.c_str());
    if (m_eventBridge) m_eventBridge->onAdapterEvent(bluetooth::AdapterEvent::DeviceDiscovered, std::move(event));
}

} // namespace Plugin
} // namespace WPEFramework
