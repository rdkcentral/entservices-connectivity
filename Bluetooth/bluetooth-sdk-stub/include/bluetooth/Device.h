/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
*/

#pragma once

#include <Events.h>
#include <Status.h>

#ifdef AUDIO_SUPPORT
#include "bluetooth/Audio.h"
#endif

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/**
 * @file Device.h
 * @brief Bluetooth device management interface
 * 
 * This file contains the Device class and related enumerations for managing
 * Bluetooth devices including connection, pairing, and GATT client functionality.
 */
namespace bluetooth {
class Device;
class Adapter;
class Manager;
class Audio;

namespace gattClient {
class Client;
}
/**
 * @class DeviceProperties
 * @brief Data-only class holding all Bluetooth Device1 interface properties.
 *
 * Fields are optional since not all properties may be available for every device.
 */
class DeviceProperties {
 public:
  std::optional<std::string> address;
  std::optional<std::string> addressType;
  std::optional<std::string> name;
  std::optional<std::string> alias;
  std::optional<uint32_t> classOfDevice;
  std::optional<uint16_t> appearance;
  std::optional<std::string> icon;
  std::optional<bool> paired;
  std::optional<bool> bonded;
  std::optional<bool> trusted;
  std::optional<bool> blocked;
  std::optional<bool> legacyPairing;
  std::optional<int16_t> rssi;
  std::optional<bool> connected;
  std::optional<std::vector<std::string>> uuids;
  std::optional<std::string> modalias;
  std::optional<std::string> adapter;
  std::optional<std::map<uint16_t, std::vector<uint8_t>>> manufacturerData;
  std::optional<std::map<std::string, std::vector<uint8_t>>> serviceData;
  std::optional<int16_t> txPower;
  std::optional<bool> servicesResolved;
  std::optional<bool> wakeAllowed;
  std::optional<uint8_t> batteryLevel;
};
/**
 * @enum DeviceEvent
 * @brief Events that can be emitted by a Bluetooth device
 */
enum class DeviceEvent { Paired, Connected, Disconnected, ServicesResolved, ServicesUnresolved, Unpaired };
/**
 * @enum DeviceState
 * @brief Possible states of a Bluetooth device
 */
enum class DeviceState { Invalid, Paired, Discovered, Connected };

/**
 * @enum SignalStrength
 * @brief Signal strength categories based on RSSI value
 */
enum class SignalStrength {
  None = 0,       //!< No signal (0 bars)
  Poor,           //!< Poor (1 bar)
  Fair,           //!< Fair (2 bars)
  Good,           //!< Good (3 bars)
  Excellent       //!< Excellent (4 bars)
};

/**
 * @class Device
 * @brief Represents a Bluetooth device with connection and pairing capabilities
 * 
 * The Device class provides an interface for managing Bluetooth devices,
 * including operations like pairing, connecting, disconnecting, and accessing
 * device properties. It also provides event emission capabilities and GATT
 * client functionality.
 */
class Device : public EventEmitter<DeviceEvent, std::shared_ptr<Device>>,
               public std::enable_shared_from_this<Device> {
 public:
/**
 * @brief Constructs a Device object
 * @param adapter Weak pointer to the parent Adapter
 * @param objectPath Object path for the device
 */
  explicit Device(std::weak_ptr<bluetooth::Adapter> adapter, const std::string& objectPath);
/**
 * @brief Constructs a Device object with managed object paths
 * @param adapter Weak pointer to the parent Adapter
 * @param objectPath Object path for the device
 * @param managedObjectPaths Vector of managed object paths
 */
  Device(std::weak_ptr<bluetooth::Adapter> adapter, const std::string& objectPath,
         const std::vector<std::string>& managedObjectPaths);
/**
 * @brief Destructor for Device
 */
  ~Device();
/**
 * @brief Gets the device's Bluetooth address
 * @param str Reference to string to store the address
 * @return Status indicating success or failure
 */
  Status address(std::string& str);
/**
 * @brief Gets the device's name
 * @param str Reference to string to store the name
 * @return Status indicating success or failure
 */
  Status name(std::string& str);
/**
 * @brief Gets connected state
 * @param value Reference to store connected state
 * @return Status indicating success or failure
 */
  Status connected(bool& value);
/**
 * @brief Gets the device's RSSI (Received Signal Strength Indicator)
 * @param rssi Reference to store the RSSI value
 * @return Status indicating success or failure
 */
  Status rssi(int8_t& rssi);
/**
 * @brief Gets the device's manufacturer data
 * @param data Reference to map to store manufacturer data
 * @return Status indicating success or failure
 */
  Status manufacturerData(std::map<uint16_t, std::vector<uint8_t>>& data);
/**
 * @brief Gets all device properties in a single call
 * @param properties Reference to DeviceProperties struct to populate
 * @return Status indicating success or failure
 */
  Status getAllProperties(DeviceProperties& properties);
/**
 * @brief Gets the current device state
 * @return Current DeviceState
 */
  DeviceState state();
/**
 * @brief Sets the device state
 * @param state New DeviceState to set
 */
  void state(DeviceState state);
/**
 * @brief Connects to the device
 * @param sync Whether to perform synchronous connection (default: false)
 * @param timeout Connection timeout in seconds (default: 10)
 * @return Status indicating success or failure
 */
  Status connect(bool sync = false, uint8_t timeout = 10);
/**
 * @brief Pairs with the device
 * @param sync Whether to perform synchronous pairing (default: false)
 * @param timeout Pairing timeout in seconds (default: 10)
 * @return Status indicating success or failure
 */
  Status pair(bool sync = false, uint8_t timeout = 10);
/**
 * @brief Disconnects from the device
 * @param sync Whether to perform synchronous disconnection (default: false)
 * @param timeout Disconnection timeout in seconds (default: 10)
 * @return Status indicating success or failure
 */
  Status disconnect(bool sync = false, uint8_t timeout = 10);
/**
 * @brief Unpairs the device
 * @return Status indicating success or failure
 */
  Status unpair();
/**
 * @brief Gets the GATT client for this device
 * @return Shared pointer to the GATT client
 */
  std::shared_ptr<gattClient::Client> getGattClient();

#ifdef AUDIO_SUPPORT
  Status setVolume(float volume);
  float getVolume();
  Status setMute(bool mute);
  bool isMuted();
  Status setDelayCompensation(uint32_t delayMs);
  void registerForAudioEvents(std::function<void(AudioEvent, AudioEventData)> callback);
#endif
#ifdef AUDIO_SUPPORT
 protected:
  Status initAudio(WpNode* node, WpProxy* device);
#endif

public:
/**
 * @brief Enable auto-connect for an LE device (adds to kernel auto-connect list)
 * @return Status indicating success or failure
 */
  Status setAutoconnectOn();

/**
 * @brief Disable auto-connect for an LE device (removes from kernel auto-connect list)
 * @return Status indicating success or failure
 */
  Status setAutoconnectOff();

/**
 * @brief Gets the battery level percentage for this device
 * @param percentage Reference to store the battery level (0-100)
 * @return Status indicating success or failure
 */
  Status getBatteryLevel(uint8_t& percentage);

/**
 * @brief Gets the signal strength category for a connected device using btmgmt conn-info
 * @param strength Reference to store the signal strength category
 * @return Status indicating success or failure
 */
  Status signalStrength(SignalStrength& strength);

 protected:
  friend class Manager;
/**
 * @brief Checks if the device is in a reconnection cooldown state
 * @return true if the device should be rejected due to rapid reconnection attempts
 */
  bool isInCooldown() const;

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;

  Status adapterRemove(const std::string& devicePath);
  Status adapterSetPairable(bool pairable);
};

}  // namespace bluetooth



