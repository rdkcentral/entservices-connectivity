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

namespace WPEFramework {
namespace Plugin {

std::string BtSdkAdapterImpl::init(PluginHost::IShell* /* service */,
                                        BtEventCallbacks&& eventCallbacks,
                                        BtAuthCallbacks&& authCallbacks) {
    LOGINFO("BtSdkAdapterImpl::init: entry");
    m_eventBridge = std::make_unique<EventBridge>(m_registry, std::move(eventCallbacks));
    m_authBridge  = std::make_unique<AuthBridge>(m_registry, std::move(authCallbacks));

    // auto authCb = [this](bluetooth::AuthorisationType type,
    //                      std::shared_ptr<bluetooth::Device> device) -> bool {
    //     return m_authBridge->onAuthRequest(type, std::move(device));
    // };

    // An empty LogRedirect would leave the SDK's Logger singleton holding a null
    // callback set, which it dereferences unconditionally on the very first log
    // line emitted from inside the Manager constructor.
    // auto logRedirect = std::make_unique<LogRedirect>(
    //     [](std::string& msg) { LOGINFO("%s", msg.c_str()); },
    //     [](std::string& msg) { LOGINFO("%s", msg.c_str()); },
    //     [](std::string& msg) { LOGWARN("%s", msg.c_str()); },
    //     [](std::string& msg) { LOGERR("%s", msg.c_str()); });

    try {
        // <pca> debug - This is throwing, simplify for now to narrow-down the problem
        // m_manager = std::make_unique<bluetooth::Manager>(
        //     bluetooth::AuthorisationMode::ExternalAuthorisation,
        //     std::move(authCb),
        //     LogLocation::LogRedirect,
        //     std::move(logRedirect)
        // );
        m_manager = std::make_unique<bluetooth::Manager>(bluetooth::AuthorisationMode::AutoAccept);
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
    Status status = m_adapter->startScan(buildScanFilter(profile));
    if (!status) {
        LOGERR("BtSdkAdapterImpl::startScan: exit with error: %s", status.get_message().c_str());
        return false;
    }
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
    device->name(info.name);
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
    for (auto& d : m_adapter->getDevices(bluetooth::DeviceState::Discovered))
        result.push_back(deviceToInfo(d));
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
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = m_devicesByHandle.find(handleStr);
    if (it == m_devicesByHandle.end()) {
        LOGERR("BtSdkAdapterImpl::pairDevice: exit with error: device handle was not found");
        return false;
    }
    Status status = it->second->pair(true);
    if (!status) {
        LOGERR("BtSdkAdapterImpl::pairDevice: exit with error: %s", status.get_message().c_str());
        return false;
    }
    LOGINFO("BtSdkAdapterImpl::pairDevice: exit successfully");
    return true;
}
bool BtSdkAdapterImpl::unpairDevice(const std::string& handleStr) {
    LOGINFO("BtSdkAdapterImpl::unpairDevice: entry handle=%s", handleStr.c_str());
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = m_devicesByHandle.find(handleStr);
    if (it == m_devicesByHandle.end()) {
        LOGERR("BtSdkAdapterImpl::unpairDevice: exit with error: device handle was not found");
        return false;
    }
    Status status = it->second->unpair();
    if (!status) {
        LOGERR("BtSdkAdapterImpl::unpairDevice: exit with error: %s", status.get_message().c_str());
        return false;
    }
    LOGINFO("BtSdkAdapterImpl::unpairDevice: exit successfully");
    return true;
}
bool BtSdkAdapterImpl::connectDevice(const std::string& handleStr, const std::string& /*deviceType*/) {
    LOGINFO("BtSdkAdapterImpl::connectDevice: entry handle=%s", handleStr.c_str());
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = m_devicesByHandle.find(handleStr);
    if (it == m_devicesByHandle.end()) {
        LOGERR("BtSdkAdapterImpl::connectDevice: exit with error: device handle was not found");
        return false;
    }
    Status status = it->second->connect(true);
    if (!status) {
        LOGERR("BtSdkAdapterImpl::connectDevice: exit with error: %s", status.get_message().c_str());
        return false;
    }
    LOGINFO("BtSdkAdapterImpl::connectDevice: exit successfully");
    return true;
}
bool BtSdkAdapterImpl::disconnectDevice(const std::string& handleStr, const std::string& /*deviceType*/) {
    LOGINFO("BtSdkAdapterImpl::disconnectDevice: entry handle=%s", handleStr.c_str());
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = m_devicesByHandle.find(handleStr);
    if (it == m_devicesByHandle.end()) {
        LOGERR("BtSdkAdapterImpl::disconnectDevice: exit with error: device handle was not found");
        return false;
    }
    Status status = it->second->disconnect(true);
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
    device->name(props.name);
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
    if (event == bluetooth::AdapterEvent::DeviceDiscovered && data.device) {
        registerDeviceEvents(data.device);
    } else if (event == bluetooth::AdapterEvent::DeviceDisappeared && data.device) {
        std::string mac;
        data.device->address(mac);
        m_registry.unregisterDevice(mac);
        std::string handle = DeviceRegistry::deriveHandle(mac);
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        m_devicesByHandle.erase(handle);
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

bluetooth::ScanFilter BtSdkAdapterImpl::buildScanFilter(const std::string& profile) const {
    LOGINFO("BtSdkAdapterImpl::buildScanFilter: entry profile=%s", profile.c_str());
    bluetooth::ScanFilter filter;
    const bool hasAudio = profile.find("LOUDSPEAKER") != std::string::npos
                       || profile.find("HEADPHONES")  != std::string::npos
                       || profile.find("WEARABLE HEADSET") != std::string::npos
                       || profile.find("HIFI AUDIO DEVICE") != std::string::npos;
    const bool hasHid   = profile.find("KEYBOARD") != std::string::npos
                       || profile.find("MOUSE")    != std::string::npos
                       || profile.find("JOYSTICK") != std::string::npos;

    if (profile.find("LE TILE") != std::string::npos ||
         profile == "LE" ||
         profile.find(", LE") != std::string::npos ||
         profile.find("LE,") != std::string::npos) {
         filter.type = bluetooth::ScanType::LeOnly;
         LOGINFO("BtSdkAdapterImpl::buildScanFilter: exit type=LeOnly uuidCount=0");
         return filter;
    }
    if (profile.find("SMARTPHONE") != std::string::npos || profile.find("TABLET") != std::string::npos) {
        filter.type = bluetooth::ScanType::ClassicOnly;
        filter.uuids = { bluetooth::Uuid(0x110a) };
        LOGINFO("BtSdkAdapterImpl::buildScanFilter: exit type=ClassicOnly uuidCount=%zu", filter.uuids.size());
        return filter;
    }
    filter.type = bluetooth::ScanType::AllDevices;
    if (hasAudio) filter.uuids.push_back(bluetooth::Uuid(0x110b));
    if (hasHid)   filter.uuids.push_back(bluetooth::Uuid(0x1124));
    LOGINFO("BtSdkAdapterImpl::buildScanFilter: exit type=AllDevices uuidCount=%zu", filter.uuids.size());
    return filter;
}

} // namespace Plugin
} // namespace WPEFramework
