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

// Real bluetooth-sdk implementation of IBtAdapter, selected at runtime by
// BtAdapter::ensureImpl() alongside BtMgrAdapterImpl. Compiles against
// the external Bluetooth SDK package (production) or Tests/mocks/ (test
// builds).

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <bluetooth/Adapter.h>
#include <bluetooth/Device.h>
#include <bluetooth/Manager.h>
#include <Status.h>

#include "AuthBridge.h"
#include "DeviceRegistry.h"
#include "DeviceTypeClassifier.h"
#include "EventBridge.h"
#include "IBtAdapter.h"
#include "ScanFilterPolicy.h"

namespace WPEFramework {
namespace PluginHost { class IShell; }
namespace Plugin {

class BtSdkAdapterImpl : public IBtAdapter {
public:
    BtSdkAdapterImpl()  = default;
    ~BtSdkAdapterImpl() override = default;

    std::string init(PluginHost::IShell* service,
                     BtEventCallbacks&& eventCallbacks,
                     BtAuthCallbacks&& authCallbacks) override;
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
    bool connectDevice(const std::string& handleStr, const std::string& deviceType = "") override;
    bool disconnectDevice(const std::string& handleStr, const std::string& deviceType = "") override;

    bool getDeviceProperties(const std::string& handleStr,
                             BtDeviceProperties& props) const override;

    std::string getMacForHandle(const std::string& handleStr) const override;

    bool respondToEvent(const std::string& handleStr, const std::string& eventType, bool accepted) override;

    // Audio stubs pending T-7 (BLUETOOTH_AUDIO_SUPPORT / AUDIO_SUPPORT SDK module).
    bool               setAudioStream(long long int deviceID, const std::string& streamName) override;
    bool               setAudioControlCommand(long long int deviceID, const std::string& cmd) override;
    bool               setDeviceVolumeMute(long long int deviceID, const std::string& profile,
                                            uint8_t volume, bool mute) override;
    BtDeviceVolumeMute getDeviceVolumeMute(long long int deviceID,
                                           const std::string& profile) const override;
    BtMediaTrackInfo   getMediaTrackInfo(long long int deviceID) const override;

private:
    void onAdapterEvent(bluetooth::AdapterEvent event, bluetooth::AdapterEventData data);
    void registerDeviceEvents(std::shared_ptr<bluetooth::Device> device);
    void unregisterDeviceEvents(std::shared_ptr<bluetooth::Device> device);
    bluetooth::ScanFilter buildScanFilter(ScanFilterPolicy::Mode mode) const;
    // Drops unpaired devices from the previous scan, as the legacy stack did when a scan started.
    void pruneDiscoveredDevices();
    // True for an unpaired device the legacy BTMgr stack would not have reported for the current scan.
    bool isFilteredFromDiscovery(const std::shared_ptr<bluetooth::Device>& device, bool typeSettled = true) const;

    BtDeviceInfo deviceToInfo(std::shared_ptr<bluetooth::Device> device) const;

    // Keep SDK objects unset until init(); this type is constructed at plugin load.
    std::unique_ptr<bluetooth::Manager> m_manager;
    std::shared_ptr<bluetooth::Adapter> m_adapter;

    // Device pointer map (handle → Device); parallel to DeviceRegistry metadata.
    mutable std::mutex m_devicesMutex;
    std::unordered_map<std::string, std::shared_ptr<bluetooth::Device>> m_devicesByHandle;

    // MACs whose DeviceDisappeared event must not be forwarded (never announced, or pruned by us).
    std::mutex m_suppressedMutex;
    std::unordered_set<std::string> m_suppressedMacs;

    // Mode of the most recent startScan(); retained after stopScan() so the discovered list stays consistent.
    std::atomic<ScanFilterPolicy::Mode> m_scanMode{ScanFilterPolicy::Mode::Default};

    DeviceRegistry m_registry;
    std::unique_ptr<EventBridge> m_eventBridge;
    std::unique_ptr<AuthBridge>  m_authBridge;
};

} // namespace Plugin
} // namespace WPEFramework
