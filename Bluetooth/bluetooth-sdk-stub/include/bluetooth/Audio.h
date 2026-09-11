#pragma once

#include <Events.h>
#include <Status.h>

struct _WpCore;
typedef struct _WpCore WpCore;
struct _WpNode;
typedef struct _WpNode WpNode;
struct _WpProxy;
typedef struct _WpProxy WpProxy;

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>
#include <atomic>

/**
 * @file Audio.h
 * @brief Handles audio commands for a Bluetooth device, specifically volume, delay, and mute controls.
 * 
 * This file interacts with the pipewire audio server to manage audio streams and applies necessary delay compensation for Bluetooth audio outputs to maintain AV sync.
 * It also handles volume adjustments and mute controls
 */
namespace bluetooth {

enum class AudioEvent { VolumeChanged, MuteStateChanged, DelayCompensationChanged };

struct AudioEventData {
  std::string deviceMacAddress;  ///< MAC address of the Bluetooth device associated with the event
  float volume;                 ///< Current volume level (0.0 to 1.0)
  bool muted;                   ///< Current mute state
  uint32_t delayCompensation;   ///< Current delay compensation in milliseconds
};

class Audio : public EventEmitter<AudioEvent, AudioEventData> {
  public:
    /**
    * @brief Constructor for the Audio class.
    * @param deviceMacAddress The MAC address of the Bluetooth device.
    */
    explicit Audio(std::string deviceMacAddress, WpNode* node, WpProxy* device);
    
    /**
    * @brief Destructor for the Audio class.
    */
    ~Audio();
    
    /**
    * @brief Sets the volume level for the audio output.
    * @param volume The desired volume level (0.0 to 1.0).
    * @return Status of the operation.
    */
    Status setVolume(float volume);
    
    /**
    * @brief Gets the current volume level for the audio output.
    * @return The current volume level (0.0 to 1.0).
    */
    float getVolume();
    
    /**
    * @brief Mutes or unmutes the audio output.
    * @param mute True to mute, false to unmute.
    * @return Status of the operation.
    */
    Status setMute(bool mute);
    
    /**
    * @brief Checks if the audio output is currently muted.
    * @return True if muted, false otherwise.
    */
    bool isMuted();
    
    /**
    * @brief Sets delay compensation for Bluetooth audio outputs to maintain AV sync.
    * @param delayMs The desired delay compensation in milliseconds.
    * @return Status of the operation.
    */
    Status setDelayCompensation(uint32_t delayMs);
    
  private:
    void handleNodePropsChanged();

    /**
     * @brief Shared state used to make deferred idle callbacks lifetime-safe.
     *
     * Idle sources queued on the WirePlumber/GLib loop capture a shared_ptr to
     * this state instead of a raw Audio*. The destructor nulls @c audio under
     * @c mutex, so any still-pending source sees a null pointer and safely
     * does nothing rather than dereferencing a destroyed Audio object.
     */
    struct IdleCallbackState {
      std::mutex mutex;         /**< Guards access to @c audio. */
      Audio* audio{nullptr};    /**< Owning Audio, or nullptr once destroyed. */
    };

    std::string m_deviceMacAddress; /**< MAC address of the Bluetooth device. */
    WpCore* m_core; /**< Pointer to the WirePlumber core instance. */
    WpNode* m_node; /**< Pointer to the WirePlumber node instance. */
    WpProxy* m_device; /**< Pointer to the WirePlumber device proxy (for Route params). */
    int m_routeIndex{0}; /**< PipeWire Route index for the audio sink. */
    int m_routeDevice{0}; /**< PipeWire Route device index. */
    std::map<std::string, std::tuple<float, bool, uint32_t>> m_audioSettings; /**< Map to store audio settings (volume, mute state, delay compensation) for each device. */
    std::mutex m_mtx;
    std::shared_ptr<IdleCallbackState> m_idleCallbackState{std::make_shared<IdleCallbackState>()}; /**< Shared guard for deferred idle callbacks. */
    unsigned long m_signalHandlerId{0}; /**< GSignal handler ID for params-changed. */
};

}  // namespace bluetooth

