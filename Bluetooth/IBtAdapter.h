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

// Pure virtual interface for the bluetooth adapter abstraction layer.
// Intentionally SDK-free so the dispatch wrapper and testframework mock
// can include this header without a bluetooth-sdk installation.

#include <string>
#include <vector>

#include "BtAdapterCallbacks.h"

namespace WPEFramework {
namespace PluginHost { class IShell; }
namespace Plugin {

class IBtAdapter {
public:
    virtual ~IBtAdapter() = default;

    virtual std::string init(PluginHost::IShell* service,
                             BtEventCallbacks eventCallbacks,
                             BtAuthCallbacks authCallbacks) = 0;
    virtual void deinit() = 0;

    virtual bool getAdapterPowered(bool& powered) const = 0;
    virtual bool setAdapterPowered(bool powered) = 0;
    virtual bool getAdapterName(std::string& name) const = 0;
    virtual bool setAdapterName(const std::string& name) = 0;
    virtual bool isAdapterDiscoverable(bool& discoverable) const = 0;
    virtual bool setAdapterDiscoverable(bool discoverable, int timeoutSeconds) = 0;

    virtual bool startScan(const std::string& profile) = 0;
    virtual bool stopScan() = 0;

    // Device list queries return a handle-string-keyed list of descriptors.
    // The BtDeviceInfo structs contain all fields needed to build JSON responses
    // without requiring shared_ptr<bluetooth::Device> in callers.
    struct BtDeviceInfo {
        std::string handleStr;
        std::string mac;
        std::string name;
        std::string deviceType;
        bool        connected{false};
        bool        paired{false};
        uint32_t    classOfDevice{0};
        uint16_t    appearance{0};
        std::vector<std::string> uuids;
    };

    virtual std::vector<BtDeviceInfo> getDiscoveredDevices() const = 0;
    virtual std::vector<BtDeviceInfo> getPairedDevices() const = 0;
    virtual std::vector<BtDeviceInfo> getConnectedDevices() const = 0;

    virtual bool pairDevice(const std::string& handleStr) = 0;
    virtual bool unpairDevice(const std::string& handleStr) = 0;
    virtual bool connectDevice(const std::string& handleStr, const std::string& deviceType = "") = 0;
    virtual bool disconnectDevice(const std::string& handleStr, const std::string& deviceType = "") = 0;

    // Returns device properties as a plain struct (no SDK types).
    struct BtDeviceProperties {
        std::string handleStr;
        std::string mac;
        std::string name;
        std::string deviceType;
        uint32_t    classOfDevice{0};
        uint16_t    appearance{0};
        int16_t     rssi{0};
        int16_t     signalLevel{0};
        uint16_t    vendorId{0};
        uint8_t     batteryLevel{0};
        std::string modalias;
        std::vector<std::string> uuids;
    };
    virtual bool getDeviceProperties(const std::string& handleStr,
                                     BtDeviceProperties& props) const = 0;

    // Resolve a handle string to the device's MAC address.
    virtual std::string getMacForHandle(const std::string& handleStr) const = 0;

    virtual void respondToEvent(const std::string& mac, bool accepted) = 0;

    // ── Audio operations ──────────────────────────────────────────────────────
    // deviceID is vestigial in setAudioStream (adapter-level op, not device-level).

    struct BtMediaTrackInfo {
        std::string album, genre, title, artist;
        uint32_t    duration{0}, trackNumber{0}, numberOfTracks{0};
    };

    struct BtDeviceVolumeMute {
        uint8_t volume{0};
        bool    mute{false};
        bool    valid{false};   // false on backend failure
    };

    virtual bool               setAudioStream(long long int deviceID,
                                               const std::string& streamName) = 0;
    virtual bool               setAudioControlCommand(long long int deviceID,
                                                       const std::string& cmd) = 0;
    virtual bool               setDeviceVolumeMute(long long int deviceID,
                                                    const std::string& profile,
                                                    uint8_t volume, bool mute) = 0;
    virtual BtDeviceVolumeMute getDeviceVolumeMute(long long int deviceID,
                                                   const std::string& profile) const = 0;
    virtual BtMediaTrackInfo   getMediaTrackInfo(long long int deviceID) const = 0;
};

} // namespace Plugin
} // namespace WPEFramework
