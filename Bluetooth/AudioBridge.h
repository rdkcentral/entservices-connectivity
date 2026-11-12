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

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <bluetooth/Audio.h>
#include <bluetooth/Device.h>

#include "DeviceRegistry.h"
#include "IBtAdapter.h"

namespace WPEFramework {
namespace Plugin {

/**
 * Volume and mute of the SDK's audio output devices, with the behavior of BTMgr's BTRMGR_SetDeviceVolumeMute,
 * BTRMGR_GetDeviceVolumeMute and volume/mute media control, over the SDK's audio API.
 *
 * The SDK sets the volume on the speaker (AVRCP absolute volume, through PipeWire), so what is left to do here is
 * what BTMgr did above that layer: the 0 to 255 scale, the local volume steps, the last volume and mute kept for
 * the device (RDKTV_PERSIST_VOLUME), restoring the volume when the device connects, and onDeviceMediaStatus.
 */
class AudioBridge {
public:
    struct Callbacks {
        std::function<void(long long int deviceId, const std::string& name, const std::string& deviceType,
                           uint8_t volume, bool mute, const std::string& command)> onDeviceMediaStatus;
        std::function<std::string(const std::shared_ptr<bluetooth::Device>& device,
                                  const std::string& handleStr)> resolveDeviceType;
        std::function<bool(const std::string& handleStr, uint8_t& volume)> getLastVolume;
        std::function<void(const std::string& handleStr, uint8_t volume)> setLastVolume;
        std::function<bool(bool& mute)> getLastMute;
        std::function<void(bool mute)> setLastMute;
    };

    AudioBridge(DeviceRegistry& registry, Callbacks&& callbacks);

    AudioBridge(const AudioBridge&) = delete;
    AudioBridge& operator=(const AudioBridge&) = delete;

    // A change the SDK reports for the device. Changes the plugin makes itself are recognised and not reported twice.
    void onAudioEvent(bluetooth::AudioEvent event, const bluetooth::AudioEventData& data,
                      const std::shared_ptr<bluetooth::Device>& device);

    // BTMgr applied the stored volume once a device connected; here that happens when the device's audio is first
    // available, which is not before the connection is complete.
    void onDeviceConnected(const std::shared_ptr<bluetooth::Device>& device);
    void onDeviceDisconnected(const std::shared_ptr<bluetooth::Device>& device);

    // deviceType is the one the client sent; only an audio output device is supported.
    bool setVolumeMute(const std::shared_ptr<bluetooth::Device>& device, const std::string& deviceType,
                       uint8_t volume, bool mute);
    IBtAdapter::BtDeviceVolumeMute getVolumeMute(const std::shared_ptr<bluetooth::Device>& device,
                                                 const std::string& deviceType);
    // VOLUME_UP, VOLUME_DOWN, AUDIO_MUTE and AUDIO_UNMUTE; any other command fails.
    bool sendCommand(const std::shared_ptr<bluetooth::Device>& device, const std::string& command);

private:
    struct State {
        bool restorePending = false;
        std::chrono::steady_clock::time_point suppressUntil{};
        // What the plugin is setting right now, so the SDK's event for it can be told from a change by the device.
        bool expectVolume = false;
        uint8_t volume = 0;
        bool expectMute = false;
        bool mute = false;
    };

    // Marks a volume and/or mute change as the plugin's own while in scope.
    class Expectation {
    public:
        Expectation(AudioBridge& owner, std::string mac, const uint8_t* volume, const bool* mute);
        ~Expectation();
        Expectation(const Expectation&)            = delete;
        Expectation& operator=(const Expectation&) = delete;
    private:
        AudioBridge& m_owner;
        std::string  m_mac;
    };

    bool isAudioOutput(const std::shared_ptr<bluetooth::Device>& device, const std::string& handleStr) const;
    bool storedVolume(const std::string& handleStr, uint8_t& volume) const;
    bool storedMute(bool& mute) const;
    uint8_t currentVolume(const std::shared_ptr<bluetooth::Device>& device, const std::string& handleStr) const;
    void persistVolume(const std::string& handleStr, uint8_t volume);
    void persistMute(bool mute);
    void emitMediaStatus(const std::shared_ptr<bluetooth::Device>& device, const std::string& handleStr,
                         uint8_t volume, bool mute, const std::string& command);
    // Applies the stored volume once; false if the device's audio is not available yet.
    bool restoreStoredVolume(const std::shared_ptr<bluetooth::Device>& device, const std::string& handleStr,
                             const std::string& mac);

    DeviceRegistry& m_registry;
    Callbacks m_callbacks;

    std::mutex m_mutex;
    std::unordered_map<std::string, State> m_states;   // by MAC
};

} // namespace Plugin
} // namespace WPEFramework
