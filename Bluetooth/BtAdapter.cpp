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

#include "BtAdapter.h"
#include "DeviceRegistry.h"

// In production builds, auto-construct the right backend impl.
// BLUETOOTH_USE_SDK is set by CMakeLists when BluetoothSDK is found.
// Guard prevents inclusion of backend headers in test builds.
#ifndef RDK_SERVICES_L1_TEST
#ifdef BLUETOOTH_USE_SDK
#include "BtSdkAdapterImpl.h"
#else
#include "BtMgrAdapterImpl.h"
#endif
#endif

#include <cassert>

namespace WPEFramework {
namespace Plugin {

IBtAdapter* BtAdapter::impl = nullptr;

void BtAdapter::setImpl(IBtAdapter* newImpl) {
    // Matches the pattern used by Btmgr::setImpl in entservices-testframework.
    assert((impl == nullptr) || (newImpl == nullptr));
    impl = newImpl;
}

#ifndef RDK_SERVICES_L1_TEST
#ifdef BLUETOOTH_USE_SDK
static BtSdkAdapterImpl g_btAdapterImpl;
#else
static BtMgrAdapterImpl g_btAdapterImpl;
#endif
#endif

static IBtAdapter& getImpl() {
#ifndef RDK_SERVICES_L1_TEST
    if (!BtAdapter::impl) BtAdapter::impl = &g_btAdapterImpl;
#endif
    assert(BtAdapter::impl != nullptr);
    return *BtAdapter::impl;
}

std::string BtAdapter::init(PluginHost::IShell* service,
                                BtEventCallbacks evtCbs,
                                BtAuthCallbacks  authCbs) {
    return getImpl().init(service, std::move(evtCbs), std::move(authCbs));
}
void BtAdapter::deinit() { getImpl().deinit(); }

bool BtAdapter::getAdapterPowered(bool& p) const  { return getImpl().getAdapterPowered(p); }
bool BtAdapter::setAdapterPowered(bool p)          { return getImpl().setAdapterPowered(p); }
bool BtAdapter::getAdapterName(std::string& n) const { return getImpl().getAdapterName(n); }
bool BtAdapter::setAdapterName(const std::string& n) { return getImpl().setAdapterName(n); }
bool BtAdapter::isAdapterDiscoverable(bool& d) const { return getImpl().isAdapterDiscoverable(d); }
bool BtAdapter::setAdapterDiscoverable(bool d, int t) { return getImpl().setAdapterDiscoverable(d, t); }

bool BtAdapter::startScan(const std::string& profile) { return getImpl().startScan(profile); }
bool BtAdapter::stopScan()                             { return getImpl().stopScan(); }

std::vector<IBtAdapter::BtDeviceInfo> BtAdapter::getDiscoveredDevices() const { return getImpl().getDiscoveredDevices(); }
std::vector<IBtAdapter::BtDeviceInfo> BtAdapter::getPairedDevices()     const { return getImpl().getPairedDevices(); }
std::vector<IBtAdapter::BtDeviceInfo> BtAdapter::getConnectedDevices()  const { return getImpl().getConnectedDevices(); }

bool BtAdapter::pairDevice(const std::string& h)     { return getImpl().pairDevice(h); }
bool BtAdapter::unpairDevice(const std::string& h)   { return getImpl().unpairDevice(h); }
bool BtAdapter::connectDevice(const std::string& h)  { return getImpl().connectDevice(h); }
bool BtAdapter::disconnectDevice(const std::string& h) { return getImpl().disconnectDevice(h); }

bool BtAdapter::getDeviceProperties(const std::string& h,
                                         IBtAdapter::BtDeviceProperties& p) const {
    return getImpl().getDeviceProperties(h, p);
}

std::string BtAdapter::getMacForHandle(const std::string& h) const {
    return getImpl().getMacForHandle(h);
}

void BtAdapter::respondToEvent(const std::string& mac, bool accepted) {
    getImpl().respondToEvent(mac, accepted);
}

// static — pure computation, no impl needed
std::string BtAdapter::handleForMac(const std::string& mac) {
    return DeviceRegistry::deriveHandle(mac);
}

bool BtAdapter::setAudioStream(long long int deviceID, const std::string& s)          { return getImpl().setAudioStream(deviceID, s); }
bool BtAdapter::setAudioControlCommand(long long int deviceID, const std::string& s)  { return getImpl().setAudioControlCommand(deviceID, s); }
bool BtAdapter::setDeviceVolumeMute(long long int d, const std::string& p, uint8_t v, bool m) { return getImpl().setDeviceVolumeMute(d, p, v, m); }
IBtAdapter::BtDeviceVolumeMute BtAdapter::getDeviceVolumeMute(long long int d, const std::string& p) const { return getImpl().getDeviceVolumeMute(d, p); }
IBtAdapter::BtMediaTrackInfo   BtAdapter::getMediaTrackInfo(long long int d) const    { return getImpl().getMediaTrackInfo(d); }

} // namespace Plugin
} // namespace WPEFramework
                                AuthBridge::Callbacks authCallbacks) {
    m_eventBridge = std::make_unique<EventBridge>(m_registry, std::move(eventCallbacks));
    m_authBridge  = std::make_unique<AuthBridge>(m_registry, std::move(authCallbacks));

    auto authCb = [this](bluetooth::AuthorisationType type,
                         std::shared_ptr<bluetooth::Device> device) -> bool {
        return m_authBridge->onAuthRequest(type, device);
    };

    // TODO(INV-2): Confirm at integration time whether Thunder supplies a D-Bus event
    // loop that sdbus-c++ can attach to, or whether Manager needs a dedicated thread.
    try {
        m_manager = std::make_unique<bluetooth::Manager>(
            bluetooth::AuthorisationMode::ExternalAuthorisation,
            std::move(authCb),
            LogLocation::LogRedirect,
            std::unique_ptr<LogRedirect>{} // placeholder — wire real LogRedirect when available
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

    // Register device events for devices already known at init time.
    for (auto& device : m_adapter->getDevices()) {
        registerDeviceEvents(device);
    }

    return "";
}

void BtAdapter::deinit() {
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
}

// ── Adapter operations ────────────────────────────────────────────────────────

bool BtAdapter::getAdapterPowered(bool& powered) const {
    if (!m_adapter) return false;
    return static_cast<bool>(m_adapter->getPowered(powered));
}

bool BtAdapter::setAdapterPowered(bool powered) {
    if (!m_adapter) return false;
    return static_cast<bool>(m_adapter->setPowered(powered));
}

bool BtAdapter::getAdapterName(std::string& name) const {
    if (!m_adapter) return false;
    try {
        name = m_adapter->Alias();
        return true;
    } catch (...) { return false; }
}

bool BtAdapter::setAdapterName(const std::string& name) {
    if (!m_adapter) return false;
    try {
        m_adapter->Alias(name);
        return true;
    } catch (...) { return false; }
}

bool BtAdapter::isAdapterDiscoverable(bool& discoverable) const {
    if (!m_adapter) return false;
    try {
        discoverable = m_adapter->Discoverable();
        return true;
    } catch (...) { return false; }
}

bool BtAdapter::setAdapterDiscoverable(bool discoverable, int timeoutSeconds) {
    if (!m_adapter) return false;
    try {
        m_adapter->Discoverable(discoverable);
        m_adapter->DiscoverableTimeout(static_cast<uint32_t>(timeoutSeconds > 0 ? timeoutSeconds : 0));
        return true;
    } catch (...) { return false; }
}

// ── Discovery ────────────────────────────────────────────────────────────────

bool BtAdapter::startScan(const std::string& profile) {
    if (!m_adapter) return false;
    return static_cast<bool>(m_adapter->startScan(buildScanFilter(profile)));
}

bool BtAdapter::stopScan() {
    if (!m_adapter) return false;
    return static_cast<bool>(m_adapter->stopScan());
}

// ── Device lists ─────────────────────────────────────────────────────────────

std::vector<std::shared_ptr<bluetooth::Device>> BtAdapter::getDiscoveredDevices() const {
    if (!m_adapter) return {};
    return m_adapter->getDevices(bluetooth::DeviceState::Discovered);
}

std::vector<std::shared_ptr<bluetooth::Device>> BtAdapter::getPairedDevices() const {
    if (!m_adapter) return {};
    return m_adapter->getDevices(bluetooth::DeviceState::Paired);
}

std::vector<std::shared_ptr<bluetooth::Device>> BtAdapter::getConnectedDevices() const {
    if (!m_adapter) return {};
    return m_adapter->getDevices(bluetooth::DeviceState::Connected);
}

// ── Device operations ─────────────────────────────────────────────────────────

bool BtAdapter::pairDevice(const std::string& handleStr) {
    auto device = m_registry.getDeviceByHandle(handleStr);
    if (!device) return false;
    Status s = device->pair(/* sync= */ true);
    return static_cast<bool>(s);
}

bool BtAdapter::unpairDevice(const std::string& handleStr) {
    auto device = m_registry.getDeviceByHandle(handleStr);
    if (!device) return false;
    Status s = device->unpair();
    return static_cast<bool>(s);
}

bool BtAdapter::connectDevice(const std::string& handleStr) {
    auto device = m_registry.getDeviceByHandle(handleStr);
    if (!device) return false;
    Status s = device->connect(/* sync= */ true);
    return static_cast<bool>(s);
}

bool BtAdapter::disconnectDevice(const std::string& handleStr) {
    auto device = m_registry.getDeviceByHandle(handleStr);
    if (!device) return false;
    Status s = device->disconnect(/* sync= */ true);
    return static_cast<bool>(s);
}

bool BtAdapter::getDeviceProperties(const std::string& handleStr,
                                        bluetooth::DeviceProperties& props) const {
    auto device = m_registry.getDeviceByHandle(handleStr);
    if (!device) return false;
    return static_cast<bool>(device->getAllProperties(props));
}

std::shared_ptr<bluetooth::Device> BtAdapter::getDeviceByHandle(const std::string& handleStr) const {
    return m_registry.getDeviceByHandle(handleStr);
}

void BtAdapter::respondToEvent(const std::string& mac, bool accepted) {
    if (m_authBridge) {
        m_authBridge->onRespondToEvent(mac, accepted);
    }
}

// ── Private helpers ──────────────────────────────────────────────────────────

void BtAdapter::onAdapterEvent(bluetooth::AdapterEvent event, bluetooth::AdapterEventData data) {
    if (event == bluetooth::AdapterEvent::DeviceDiscovered && data.device) {
        registerDeviceEvents(data.device);
    } else if (event == bluetooth::AdapterEvent::DeviceDisappeared && data.device) {
        std::string mac;
        data.device->address(mac);
        m_registry.unregisterDevice(mac);
    }

    if (m_eventBridge) {
        m_eventBridge->onAdapterEvent(event, std::move(data));
    }
}

void BtAdapter::registerDeviceEvents(std::shared_ptr<bluetooth::Device> device) {
    if (!device) return;

    std::string mac;
    device->address(mac);

    // Populate type cache on first registration.
    std::string handle = DeviceRegistry::deriveHandle(mac);
    if (m_registry.getDeviceType(handle).empty()) {
        bluetooth::DeviceProperties props;
        if (device->getAllProperties(props)) {
            m_registry.setDeviceType(handle, DeviceTypeClassifier::classify(props));
        }
    }

    m_registry.registerDevice(mac, device);

    device->registerForEvents(
        [this](bluetooth::DeviceEvent ev, std::shared_ptr<bluetooth::Device> dev) {
            if (m_eventBridge) {
                m_eventBridge->onDeviceEvent(ev, std::move(dev));
            }
        });
}

void BtAdapter::unregisterDeviceEvents(std::shared_ptr<bluetooth::Device> device) {
    if (!device) return;
    device->unregisterForEvents();
}

bluetooth::ScanFilter BtAdapter::buildScanFilter(const std::string& profile) const {
    bluetooth::ScanFilter filter;

    const bool hasAudio = profile.find("LOUDSPEAKER")    != std::string::npos
                       || profile.find("HEADPHONES")     != std::string::npos
                       || profile.find("WEARABLE HEADSET") != std::string::npos
                       || profile.find("HIFI AUDIO DEVICE") != std::string::npos;

    const bool hasHid   = profile.find("KEYBOARD") != std::string::npos
                       || profile.find("MOUSE")    != std::string::npos
                       || profile.find("JOYSTICK") != std::string::npos;

    if (profile.find("LE TILE") != std::string::npos || profile.find("LE") != std::string::npos) {
        filter.type = bluetooth::ScanType::LeOnly;
        return filter;
    }

    if (profile.find("SMARTPHONE") != std::string::npos || profile.find("TABLET") != std::string::npos) {
        filter.type = bluetooth::ScanType::ClassicOnly;
        filter.uuids = { bluetooth::Uuid::ServiceClasses::AudioSource };
        return filter;
    }

    if (hasAudio || hasHid || profile.find("DEFAULT") != std::string::npos || profile.empty()) {
        filter.type = bluetooth::ScanType::AllDevices;
        if (hasAudio) filter.uuids.push_back(bluetooth::Uuid::ServiceClasses::AudioSink);
        if (hasHid)   filter.uuids.push_back(bluetooth::Uuid(0x1124)); // HumanInterfaceDevice
        return filter;
    }

    // No match → scan all.
    filter.type = bluetooth::ScanType::AllDevices;
    return filter;
}

} // namespace Plugin
} // namespace WPEFramework
