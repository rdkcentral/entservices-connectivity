#pragma once

#include <Events.h>
#include <Status.h>

struct _WpCore;
typedef struct _WpCore WpCore;
struct _WpNode;
typedef struct _WpNode WpNode;
struct _WpProxy;
typedef struct _WpProxy WpProxy;

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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

    /**
     * @brief Binds the lifetime-tracking weak reference to the owning shared_ptr.
     *
     * Must be called once, right after this Audio is wrapped in a shared_ptr
     * (e.g. immediately after std::make_shared<Audio>(...)). Deferred idle
     * sources and the callback worker lock this weak_ptr to obtain a strong
     * reference, so they keep the object alive for the duration of a callback
     * instead of racing with destruction.
     */
    void bindSelf(const std::shared_ptr<Audio>& self);

  private:
    void handleNodePropsChanged();

    /**
     * @brief Enqueues a callback to run on the single audio worker thread.
     *
     * Used to deliver audio event callbacks off the WirePlumber/GLib thread
     * without spawning a new thread per event. Frequent volume/mute changes
     * are serialized through one worker instead of an unbounded number of
     * detached threads.
     */
    void dispatchCallback(std::function<void()> task);

    /**
     * @brief Worker-thread synchronization state.
     *
     * Held in a separately owned heap block (shared_ptr) that the worker thread
     * captures by value, so the worker loop only ever touches this state and
     * never *this. If a user callback drops the last owning reference to this
     * Audio - destroying it on the worker thread itself - the loop can still
     * observe @c stop and exit safely through this shared state rather than a
     * freed Audio.
     */
    struct CallbackWorker {
      std::mutex mutex;                          /**< Guards @c queue and @c stop. */
      std::condition_variable cv;                /**< Signals new work or shutdown. */
      std::deque<std::function<void()>> queue;   /**< Pending callbacks. */
      bool stop{false};                          /**< Set during teardown to stop the worker. */
    };

    /** @brief Body of the single audio callback worker thread. */
    static void callbackWorkerLoop(std::shared_ptr<CallbackWorker> state, std::string mac);

    /**
     * @brief Shared state used to make deferred idle callbacks lifetime-safe.
     *
     * Idle sources queued on the WirePlumber/GLib loop capture a shared_ptr to
     * this state instead of a raw Audio*. @c audio is a weak_ptr: locking it
     * yields a strong reference that keeps Audio alive for the duration of the
     * callback, or an empty pointer once the last owning shared_ptr is gone -
     * so no manual invalidation under a mutex is required.
     */
    struct IdleCallbackState {
      std::weak_ptr<Audio> audio;  /**< Weak owner; empty once the last strong reference is gone. */
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

    /**
     * @brief Single worker thread + bounded queue used to dispatch audio event
     * callbacks. This replaces spawning a detached thread per volume/mute
     * change, which could create an unbounded number of threads under frequent
     * updates.
     */
    std::shared_ptr<CallbackWorker> m_callbackState{std::make_shared<CallbackWorker>()}; /**< Decoupled worker state; outlives *this if a callback self-destructs. */
    std::thread m_callbackWorker;                    /**< The one callback dispatch thread. */
};

}  // namespace bluetooth


