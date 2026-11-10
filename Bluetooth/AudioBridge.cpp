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
#include "AudioBridge.h"
#include "AuthBridge.h"
#include "DeviceDisplayName.h"
#include "VolumeUtils.h"
#include <UtilsLogging.h>
#include <cstdlib>
#include <utility>

namespace WPEFramework {
namespace Plugin {

AudioBridge::Expectation::Expectation(AudioBridge& owner, std::string mac, const uint8_t* volume, const bool* mute)
    : m_owner(owner)
    , m_mac(std::move(mac))
{
    std::lock_guard<std::mutex> lock(m_owner.m_mutex);
    State& state = m_owner.m_states[m_mac];
    if (volume) {
        state.expectVolume = true;
        state.volume = *volume;
    }
    if (mute) {
        state.expectMute = true;
        state.mute = *mute;
    }
}

AudioBridge::Expectation::~Expectation() {
    std::lock_guard<std::mutex> lock(m_owner.m_mutex);
    State& state = m_owner.m_states[m_mac];
    state.expectVolume = false;
    state.expectMute = false;
}

AudioBridge::AudioBridge(DeviceRegistry& registry, Callbacks&& callbacks)
    : m_registry(registry)
    , m_callbacks(std::move(callbacks))
{}

bool AudioBridge::isAudioOutput(const std::shared_ptr<bluetooth::Device>& device, const std::string& handleStr) const {
    const std::string type = m_callbacks.resolveDeviceType ? m_callbacks.resolveDeviceType(device, handleStr)
                                                           : m_registry.getDeviceType(handleStr);
    return AuthBridge::isAudioOutputType(type);
}

bool AudioBridge::storedVolume(const std::string& handleStr, uint8_t& volume) const {
    return m_callbacks.getLastVolume && m_callbacks.getLastVolume(handleStr, volume);
}

bool AudioBridge::storedMute(bool& mute) const {
    return m_callbacks.getLastMute && m_callbacks.getLastMute(mute);
}

uint8_t AudioBridge::currentVolume(const std::shared_ptr<bluetooth::Device>& device, const std::string& handleStr) const {
    uint8_t volume = 0;
    if (storedVolume(handleStr, volume)) return volume;
    return volumeFromSdk(device->getVolume());
}

void AudioBridge::persistVolume(const std::string& handleStr, uint8_t volume) {
    if (m_callbacks.setLastVolume) m_callbacks.setLastVolume(handleStr, volume);
}

void AudioBridge::persistMute(bool mute) {
    if (m_callbacks.setLastMute) m_callbacks.setLastMute(mute);
}

void AudioBridge::emitMediaStatus(const std::shared_ptr<bluetooth::Device>& device, const std::string& handleStr,
                                  uint8_t volume, bool mute, const std::string& command) {
    if (!m_callbacks.onDeviceMediaStatus) return;
    std::string type = m_callbacks.resolveDeviceType ? m_callbacks.resolveDeviceType(device, handleStr)
                                                     : m_registry.getDeviceType(handleStr);
    m_callbacks.onDeviceMediaStatus(std::strtoll(handleStr.c_str(), nullptr, 10), deviceDisplayName(device),
                                    type, volume, mute, command);
}

namespace {

// BTMgr held back the media status for 4 seconds while it applied the stored volume.
constexpr std::chrono::seconds kRestoreSuppress{4};

std::string macOf(const std::shared_ptr<bluetooth::Device>& device) {
    std::string mac;
    device->address(mac);
    return mac;
}

} // namespace

void AudioBridge::onAudioEvent(bluetooth::AudioEvent event, const bluetooth::AudioEventData& data,
                               const std::shared_ptr<bluetooth::Device>& device) {
    if (!device || event == bluetooth::AudioEvent::DelayCompensationChanged) return;
    const std::string mac = macOf(device);
    const std::string handle = DeviceRegistry::deriveHandle(mac);
    if (!DeviceRegistry::isValidHandle(handle) || !isAudioOutput(device, handle)) return;

    const uint8_t volume = volumeFromSdk(data.volume);
    bool restore = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_states.find(mac);
        if (it != m_states.end()) {
            State& state = it->second;
            if (std::chrono::steady_clock::now() < state.suppressUntil) return;
            if (event == bluetooth::AudioEvent::VolumeChanged && state.expectVolume && state.volume == volume) return;
            if (event == bluetooth::AudioEvent::MuteStateChanged && state.expectMute && state.mute == data.muted) return;
            restore = state.restorePending;
            state.restorePending = false;
        }
    }
    // The stored volume goes back to the device first; that is what the client is told about, not the device's own.
    if (restore && restoreStoredVolume(device, handle, mac)) return;

    uint8_t previousVolume = kVolumeDefaultPrevious;
    (void)storedVolume(handle, previousVolume);
    bool previousMute = false;
    (void)storedMute(previousMute);

    if (event == bluetooth::AudioEvent::VolumeChanged) {
        persistVolume(handle, volume);
        emitMediaStatus(device, handle, volume, data.muted,
                        mediaStatusCommand(previousVolume, data.muted, volume, data.muted));
    } else {
        persistMute(data.muted);
        emitMediaStatus(device, handle, volume, data.muted,
                        mediaStatusCommand(volume, previousMute, volume, data.muted));
    }
}

void AudioBridge::onDeviceConnected(const std::shared_ptr<bluetooth::Device>& device) {
    if (!device) return;
    const std::string mac = macOf(device);
    const std::string handle = DeviceRegistry::deriveHandle(mac);
    if (!DeviceRegistry::isValidHandle(handle) || !isAudioOutput(device, handle)) return;
    uint8_t stored = 0;
    if (!storedVolume(handle, stored)) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_states[mac].restorePending = true;
}

void AudioBridge::onDeviceDisconnected(const std::shared_ptr<bluetooth::Device>& device) {
    if (!device) return;
    const std::string mac = macOf(device);
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_states.find(mac);
    if (it != m_states.end()) it->second.restorePending = false;
}

bool AudioBridge::restoreStoredVolume(const std::shared_ptr<bluetooth::Device>& device, const std::string& handleStr,
                                      const std::string& mac) {
    uint8_t stored = 0;
    if (!storedVolume(handleStr, stored)) return true;
    bool mute = false;
    (void)storedMute(mute);
    const uint8_t live = volumeFromSdk(device->getVolume());
    {
        Expectation expectation(*this, mac, &stored, nullptr);
        Status status = device->setVolume(volumeToSdk(stored));
        if (!status) {
            LOGERR("AudioBridge::restoreStoredVolume: exit with error: %s", status.get_message().c_str());
            return false;
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_states[mac].suppressUntil = std::chrono::steady_clock::now() + kRestoreSuppress;
    }
    LOGINFO("AudioBridge::restoreStoredVolume: applied volume=%u handle=%s", static_cast<unsigned>(stored), handleStr.c_str());
    emitMediaStatus(device, handleStr, stored, mute, mediaStatusCommand(live, mute, stored, mute));
    return true;
}

bool AudioBridge::setVolumeMute(const std::shared_ptr<bluetooth::Device>& device, const std::string& deviceType,
                                uint8_t volume, bool mute) {
    LOGINFO("AudioBridge::setVolumeMute: entry deviceType=%s volume=%u mute=%d", deviceType.c_str(),
            static_cast<unsigned>(volume), mute);
    if (!device) return false;
    if (volumeOpTypeFromDeviceType(deviceType) != VolumeOpType::AudioOutput) {
        LOGERR("AudioBridge::setVolumeMute: exit with error: not an audio output device");
        return false;
    }
    if (device->state() != bluetooth::DeviceState::Connected) {
        LOGERR("AudioBridge::setVolumeMute: exit with error: device is not connected");
        return false;
    }
    const std::string mac = macOf(device);
    const std::string handle = DeviceRegistry::deriveHandle(mac);

    uint8_t previousVolume = kVolumeDefaultPrevious;
    (void)storedVolume(handle, previousVolume);
    bool previousMute = false;
    (void)storedMute(previousMute);

    {
        Expectation expectation(*this, std::move(mac), &volume, &mute);
        Status status = device->setVolume(volumeToSdk(volume));
        if (!status) {
            LOGERR("AudioBridge::setVolumeMute: exit with error: %s", status.get_message().c_str());
            return false;
        }
        persistVolume(handle, volume);
        status = device->setMute(mute);
        if (!status) {
            LOGERR("AudioBridge::setVolumeMute: exit with error: %s", status.get_message().c_str());
            return false;
        }
    }
    persistMute(mute);
    emitMediaStatus(device, handle, volume, mute, mediaStatusCommand(previousVolume, previousMute, volume, mute));
    LOGINFO("AudioBridge::setVolumeMute: exit successfully");
    return true;
}

IBtAdapter::BtDeviceVolumeMute AudioBridge::getVolumeMute(const std::shared_ptr<bluetooth::Device>& device,
                                                          const std::string& deviceType) {
    IBtAdapter::BtDeviceVolumeMute result;
    if (!device || volumeOpTypeFromDeviceType(deviceType) != VolumeOpType::AudioOutput ||
        device->state() != bluetooth::DeviceState::Connected) {
        LOGERR("AudioBridge::getVolumeMute: exit with error: not a connected audio output device");
        return result;
    }
    const std::string handle = DeviceRegistry::deriveHandle(macOf(device));
    bool mute = false;
    if (!storedMute(mute)) mute = device->isMuted();
    result.volume = currentVolume(device, handle);
    result.mute = mute;
    result.valid = true;
    return result;
}

bool AudioBridge::sendCommand(const std::shared_ptr<bluetooth::Device>& device, const std::string& command) {
    LOGINFO("AudioBridge::sendCommand: entry command=%s", command.c_str());
    const bool volumeStep = (command == "VOLUME_UP" || command == "VOLUME_DOWN");
    const bool muteChange = (command == "AUDIO_MUTE" || command == "AUDIO_UNMUTE");
    if (!device || !(volumeStep || muteChange)) return false;

    const std::string mac = macOf(device);
    const std::string handle = DeviceRegistry::deriveHandle(mac);
    if (!DeviceRegistry::isValidHandle(handle) || !isAudioOutput(device, handle) ||
        device->state() != bluetooth::DeviceState::Connected) {
        LOGERR("AudioBridge::sendCommand: exit with error: not a connected audio output device");
        return false;
    }

    if (volumeStep) {
        const uint8_t current = currentVolume(device, handle);
        const uint8_t next = (command == "VOLUME_UP") ? volumeStepUp(current) : volumeStepDown(current);
        {
            Expectation expectation(*this, std::move(mac), &next, nullptr);
            Status status = device->setVolume(volumeToSdk(next));
            if (!status) {
                LOGERR("AudioBridge::sendCommand: exit with error: %s", status.get_message().c_str());
                return false;
            }
        }
        persistVolume(handle, next);
        // As BTMgr reports it: the volume step leaves the mute state alone but reports mute at volume 0.
        emitMediaStatus(device, handle, next, next == 0, command);
    } else {
        const bool mute = (command == "AUDIO_MUTE");
        {
            Expectation expectation(*this, std::move(mac), nullptr, &mute);
            Status status = device->setMute(mute);
            if (!status) {
                LOGERR("AudioBridge::sendCommand: exit with error: %s", status.get_message().c_str());
                return false;
            }
        }
        persistMute(mute);
        emitMediaStatus(device, handle, currentVolume(device, handle), mute, command);
    }
    LOGINFO("AudioBridge::sendCommand: exit successfully");
    return true;
}

} // namespace Plugin
} // namespace WPEFramework
