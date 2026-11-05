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
#include "BtAdapter.h"
#include "DeviceRegistry.h"
#include "BtSdkAdapterImpl.h"
#ifdef BLUETOOTH_HAS_BTMGR
#include "BtMgrAdapterImpl.h"
#endif

#include <atomic>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <UtilsLogging.h>

namespace {

bool useSdkBackend() {
    static const char* propFile = "/etc/device.properties";
    std::ifstream f(propFile);
    if (f.is_open())
    {
        std::string line;
        while (std::getline(f, line))
        {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            if (key == "BLUETOOTH_SDK_ENABLED") {
                std::string normalized;
                normalized.reserve(val.size());
                for (unsigned char ch : val) {
                    normalized.push_back(static_cast<char>(std::tolower(ch)));
                }
                // Trim trailing whitespace/newlines while preserving the intended value.
                while (!normalized.empty() && std::isspace(static_cast<unsigned char>(normalized.back()))) {
                    normalized.pop_back();
                }
                while (!normalized.empty() && std::isspace(static_cast<unsigned char>(normalized.front()))) {
                    normalized.erase(normalized.begin());
                }
                return normalized == "true";
            }
        }
    }
    return false;
}

} // namespace

namespace WPEFramework {
namespace Plugin {

std::atomic<IBtAdapter*> BtAdapter::impl{nullptr};
std::atomic<bool> BtAdapter::injected{false};

// The SDK backend is always compiled in; the BTMgr backend only when the
// legacy BTMgr library is available (BLUETOOTH_HAS_BTMGR). Only one is ever
// selected (see ensureImpl()).
#ifdef BLUETOOTH_HAS_BTMGR
static BtMgrAdapterImpl g_btMgrAdapterImpl;
#endif
static BtSdkAdapterImpl g_btSdkAdapterImpl;

void BtAdapter::setImpl(IBtAdapter* newImpl) {
    // Matches the pattern used by Btmgr::setImpl in entservices-testframework.
    assert((impl.load(std::memory_order_acquire) == nullptr) || (newImpl == nullptr));
    impl.store(newImpl, std::memory_order_release);
    injected.store(newImpl != nullptr, std::memory_order_release);
}

void BtAdapter::ensureImpl() {
    if (impl.load(std::memory_order_acquire)) {
        return;
    }

#ifdef BLUETOOTH_HAS_BTMGR
    IBtAdapter* chosen = useSdkBackend() ? static_cast<IBtAdapter*>(&g_btSdkAdapterImpl)
                                         : static_cast<IBtAdapter*>(&g_btMgrAdapterImpl);
#else
    IBtAdapter* chosen = &g_btSdkAdapterImpl;
#endif
    // Never overwrite an impl published concurrently (e.g. by init()).
    IBtAdapter* expected = nullptr;
    impl.compare_exchange_strong(expected, chosen, std::memory_order_acq_rel, std::memory_order_acquire);
}

IBtAdapter& BtAdapter::getImpl() {
    IBtAdapter* current = nullptr;
    // Retry covers a concurrent deinit() clearing impl between ensureImpl() and the load.
    while ((current = impl.load(std::memory_order_acquire)) == nullptr) {
        ensureImpl();
    }
    return *current;
}

std::string BtAdapter::init(PluginHost::IShell* service,
                            BtEventCallbacks&& evtCbs,
                            BtAuthCallbacks&& authCbs) {
    LOGINFO("BtAdapter::init: entry");
    // Only an explicit setImpl() injection skips probing; an impl chosen lazily by ensureImpl() must not.
    IBtAdapter* injectedImpl = injected.load(std::memory_order_acquire) ? impl.load(std::memory_order_acquire) : nullptr;
    if (injectedImpl) {
        // setImpl() test injection already fixed the backend; skip probing.
        LOGINFO("BtAdapter::init: impl already set");
        return injectedImpl->init(service, std::move(evtCbs), std::move(authCbs));
    }

#ifdef BLUETOOTH_HAS_BTMGR
    if (useSdkBackend()) {
#else
    {
#endif
        // librdk_bluetooth.so.1 may resolve to the real vendor SDK or to the
        // always-unavailable bundled stub, depending on what this product's
        // build/image actually installed. getDefaultAdapter() succeeding is
        // the only reliable, runtime signal that the real SDK is behind it;
        // the stub deliberately fails it so callers fall back correctly.
        std::string sdkError = g_btSdkAdapterImpl.init(service, BtEventCallbacks(evtCbs), BtAuthCallbacks(authCbs));
        LOGINFO("BtAdapter::init: sdkError: %s", sdkError.c_str());
        if (sdkError.empty()) {
            impl.store(&g_btSdkAdapterImpl, std::memory_order_release);
            return {};
        }
        // Release whatever the failed init partially constructed (e.g. the
        // Manager/D-Bus connection) before falling back to BTMgr.
        g_btSdkAdapterImpl.deinit();
#ifndef BLUETOOTH_HAS_BTMGR
        // SDK-only build: there is no legacy backend to fall back to.
        return sdkError;
#endif
    }

#ifdef BLUETOOTH_HAS_BTMGR
    impl.store(&g_btMgrAdapterImpl, std::memory_order_release);
    return g_btMgrAdapterImpl.init(service, std::move(evtCbs), std::move(authCbs));
#endif
}

void BtAdapter::deinit() {
    IBtAdapter* current = impl.load(std::memory_order_acquire);
    if (!current) {
        return;
    }
    current->deinit();
    // All instances are static; no dynamic cleanup needed. CAS so a concurrent setImpl() is not clobbered.
    impl.compare_exchange_strong(current, nullptr, std::memory_order_acq_rel, std::memory_order_acquire);
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
bool BtAdapter::queryPairedDevices(std::vector<IBtAdapter::BtDeviceInfo>& out) const { return getImpl().queryPairedDevices(out); }
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

bool BtAdapter::respondToEvent(const std::string& handleStr, const std::string& eventType, bool accepted) {
    return getImpl().respondToEvent(handleStr, eventType, accepted);
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

void BtAdapter::startAutoConnect() { getImpl().startAutoConnect(); }
void BtAdapter::setSystemPowerOn(bool on) { getImpl().setSystemPowerOn(on); }
void BtAdapter::setGamepadStandbyMode(bool standby) { getImpl().setGamepadStandbyMode(standby); }

} // namespace Plugin
} // namespace WPEFramework
