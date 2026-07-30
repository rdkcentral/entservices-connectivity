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

// Real bluetooth-sdk implementation of IBtSdkAdapter.
// This header includes SDK headers and must NOT be compiled in test builds.
// BtSdkAdapterRealImpl.cpp is excluded from BLUETOOTH_PLUGIN_SOURCES when
// RDK_SERVICES_L1_TEST is defined.

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <bluetooth/Adapter.h>
#include <bluetooth/Device.h>
#include <bluetooth/Manager.h>
#include <Status.h>

#include "AuthBridge.h"
#include "DeviceRegistry.h"
#include "DeviceTypeClassifier.h"
#include "EventBridge.h"
#include "IBtSdkAdapter.h"

namespace WPEFramework {
namespace PluginHost { class IShell; }
namespace Plugin {

class BtSdkAdapterRealImpl : public IBtSdkAdapter {
public:
    BtSdkAdapterRealImpl()  = default;
    ~BtSdkAdapterRealImpl() override = default;

    std::string init(PluginHost::IShell* service,
                     BtEventCallbacks eventCallbacks,
                     BtAuthCallbacks  authCallbacks) override;
    void deinit() override;

    bool getAdapterPowered(bool& powered) const override;
    bool setAdapterPowered(bool powered) override;
    bool getAdapterName(std::string& name) const override;
    bool setAdapterName(const std::string& name) override;
    bool isAdapterDiscoverable(bool& discoverable) const override;
    bool setAdapterDiscoverable(bool discoverable, int timeoutSeconds) override;

    bool startScan(const std::string& profile) override;
    bool stopScan() override;

    std::vector<BtDeviceInfo> getDiscoveredDevices() const override;
    std::vector<BtDeviceInfo> getPairedDevices()     const override;
    std::vector<BtDeviceInfo> getConnectedDevices()  const override;

    bool pairDevice(const std::string& handleStr) override;
    bool unpairDevice(const std::string& handleStr) override;
    bool connectDevice(const std::string& handleStr) override;
    bool disconnectDevice(const std::string& handleStr) override;

    bool getDeviceProperties(const std::string& handleStr,
                             BtDeviceProperties& props) const override;

    std::string getMacForHandle(const std::string& handleStr) const override;

    void respondToEvent(const std::string& mac, bool accepted) override;

private:
    void onAdapterEvent(bluetooth::AdapterEvent event, bluetooth::AdapterEventData data);
    void registerDeviceEvents(std::shared_ptr<bluetooth::Device> device);
    void unregisterDeviceEvents(std::shared_ptr<bluetooth::Device> device);
    bluetooth::ScanFilter buildScanFilter(const std::string& profile) const;

    BtDeviceInfo deviceToInfo(std::shared_ptr<bluetooth::Device> device) const;

    std::unique_ptr<bluetooth::Manager> m_manager;
    std::shared_ptr<bluetooth::Adapter> m_adapter;

    // Device pointer map (handle → Device); parallel to DeviceRegistry metadata.
    mutable std::mutex m_devicesMutex;
    std::unordered_map<std::string, std::shared_ptr<bluetooth::Device>> m_devicesByHandle;

    DeviceRegistry m_registry;
    std::unique_ptr<EventBridge> m_eventBridge;
    std::unique_ptr<AuthBridge>  m_authBridge;
};

} // namespace Plugin
} // namespace WPEFramework
