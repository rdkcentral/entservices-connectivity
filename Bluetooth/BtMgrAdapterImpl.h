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

// BTMgr implementation of IBtAdapter.

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "IBtAdapter.h"

namespace WPEFramework {
namespace PluginHost { class IShell; }
namespace Plugin {

class BtMgrAdapterImpl : public IBtAdapter {
public:
    BtMgrAdapterImpl()  = default;
    ~BtMgrAdapterImpl() override = default;

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

    bool               setAudioStream(long long int deviceID,
                                       const std::string& streamName) override;
    bool               setAudioControlCommand(long long int deviceID,
                                               const std::string& cmd) override;
    bool               setDeviceVolumeMute(long long int deviceID,
                                            const std::string& profile,
                                            uint8_t volume, bool mute) override;
    BtDeviceVolumeMute getDeviceVolumeMute(long long int deviceID,
                                           const std::string& profile) const override;
    BtMediaTrackInfo   getMediaTrackInfo(long long int deviceID) const override;

private:
    // Maps BTRMGR device operation type from a profile string.
    static int deviceOpTypeFromProfile(const std::string& profile);

    void onEvent(void* data, size_t len);

    void cacheHandleToMac(const std::string& handleStr, const std::string& mac) const;

    BtEventCallbacks m_evtCbs;
    BtAuthCallbacks  m_authCbs;

    mutable std::mutex                                       m_mapMutex;
    mutable std::unordered_map<std::string, std::string>    m_handleToMac;  // handle → MAC

    static std::atomic<BtMgrAdapterImpl*> s_instance;
};

} // namespace Plugin
} // namespace WPEFramework
