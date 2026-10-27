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

#include "BtSdkAdapterImpl.h"

#include <bluetooth/Appearance.h>
#include <bluetooth/Uuid.h>
#include <core/core.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <UtilsLogging.h>
#include <LogRedirect.h>

#include "DeviceDisplayName.h"

namespace WPEFramework {
namespace Plugin {

BtSdkAdapterImpl::~BtSdkAdapterImpl() {
    stopPendingWorker();
}

std::string BtSdkAdapterImpl::init(PluginHost::IShell* /* service */,
                                        BtEventCallbacks&& eventCallbacks,
                                        BtAuthCallbacks&& authCallbacks) {
    LOGINFO("BtSdkAdapterImpl::init: entry");
    m_eventBridge = std::make_unique<EventBridge>(m_registry, std::move(eventCallbacks));
    m_authBridge  = std::make_unique<AuthBridge>(m_registry, std::move(authCallbacks));

    // <pca> debug
    // auto authCb = [this](bluetooth::AuthorisationType type,
    //                      std::shared_ptr<bluetooth::Device> device) -> bool {
    //     return m_authBridge->onAuthRequest(type, std::move(device));
    // };
    // </pca>

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
        // <pca> debug - This is throwing, simplify for now to narrow-down the problem
        // m_manager = std::make_unique<bluetooth::Manager>(
        //     bluetooth::AuthorisationMode::ExternalAuthorisation,
        //     std::move(authCb),
        //     LogLocation::LogRedirect,
        //     std::move(logRedirect)
        // );
        m_manager = std::make_unique<bluetooth::Manager>(
            bluetooth::AuthorisationMode::AutoAccept,
            nullptr,
            LogLocation::LogRedirect,
            std::move(logRedirect)
        );
        // </pca>
    } catch (const std::exception& e) {
        const std::string error = std::string("Failed to construct Bluetooth Manager: ") + e.what();
        LOGERR("BtSdkAdapterImpl::init: exit with error: %s", error.c_str());
        return error;
    }

    Status s = m_manager->getDefaultAdapter(m_adapter);
    if (!s) {
        const std::string error = std::string("No Bluetooth adapter found: ") + s.get_message();
        LOGERR("BtSdkAdapterImpl::init: exit with error: %s", error.c_str());
        return error;
    }

    m_adapter->registerForEvents(
        [this](bluetooth::AdapterEvent ev, bluetooth::AdapterEventData data) {
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
    // The worker emits through the event bridge and reads devices, so it must stop first.
    stopPendingWorker();
    // The bridge's worker also uses SDK devices; stop it before they are released.
    if (m_eventBridge) m_eventBridge->stop();
    if (m_adapter) {
        for (auto& device : m_adapter->getDevices()) {
            unregisterDeviceEvents(device);
        }
        m_adapter->unregisterForEvents();
        m_adapter.reset();
    }
    m_manager.reset();
    m_eventBridge.reset();
    m_authBridge.reset();
    m_registry.clear();
    {
        std::lock_guard<std::mutex> lock(m_suppressedMutex);
        m_suppressedMacs.clear();
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
bool BtSdkAdapterImpl::getAdapterName(std::string& name) const {
    LOGINFO("BtSdkAdapterImpl::getAdapterName: entry");
    (void)name;
    LOGERR("BtSdkAdapterImpl::getAdapterName: exit with error: operation is unsupported");
    return false;
}
bool BtSdkAdapterImpl::setAdapterName(const std::string& name) {
    LOGINFO("BtSdkAdapterImpl::setAdapterName: entry name=%s", name.c_str());
    (void)name;
    LOGERR("BtSdkAdapterImpl::setAdapterName: exit with error: operation is unsupported");
    return false;
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
    // <pca> debug - Just AudioOutput for now to reduce discovery noise
    //const ScanFilterPolicy::Mode mode = ScanFilterPolicy::modeFromProfile(profile);
    const ScanFilterPolicy::Mode mode = ScanFilterPolicy::Mode::AudioOutput;
    // </pca>
    // Each legacy scan starts from an empty discovered list.
    clearHeldDevices();
    pruneDiscoveredDevices();
    // Set before the scan starts so early discovery events are judged against the new mode.
    m_scanMode = mode;
    Status status = m_adapter->startScan(buildScanFilter(mode));
    if (!status) {
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
    m_scanning = false;
    Status status = m_adapter->stopScan();
    if (!status) {
        LOGERR("BtSdkAdapterImpl::stopScan: exit with error: %s", status.get_message().c_str());
        return false;
    }
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
    info.connected  = (device->state() == bluetooth::DeviceState::Connected);
    info.paired     = (device->state() == bluetooth::DeviceState::Paired
                     || device->state() == bluetooth::DeviceState::Connected);
    bluetooth::DeviceProperties props;
    if (device->getAllProperties(props)) {
        info.classOfDevice = props.classOfDevice.value_or(0);
        info.appearance    = props.appearance.value_or(0);
        info.isGamePad     = (info.appearance == (static_cast<uint16_t>(bluetooth::Appearance::Category::HumanInterfaceDevice)
                            | static_cast<uint16_t>(bluetooth::Appearance::SubCategory::Gamepad)));
        if (props.uuids.has_value()) info.uuids = props.uuids.value();
    }
    LOGINFO("BtSdkAdapterImpl::deviceToInfo: exit handle=%s mac=%s", info.handleStr.c_str(), info.mac.c_str());
    return info;
}

std::vector<IBtAdapter::BtDeviceInfo> BtSdkAdapterImpl::getDiscoveredDevices() const {
    LOGINFO("BtSdkAdapterImpl::getDiscoveredDevices: entry");
    std::vector<BtDeviceInfo> result;
    if (!m_adapter) {
        LOGERR("BtSdkAdapterImpl::getDiscoveredDevices: exit with error: adapter is unavailable");
        return result;
    }
    for (auto& d : m_adapter->getDevices(bluetooth::DeviceState::Discovered)) {
        if (isFilteredFromDiscovery(d)) continue;
        std::string mac;
        d->address(mac);
        if (isHeld(mac)) continue;
        result.push_back(deviceToInfo(d));
    }
    LOGINFO("BtSdkAdapterImpl::getDiscoveredDevices: exit count=%zu", result.size());
    return result;
}
std::vector<IBtAdapter::BtDeviceInfo> BtSdkAdapterImpl::getPairedDevices() const {
    LOGINFO("BtSdkAdapterImpl::getPairedDevices: entry");
    std::vector<BtDeviceInfo> result;
    if (!m_adapter) {
        LOGERR("BtSdkAdapterImpl::getPairedDevices: exit with error: adapter is unavailable");
        return result;
    }
    for (auto& d : m_adapter->getDevices(bluetooth::DeviceState::Paired))
        result.push_back(deviceToInfo(d));
    LOGINFO("BtSdkAdapterImpl::getPairedDevices: exit count=%zu", result.size());
    return result;
}
std::vector<IBtAdapter::BtDeviceInfo> BtSdkAdapterImpl::getConnectedDevices() const {
    LOGINFO("BtSdkAdapterImpl::getConnectedDevices: entry");
    std::vector<BtDeviceInfo> result;
    if (!m_adapter) {
        LOGERR("BtSdkAdapterImpl::getConnectedDevices: exit with error: adapter is unavailable");
        return result;
    }
    for (auto& d : m_adapter->getDevices(bluetooth::DeviceState::Connected))
        result.push_back(deviceToInfo(d));
    LOGINFO("BtSdkAdapterImpl::getConnectedDevices: exit count=%zu", result.size());
    return result;
}

// ── Device operations ─────────────────────────────────────────────────────────

bool BtSdkAdapterImpl::pairDevice(const std::string& handleStr) {
    LOGINFO("BtSdkAdapterImpl::pairDevice: entry handle=%s", handleStr.c_str());
    // Don't hold the lock across the blocking SDK call: device events fire while it runs.
    std::shared_ptr<bluetooth::Device> device;
    {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        auto it = m_devicesByHandle.find(handleStr);
        if (it != m_devicesByHandle.end()) device = it->second;
    }
    if (!device) {
        LOGERR("BtSdkAdapterImpl::pairDevice: exit with error: device handle was not found");
        return false;
    }
    // Hold this device's status changes so the PAIRING_CHANGE reaches the client after the pair response.
    if (m_eventBridge) m_eventBridge->beginDeviceOperation(device);
    Status status = device->pair(true);
    // Called before the error check: a failed request must release its held changes too.
    if (m_eventBridge) m_eventBridge->completeDeviceOperation(bluetooth::DeviceEvent::Paired, device, bool(status));
    if (!status) {
        LOGERR("BtSdkAdapterImpl::pairDevice: exit with error: %s", status.get_message().c_str());
        return false;
    }
    LOGINFO("BtSdkAdapterImpl::pairDevice: exit successfully");
    return true;
}
bool BtSdkAdapterImpl::unpairDevice(const std::string& handleStr) {
    LOGINFO("BtSdkAdapterImpl::unpairDevice: entry handle=%s", handleStr.c_str());
    std::shared_ptr<bluetooth::Device> device;
    {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        auto it = m_devicesByHandle.find(handleStr);
        if (it != m_devicesByHandle.end()) device = it->second;
    }
    if (!device) {
        // BlueZ has no object for this device, so there is nothing to unpair. Succeed so the caller
        // still drops its own paired-device record instead of leaving it stuck.
        LOGWARN("BtSdkAdapterImpl::unpairDevice: exit successfully: device handle was not found, treating as already unpaired");
        return true;
    }
    // Not under m_devicesMutex: the SDK emits DeviceDisappeared synchronously from unpair(), and
    // onAdapterEvent takes that mutex.
    Status status = device->unpair();
    if (!status) {
        LOGERR("BtSdkAdapterImpl::unpairDevice: exit with error: %s", status.get_message().c_str());
        return false;
    }
    LOGINFO("BtSdkAdapterImpl::unpairDevice: exit successfully");
    return true;
}
bool BtSdkAdapterImpl::connectDevice(const std::string& handleStr, const std::string& /*deviceType*/) {
    LOGINFO("BtSdkAdapterImpl::connectDevice: entry handle=%s", handleStr.c_str());
    std::shared_ptr<bluetooth::Device> device;
    {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        auto it = m_devicesByHandle.find(handleStr);
        if (it != m_devicesByHandle.end()) device = it->second;
    }
    if (!device) {
        LOGERR("BtSdkAdapterImpl::connectDevice: exit with error: device handle was not found");
        return false;
    }
    // Hold this device's status changes so the CONNECTION_CHANGE reaches the client after the connect response.
    if (m_eventBridge) m_eventBridge->beginDeviceOperation(device);
    Status status = device->connect(true);
    // A device BlueZ connected while pairing is already connected, so no new SDK event arrives.
    // Called before the error check: a failed request must release its held changes too.
    if (m_eventBridge) m_eventBridge->completeDeviceOperation(bluetooth::DeviceEvent::Connected, device, bool(status));
    if (!status) {
        LOGERR("BtSdkAdapterImpl::connectDevice: exit with error: %s", status.get_message().c_str());
        return false;
    }
    LOGINFO("BtSdkAdapterImpl::connectDevice: exit successfully");
    return true;
}
bool BtSdkAdapterImpl::disconnectDevice(const std::string& handleStr, const std::string& /*deviceType*/) {
    LOGINFO("BtSdkAdapterImpl::disconnectDevice: entry handle=%s", handleStr.c_str());
    std::shared_ptr<bluetooth::Device> device;
    {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        auto it = m_devicesByHandle.find(handleStr);
        if (it != m_devicesByHandle.end()) device = it->second;
    }
    if (!device) {
        LOGERR("BtSdkAdapterImpl::disconnectDevice: exit with error: device handle was not found");
        return false;
    }
    Status status = device->disconnect(true);
    if (!status) {
        LOGERR("BtSdkAdapterImpl::disconnectDevice: exit with error: %s", status.get_message().c_str());
        return false;
    }
    LOGINFO("BtSdkAdapterImpl::disconnectDevice: exit successfully");
    return true;
}

bool BtSdkAdapterImpl::getDeviceProperties(const std::string& handleStr,
                                                 BtDeviceProperties& props) const {
    LOGINFO("BtSdkAdapterImpl::getDeviceProperties: entry handle=%s", handleStr.c_str());
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = m_devicesByHandle.find(handleStr);
    if (it == m_devicesByHandle.end()) {
        LOGERR("BtSdkAdapterImpl::getDeviceProperties: exit with error: device handle was not found");
        return false;
    }
    auto device = it->second;
    std::string mac;
    device->address(mac);
    props.handleStr = handleStr;
    props.mac = std::move(mac);
    props.name = deviceDisplayName(device);
    props.deviceType = m_registry.getDeviceType(handleStr);
    bluetooth::DeviceProperties sdkProps;
    if (!device->getAllProperties(sdkProps)) {
        LOGERR("BtSdkAdapterImpl::getDeviceProperties: exit with error: SDK could not read device properties");
        return false;
    }
    props.classOfDevice = sdkProps.classOfDevice.value_or(0);
    props.appearance    = sdkProps.appearance.value_or(0);
    props.isGamePad     = (props.appearance == (static_cast<uint16_t>(bluetooth::Appearance::Category::HumanInterfaceDevice)
                        | static_cast<uint16_t>(bluetooth::Appearance::SubCategory::Gamepad)));
    props.rssi          = sdkProps.rssi.value_or(0);
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
    const std::string mac = m_registry.getMacForHandle(handleStr);
    if (!m_authBridge) {
        LOGERR("BtSdkAdapterImpl::respondToEvent: exit with error: authorization bridge is unavailable");
        return false;
    }
    const bool success = m_authBridge->onRespondToEvent(mac, accepted);
    if (!success) {
        LOGERR("BtSdkAdapterImpl::respondToEvent: exit with error: %s",
               mac.empty() ? "no MAC address is registered for the handle" : "authorization response failed");
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

// ── Private helpers ──────────────────────────────────────────────────────────

void BtSdkAdapterImpl::onAdapterEvent(bluetooth::AdapterEvent event,
                                            bluetooth::AdapterEventData data) {
    LOGINFO("BtSdkAdapterImpl::onAdapterEvent: entry event=%d hasDevice=%d", static_cast<int>(event), data.device != nullptr);
    bool filtered = false;
    if (event == bluetooth::AdapterEvent::DeviceDiscovered && data.device) {
        registerDeviceEvents(data.device);
        filtered = isFilteredFromDiscovery(data.device, /*typeSettled=*/false);
        if (filtered) {
            std::string mac;
            data.device->address(mac);
            std::lock_guard<std::mutex> lock(m_suppressedMutex);
            m_suppressedMacs.insert(mac);
        }
        if (!filtered) {
            std::string mac;
            data.device->address(mac);
            DiscoveryNameGate::Kind kind = DiscoveryNameGate::Kind::Other;
            if (!isReadyToReport(data.device, mac, 0, kind)) {
                // BlueZ has not named it yet; the worker reports it once it has a name or its wait expires.
                LOGINFO("BtSdkAdapterImpl::onAdapterEvent: exit device held until it is named mac=%s", mac.c_str());
                holdDevice(mac, std::move(data));
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
        m_registry.unregisterDevice(mac);
        std::string handle = DeviceRegistry::deriveHandle(mac);
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        m_devicesByHandle.erase(handle);
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
        [this](bluetooth::DeviceEvent ev, std::shared_ptr<bluetooth::Device> dev) {
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
            m_suppressedMacs.insert(mac);
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
        if (type != "UNKNOWN DEVICE") m_registry.setDeviceType(DeviceRegistry::deriveHandle(mac), type);
    }

    // The scan filter could not judge the type yet when the device was first seen.
    if (isFilteredFromDiscovery(device, /*typeSettled=*/true)) {
        std::lock_guard<std::mutex> lock(m_suppressedMutex);
        m_suppressedMacs.insert(mac);
        return;
    }

    LOGINFO("BtSdkAdapterImpl::releaseDevice: reporting held device mac=%s", mac.c_str());
    if (m_eventBridge) m_eventBridge->onAdapterEvent(bluetooth::AdapterEvent::DeviceDiscovered, std::move(event));
}

} // namespace Plugin
} // namespace WPEFramework
