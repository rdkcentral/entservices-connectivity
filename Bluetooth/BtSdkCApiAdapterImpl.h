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

// DRAFT — IBtAdapter implementation over the proposed C-ABI facade
// (BtSdkCApi.h), pending agreement with the bluetooth-sdk team. Only depends
// on that pure-C header, so this compiles unconditionally in every build of
// the common plugin — no bluetooth-sdk C++ headers, no sdbus-c++.
//
// BtAdapter resolves the real BtSdkCApiTable at runtime (dlopen + one dlsym)
// and hands it to setApiTable() before calling init().

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "BtSdkCApi.h"
#include "DeviceRegistry.h"
#include "IBtAdapter.h"

namespace WPEFramework {
namespace PluginHost { class IShell; }
namespace Plugin {

class BtSdkCApiAdapterImpl : public IBtAdapter {
public:
    BtSdkCApiAdapterImpl()  = default;
    ~BtSdkCApiAdapterImpl() override = default;

    // Must be called with a resolved, version-checked table before init().
    void setApiTable(const BtSdkCApiTable* api) { m_api = api; }

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

    bool respondToEvent(const std::string& mac, bool accepted) override;

    // Audio: no facade entry points proposed yet (same T-7 status as BtSdkAdapterImpl).
    bool               setAudioStream(long long int deviceID, const std::string& streamName) override;
    bool               setAudioControlCommand(long long int deviceID, const std::string& cmd) override;
    bool               setDeviceVolumeMute(long long int deviceID, const std::string& profile,
                                            uint8_t volume, bool mute) override;
    BtDeviceVolumeMute getDeviceVolumeMute(long long int deviceID,
                                           const std::string& profile) const override;
    BtMediaTrackInfo   getMediaTrackInfo(long long int deviceID) const override;

private:
    static void onAdapterEventThunk(void* userdata, BtSdkAdapterEvent event, const BtSdkDeviceInfo* device);
    static int  onAuthRequestThunk(void* userdata, BtSdkAuthorisationType type,
                                   const char* mac, const char* deviceType);

    void handleAdapterEvent(BtSdkAdapterEvent event, const BtSdkDeviceInfo* device);
    bool handleAuthRequest(BtSdkAuthorisationType type, const std::string& mac, const std::string& deviceType);

    BtDeviceInfo toDeviceInfo(const BtSdkDeviceInfo& info) const;
    std::vector<BtDeviceInfo> getDevices(BtSdkDeviceState state) const;

    const BtSdkCApiTable* m_api      = nullptr;
    BtSdkManagerHandle    m_manager  = nullptr;
    BtSdkAdapterHandle    m_adapter  = nullptr;

    // Caches mac/handle/deviceType; updated from const query methods too.
    mutable DeviceRegistry m_registry;
    BtEventCallbacks  m_eventCallbacks;
    BtAuthCallbacks   m_authCallbacks;

    // Escalated pairing/connection requests block the SDK's callback thread
    // (see AuthBridge for the equivalent pattern) until respondToEvent()
    // resolves them from the JSON-RPC thread, or AUTH_TIMEOUT_SECONDS elapses.
    static constexpr int AUTH_TIMEOUT_SECONDS = 30;
    struct PendingAuth {
        std::mutex              mutex;
        std::condition_variable cv;
        bool                    resolved{false};
        bool                    accepted{false};
    };
    mutable std::mutex m_pendingAuthMutex;
    std::unordered_map<std::string, std::shared_ptr<PendingAuth>> m_pendingAuths;
};

} // namespace Plugin
} // namespace WPEFramework
