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

// Thin static-impl dispatch wrapper over IBtAdapter.
//
// Follows the same pattern as entservices-testframework's Btmgr class:
//   - setImpl(mock) in test fixture setup → all methods dispatch to mock
//   - Production builds auto-construct BtSdkAdapterImpl on first init() if impl is null
//
// This header has NO bluetooth-sdk dependencies so it is safe to include in
// test builds where bluetooth-sdk is not available.

#include "IBtAdapter.h"

namespace WPEFramework {
namespace PluginHost { class IShell; }
namespace Plugin {

class BtAdapter : public IBtAdapter {
public:
    BtAdapter()  = default;
    ~BtAdapter() override = default;
    BtAdapter(const BtAdapter&)            = delete;
    BtAdapter& operator=(const BtAdapter&) = delete;

    // Inject a mock implementation (test builds).  Pass nullptr to reset.
    static void setImpl(IBtAdapter* newImpl);

    std::string init(PluginHost::IShell* service,
                     BtEventCallbacks eventCallbacks,
                     BtAuthCallbacks  authCallbacks);
    void deinit();

    bool getAdapterPowered(bool& powered) const;
    bool setAdapterPowered(bool powered);
    bool getAdapterName(std::string& name) const;
    bool setAdapterName(const std::string& name);
    bool isAdapterDiscoverable(bool& discoverable) const;
    bool setAdapterDiscoverable(bool discoverable, int timeoutSeconds);

    bool startScan(const std::string& profile);
    bool stopScan();

    std::vector<IBtAdapter::BtDeviceInfo> getDiscoveredDevices() const;
    std::vector<IBtAdapter::BtDeviceInfo> getPairedDevices() const;
    std::vector<IBtAdapter::BtDeviceInfo> getConnectedDevices() const;

    bool pairDevice(const std::string& handleStr);
    bool unpairDevice(const std::string& handleStr);
    bool connectDevice(const std::string& handleStr);
    bool disconnectDevice(const std::string& handleStr);

    bool getDeviceProperties(const std::string& handleStr,
                             IBtAdapter::BtDeviceProperties& props) const;

    std::string getMacForHandle(const std::string& handleStr) const;

    void respondToEvent(const std::string& mac, bool accepted);

    bool                          setAudioStream(long long int deviceID,
                                                  const std::string& streamName);
    bool                          setAudioControlCommand(long long int deviceID,
                                                         const std::string& cmd);
    bool                          setDeviceVolumeMute(long long int deviceID,
                                                       const std::string& profile,
                                                       uint8_t volume, bool mute);
    IBtAdapter::BtDeviceVolumeMute getDeviceVolumeMute(long long int deviceID,
                                                       const std::string& profile) const;
    IBtAdapter::BtMediaTrackInfo   getMediaTrackInfo(long long int deviceID) const;

    // Derive the stable numeric handle string from a MAC address.
    static std::string handleForMac(const std::string& mac);

private:
    static IBtAdapter* impl;
};

} // namespace Plugin
} // namespace WPEFramework
