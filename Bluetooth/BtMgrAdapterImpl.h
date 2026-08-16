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
// Compiled only when BTMGR is found at CMake configure time (BTMgr fallback path).
// No bluetooth-sdk headers here; no SDK types in this translation unit.

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
    bool connectDevice(const std::string& handleStr, const std::string& deviceType = "") override;
    bool disconnectDevice(const std::string& handleStr, const std::string& deviceType = "") override;

    bool getDeviceProperties(const std::string& handleStr,
                             BtDeviceProperties& props) const override;

    std::string getMacForHandle(const std::string& handleStr) const override;
    bool respondToEvent(const std::string& mac, bool accepted) override;

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
    // Derives the stable numeric handle string from a MAC address.
    // Mirrors DeviceRegistry::deriveHandle (strtoll base-16 over colons-stripped MAC).
    static std::string deriveHandle(const std::string& mac);

    // Maps BTRMGR device operation type from a profile string.
    static int deviceOpTypeFromProfile(const std::string& profile);
    static bool isAudioOutputDeviceType(const std::string& deviceType);
    static bool isAudioInputDeviceType(const std::string& deviceType);

    // Static IARM event callback — routes to s_instance.
    static int staticEventCallback(const char* owner, int eventId,
                                   void* data, size_t len);

    void onEvent(void* data, size_t len);

    bool respondToEvent(const std::string& mac, int eventType, bool accepted);

    void cacheHandleToMac(const std::string& handleStr, const std::string& mac) const;

    BtEventCallbacks m_evtCbs;
    BtAuthCallbacks  m_authCbs;

    mutable std::mutex                                       m_mapMutex;
    mutable std::unordered_map<std::string, std::string>    m_handleToMac;  // handle → MAC
    mutable std::unordered_map<std::string, std::string>    m_macToHandle;  // MAC → handle

    // Pending respondToEvent state (one at a time, matches BTMgr semantics).
    mutable std::mutex  m_pendingMutex;
    std::string         m_pendingMac;
    int                 m_pendingEventType{0};  // BTRMGR_Events_t value

    static BtMgrAdapterImpl* s_instance;
};

} // namespace Plugin
} // namespace WPEFramework
