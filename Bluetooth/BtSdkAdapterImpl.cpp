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
#include <UtilsLogging.h>
#include <LogRedirect.h>

namespace WPEFramework {
namespace Plugin {

std::string BtSdkAdapterImpl::init(PluginHost::IShell* /* service */,
                                        BtEventCallbacks&& eventCallbacks,
                                        BtAuthCallbacks&& authCallbacks) {
    printf("*** _DEBUG: BtSdkAdapterImpl::init: entry\n");
    m_eventBridge = std::make_unique<EventBridge>(m_registry, std::move(eventCallbacks));
    m_authBridge  = std::make_unique<AuthBridge>(m_registry, std::move(authCallbacks));

    auto authCb = [this](bluetooth::AuthorisationType type,
                         std::shared_ptr<bluetooth::Device> device) -> bool {
        return m_authBridge->onAuthRequest(type, std::move(device));
    };

    // TODO(INV-2): Confirm at integration time whether Thunder supplies a D-Bus event
    // loop that sdbus-c++ can attach to, or whether Manager needs a dedicated thread.
    try {
        m_manager = std::make_unique<bluetooth::Manager>(
            bluetooth::AuthorisationMode::ExternalAuthorisation,
            std::move(authCb),
            LogLocation::LogRedirect,
            std::unique_ptr<LogRedirect>{}
        );
    } catch (const std::exception& e) {
        return std::string("Failed to construct Bluetooth Manager: ") + e.what();
    }

    Status s = m_manager->getDefaultAdapter(m_adapter);
    if (!s) {
        return std::string("No Bluetooth adapter found: ") + s.get_message();
    }

    m_adapter->registerForEvents(
        [this](bluetooth::AdapterEvent ev, bluetooth::AdapterEventData data) {
            onAdapterEvent(ev, std::move(data));
        });

    for (auto& device : m_adapter->getDevices()) {
        registerDeviceEvents(device);
    }

    return "";
}

void BtSdkAdapterImpl::deinit() {
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
}

// ── Adapter operations ────────────────────────────────────────────────────────

bool BtSdkAdapterImpl::getAdapterPowered(bool& powered) const {
    if (!m_adapter) return false;
    return static_cast<bool>(m_adapter->getPowered(powered));
}
bool BtSdkAdapterImpl::setAdapterPowered(bool powered) {
    if (!m_adapter) return false;
    return static_cast<bool>(m_adapter->setPowered(powered));
}
bool BtSdkAdapterImpl::getAdapterName(std::string& name) const {
    (void)name;
    return false;
}
bool BtSdkAdapterImpl::setAdapterName(const std::string& name) {
    (void)name;
    return false;
}
bool BtSdkAdapterImpl::isAdapterDiscoverable(bool& discoverable) const {
    (void)discoverable;
    return false;
}
bool BtSdkAdapterImpl::setAdapterDiscoverable(bool discoverable, int timeoutSeconds) {
    (void)discoverable;
    (void)timeoutSeconds;
    return false;
}

// ── Discovery ────────────────────────────────────────────────────────────────

bool BtSdkAdapterImpl::startScan(const std::string& profile) {
    if (!m_adapter) return false;
    return static_cast<bool>(m_adapter->startScan(buildScanFilter(profile)));
}
bool BtSdkAdapterImpl::stopScan() {
    if (!m_adapter) return false;
    return static_cast<bool>(m_adapter->stopScan());
}

// ── Device lists ─────────────────────────────────────────────────────────────

IBtAdapter::BtDeviceInfo BtSdkAdapterImpl::deviceToInfo(
    std::shared_ptr<bluetooth::Device> device) const
{
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
    return info;
}

std::vector<IBtAdapter::BtDeviceInfo> BtSdkAdapterImpl::getDiscoveredDevices() const {
    std::vector<BtDeviceInfo> result;
    if (!m_adapter) return result;
    for (auto& d : m_adapter->getDevices(bluetooth::DeviceState::Discovered))
        result.push_back(deviceToInfo(d));
    return result;
}
std::vector<IBtAdapter::BtDeviceInfo> BtSdkAdapterImpl::getPairedDevices() const {
    std::vector<BtDeviceInfo> result;
    if (!m_adapter) return result;
    for (auto& d : m_adapter->getDevices(bluetooth::DeviceState::Paired))
        result.push_back(deviceToInfo(d));
    return result;
}
std::vector<IBtAdapter::BtDeviceInfo> BtSdkAdapterImpl::getConnectedDevices() const {
    std::vector<BtDeviceInfo> result;
    if (!m_adapter) return result;
    for (auto& d : m_adapter->getDevices(bluetooth::DeviceState::Connected))
        result.push_back(deviceToInfo(d));
    return result;
}

// ── Device operations ─────────────────────────────────────────────────────────

bool BtSdkAdapterImpl::pairDevice(const std::string& handleStr) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = m_devicesByHandle.find(handleStr);
    if (it == m_devicesByHandle.end()) return false;
    return static_cast<bool>(it->second->pair(true));
}
bool BtSdkAdapterImpl::unpairDevice(const std::string& handleStr) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = m_devicesByHandle.find(handleStr);
    if (it == m_devicesByHandle.end()) return false;
    return static_cast<bool>(it->second->unpair());
}
bool BtSdkAdapterImpl::connectDevice(const std::string& handleStr, const std::string& /*deviceType*/) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = m_devicesByHandle.find(handleStr);
    if (it == m_devicesByHandle.end()) return false;
    return static_cast<bool>(it->second->connect(true));
}
bool BtSdkAdapterImpl::disconnectDevice(const std::string& handleStr, const std::string& /*deviceType*/) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = m_devicesByHandle.find(handleStr);
    if (it == m_devicesByHandle.end()) return false;
    return static_cast<bool>(it->second->disconnect(true));
}

bool BtSdkAdapterImpl::getDeviceProperties(const std::string& handleStr,
                                                 BtDeviceProperties& props) const {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = m_devicesByHandle.find(handleStr);
    if (it == m_devicesByHandle.end()) return false;
    auto device = it->second;
    std::string mac;
    device->address(mac);
    props.handleStr = handleStr;
    props.mac = std::move(mac);
    device->name(props.name);
    props.deviceType = m_registry.getDeviceType(handleStr);
    bluetooth::DeviceProperties sdkProps;
    if (!device->getAllProperties(sdkProps)) return false;
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
    return true;
}

std::string BtSdkAdapterImpl::getMacForHandle(const std::string& handleStr) const {
    return m_registry.getMacForHandle(handleStr);
}

bool BtSdkAdapterImpl::respondToEvent(const std::string& mac, bool accepted) {
    return m_authBridge && m_authBridge->onRespondToEvent(mac, accepted);
}

// Audio stubs — implemented when BLUETOOTH_AUDIO_SUPPORT / SDK AUDIO_SUPPORT module is available (T-7).
bool BtSdkAdapterImpl::setAudioStream(long long int, const std::string&)                         { return false; }
bool BtSdkAdapterImpl::setAudioControlCommand(long long int, const std::string&)                  { return false; }
bool BtSdkAdapterImpl::setDeviceVolumeMute(long long int, const std::string&, uint8_t, bool)      { return false; }
IBtAdapter::BtDeviceVolumeMute BtSdkAdapterImpl::getDeviceVolumeMute(long long int, const std::string&) const { return {}; }
IBtAdapter::BtMediaTrackInfo   BtSdkAdapterImpl::getMediaTrackInfo(long long int) const           { return {}; }

// ── Private helpers ──────────────────────────────────────────────────────────

void BtSdkAdapterImpl::onAdapterEvent(bluetooth::AdapterEvent event,
                                            bluetooth::AdapterEventData data) {
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
}

void BtSdkAdapterImpl::registerDeviceEvents(std::shared_ptr<bluetooth::Device> device) {
    if (!device) return;
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
}

void BtSdkAdapterImpl::unregisterDeviceEvents(std::shared_ptr<bluetooth::Device> device) {
    if (!device) return;
    device->unregisterForEvents();
}

bluetooth::ScanFilter BtSdkAdapterImpl::buildScanFilter(const std::string& profile) const {
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
         return filter;
    }
    if (profile.find("SMARTPHONE") != std::string::npos || profile.find("TABLET") != std::string::npos) {
        filter.type = bluetooth::ScanType::ClassicOnly;
        filter.uuids = { bluetooth::Uuid(0x110a) };
        return filter;
    }
    filter.type = bluetooth::ScanType::AllDevices;
    if (hasAudio) filter.uuids.push_back(bluetooth::Uuid(0x110b));
    if (hasHid)   filter.uuids.push_back(bluetooth::Uuid(0x1124));
    return filter;
}

} // namespace Plugin
} // namespace WPEFramework
