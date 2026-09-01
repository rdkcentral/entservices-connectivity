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
#include "BtSdkAdapterImpl.h"
#include "BtMgrAdapterImpl.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

namespace {

// TODO: Wire up the real runtime config source for this flag once decided
// (Bluetooth.config / RFC / device property are all candidates). For now,
// BLUETOOTH_USE_SDK=1 in the environment is a placeholder so the selection
// path is exercisable before that decision is made. Defaults to BTMgr so
// existing platform behavior is unchanged until a platform opts in.
bool useSdkBackend() {
    const char* flag = std::getenv("BLUETOOTH_USE_SDK");
    return flag != nullptr && std::strcmp(flag, "1") == 0;
}

} // namespace

namespace WPEFramework {
namespace Plugin {

IBtAdapter* BtAdapter::impl = nullptr;

// Both backends are always linked/compiled in now; only one is ever selected
// (see ensureImpl()).
static BtMgrAdapterImpl g_btMgrAdapterImpl;
static BtSdkAdapterImpl g_btSdkAdapterImpl;

void BtAdapter::setImpl(IBtAdapter* newImpl) {
    // Matches the pattern used by Btmgr::setImpl in entservices-testframework.
    assert((impl == nullptr) || (newImpl == nullptr));
    impl = newImpl;
}

std::string BtAdapter::ensureImpl() {
    if (impl) {
        return {};
    }

    impl = useSdkBackend() ? static_cast<IBtAdapter*>(&g_btSdkAdapterImpl)
                           : static_cast<IBtAdapter*>(&g_btMgrAdapterImpl);
    return {};
}

IBtAdapter& BtAdapter::getImpl() {
    const std::string error = ensureImpl();
    assert(error.empty() && impl != nullptr);
    return *impl;
}

std::string BtAdapter::init(PluginHost::IShell* service,
                            BtEventCallbacks&& evtCbs,
                            BtAuthCallbacks&& authCbs) {
    const std::string error = ensureImpl();
    if (!error.empty()) {
        return error;
    }
    return impl->init(service, std::move(evtCbs), std::move(authCbs));
}

void BtAdapter::deinit() {
    if (!impl) {
        return;
    }
    impl->deinit();
    // All instances are static; no dynamic cleanup needed.
    impl = nullptr;
}

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
bool BtAdapter::connectDevice(const std::string& h, const std::string& dt)  { return getImpl().connectDevice(h, dt); }
bool BtAdapter::disconnectDevice(const std::string& h, const std::string& dt) { return getImpl().disconnectDevice(h, dt); }

bool BtAdapter::getDeviceProperties(const std::string& h,
                                         IBtAdapter::BtDeviceProperties& p) const {
    return getImpl().getDeviceProperties(h, p);
}

std::string BtAdapter::getMacForHandle(const std::string& h) const {
    return getImpl().getMacForHandle(h);
}

bool BtAdapter::respondToEvent(const std::string& mac, bool accepted) {
    return getImpl().respondToEvent(mac, accepted);
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
