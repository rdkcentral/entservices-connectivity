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

#pragma once

#include <gmock/gmock.h>

// IBtSdkAdapter and BtSdkAdapterCallbacks live in the plugin source tree.
// The L1 test CMakeLists adds entservices-connectivity/Bluetooth to the include path.
#include "IBtSdkAdapter.h"
#include "BtSdkAdapter.h"

namespace WPEFramework {
namespace Plugin {

class BtSdkAdapterImplMock : public IBtSdkAdapter {
public:
    BtSdkAdapterImplMock() = default;
    virtual ~BtSdkAdapterImplMock() = default;

    MOCK_METHOD(std::string, init,
        (WPEFramework::PluginHost::IShell*, BtEventCallbacks, BtAuthCallbacks),
        (override));
    MOCK_METHOD(void, deinit, (), (override));

    MOCK_METHOD(bool, getAdapterPowered,  (bool&),              (const, override));
    MOCK_METHOD(bool, setAdapterPowered,  (bool),               (override));
    MOCK_METHOD(bool, getAdapterName,     (std::string&),       (const, override));
    MOCK_METHOD(bool, setAdapterName,     (const std::string&), (override));
    MOCK_METHOD(bool, isAdapterDiscoverable, (bool&),           (const, override));
    MOCK_METHOD(bool, setAdapterDiscoverable,(bool, int),       (override));

    MOCK_METHOD(bool, startScan, (const std::string&), (override));
    MOCK_METHOD(bool, stopScan,  (),                   (override));

    MOCK_METHOD(std::vector<IBtSdkAdapter::BtDeviceInfo>, getDiscoveredDevices, (), (const, override));
    MOCK_METHOD(std::vector<IBtSdkAdapter::BtDeviceInfo>, getPairedDevices,     (), (const, override));
    MOCK_METHOD(std::vector<IBtSdkAdapter::BtDeviceInfo>, getConnectedDevices,  (), (const, override));

    MOCK_METHOD(bool, pairDevice,       (const std::string&), (override));
    MOCK_METHOD(bool, unpairDevice,     (const std::string&), (override));
    MOCK_METHOD(bool, connectDevice,    (const std::string&), (override));
    MOCK_METHOD(bool, disconnectDevice, (const std::string&), (override));

    MOCK_METHOD(bool, getDeviceProperties,
        (const std::string&, IBtSdkAdapter::BtDeviceProperties&),
        (const, override));

    MOCK_METHOD(std::string, getMacForHandle, (const std::string&), (const, override));
    MOCK_METHOD(void, respondToEvent, (const std::string&, bool), (override));

    // ── Event injection helpers ──────────────────────────────────────────────
    // Call these from tests to simulate SDK events flowing into the plugin.
    // The callbacks are captured when the mock's init() is called; wire them
    // via ON_CALL(*p_btSdkMock, init(...)).WillByDefault(Invoke([this](...) {
    //     m_evtCbs = evtCbs; m_authCbs = authCbs; return ""; })).

    BtEventCallbacks m_evtCbs;
    BtAuthCallbacks  m_authCbs;

    void fireDiscoveryStarted() {
        if (m_evtCbs.onStatusChanged)
            m_evtCbs.onStatusChanged("onStatusChanged", "DISCOVERY_STARTED",
                                     "", "", "", 0, 0, false, false, false, false, false);
    }
    void fireDiscoveryStopped() {
        if (m_evtCbs.onStatusChanged)
            m_evtCbs.onStatusChanged("onStatusChanged", "DISCOVERY_COMPLETED",
                                     "", "", "", 0, 0, false, false, false, false, false);
    }
    void firePowerOn() {
        if (m_evtCbs.onStatusChanged)
            m_evtCbs.onStatusChanged("onStatusChanged", "HARDWARE_AVAILABLE",
                                     "", "", "", 0, 0, false, false, false, false, false);
    }
    void firePowerOff() {
        if (m_evtCbs.onStatusChanged)
            m_evtCbs.onStatusChanged("onStatusChanged", "SOFTWARE_DISABLED",
                                     "", "", "", 0, 0, false, false, false, false, false);
    }
    void firePairingChange(const std::string& deviceId, const std::string& name,
                           const std::string& deviceType, bool paired, bool connected) {
        if (m_evtCbs.onStatusChanged)
            m_evtCbs.onStatusChanged("onStatusChanged", "PAIRING_CHANGE",
                                     deviceId, name, deviceType, 0, 0,
                                     paired, connected, false, false, false);
    }
    void fireConnectionChange(const std::string& deviceId, const std::string& name,
                              const std::string& deviceType, bool connected) {
        if (m_evtCbs.onStatusChanged)
            m_evtCbs.onStatusChanged("onStatusChanged", "CONNECTION_CHANGE",
                                     deviceId, name, deviceType, 0, 0,
                                     true /*paired*/, connected, false, false, false);
    }
    void fireDeviceDiscovered(const std::string& deviceId, const std::string& name,
                              const std::string& deviceType) {
        if (m_evtCbs.onDiscoveredDevice)
            m_evtCbs.onDiscoveredDevice(deviceId, name, deviceType, 0, 0,
                                        false, false, "DISCOVERED");
    }
    void fireDeviceLost(const std::string& deviceId, const std::string& name,
                        const std::string& deviceType) {
        if (m_evtCbs.onDeviceLost)
            m_evtCbs.onDeviceLost(deviceId, name, deviceType, 0, 0, false);
    }
    void fireRequestFailed(const std::string& status, const std::string& deviceId,
                           const std::string& name, const std::string& deviceType) {
        if (m_evtCbs.onRequestFailed)
            m_evtCbs.onRequestFailed(status, deviceId, name, deviceType, 0, 0, true, false);
    }
};

} // namespace Plugin
} // namespace WPEFramework
