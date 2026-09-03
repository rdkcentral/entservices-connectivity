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
#include <bluetooth/Device.h>
#include <bluetooth/Uuid.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/**
 * @file Adapter.h
 * @brief Bluetooth adapter management interface
 */

namespace bluetooth {

/**
 * @brief Forward declaration of Adapter class
 */
class Adapter;

/**
 * @brief Forward declaration of Manager class
 */
class Manager;

/**
 * @brief Forward declaration of the advertisement manager (see Advertisement.h)
 */
class AdvertisementMgr;

namespace gattServer {
/**
 * @brief Forward declaration of GATT Server class
 */
class Server;
}

/**
 * @brief Enumeration of adapter events that can be emitted
 */
enum class AdapterEvent {
  DiscoveryStarted,   ///< Device discovery has started
  DiscoveryStopped,   ///< Device discovery has stopped
  PoweredOn,          ///< Adapter has been powered on
  PoweredOff,         ///< Adapter has been powered off
  DeviceDiscovered,   ///< A new device has been discovered
  DeviceDisappeared   ///< A previously discovered device is no longer visible
};

/**
 * @brief Enumeration of scan types for device discovery
 */
enum class ScanType { 
  AllDevices,   ///< Scan for all types of devices (BLE and Classic)
  LeOnly,       ///< Scan only for Bluetooth Low Energy devices
  ClassicOnly   ///< Scan only for Classic Bluetooth devices
};

/**
 * @brief Structure defining scan filter criteria for device discovery
 */
struct ScanFilter {
  /**
   * @brief Default constructor initializing filter for all devices with no pattern
   */
  ScanFilter() : type(ScanType::AllDevices), pattern("") {}
  
  ScanType type;                    ///< Type of scan to perform
  std::vector<Uuid> uuids;         ///< List of service UUIDs to filter for
  std::string pattern;             ///< Name pattern to match against device names
};

/**
 * @brief Data structure passed with adapter events
 */
struct AdapterEventData {
  std::shared_ptr<Adapter> adapter;  ///< Pointer to the adapter that generated the event
  std::shared_ptr<Device> device;    ///< Pointer to the device associated with the event (if applicable)
};

/**
 * @class Adapter
 * @brief Bluetooth adapter class for managing Bluetooth operations
 * 
 * This class provides an interface to control a Bluetooth adapter, including
 * device discovery, power management, and device enumeration, with event
 * emission capabilities.
 */
class Adapter : public EventEmitter<AdapterEvent, AdapterEventData>,
                public std::enable_shared_from_this<Adapter> {
 public:
  /**
   * @brief Constructor for adapter with specified ID
   * @param id Adapter ID (default: 0 for first adapter)
   */
  explicit Adapter(int id = 0);
  
  /**
   * @brief Constructor for adapter with specified object path
   * @param adapterPath Object path of the adapter
   */
  explicit Adapter(std::string adapterPath);
  
  /**
   * @brief Destructor
   */
  ~Adapter();
  
  /**
   * @brief Start scanning for devices with optional filter
   * @param filter Scan filter criteria (default: scan for all devices)
   * @return Status of the operation
   */
  Status startScan(ScanFilter filter = ScanFilter());
  
  /**
   * @brief Stop device scanning
   * @return Status of the operation
   */
  Status stopScan();
  
  /**
   * @brief Get all discovered devices
   * @return Vector of shared pointers to discovered devices
   */
  std::vector<std::shared_ptr<Device>> getDevices();

  /**
   * @brief Set the name of the adapter
   * @param name New name for the adapter
   * @return Status of the operation
   */
  Status setName(const std::string& name);
  
  /**
   * @brief Get devices filtered by state
   * @param state Device state to filter by
   * @return Vector of shared pointers to devices matching the specified state
   */
  std::vector<std::shared_ptr<Device>> getDevices(DeviceState state);
  
  /**
   * @brief Get a specific device by MAC address
   * @param macAddress MAC address of the device to retrieve
   * @return Shared pointer to the device, or nullptr if not found
   */
  std::shared_ptr<Device> getDevice(const std::string& macAddress);

  /**
   * @brief Remove transient (unpaired, not-connected) devices and their BlueZ cache.
   *
   * @param keep Device to retain even if transient (e.g. selected target).
   * @param graceSeconds Seconds to wait before removal; 0 removes immediately.
   */
  void pruneDiscoveredDevices(const std::shared_ptr<Device>& keep = nullptr, unsigned int graceSeconds = 0);

  /**
   * @brief Get the powered state of the adapter
   * @param powered Reference to boolean that will receive the powered state
   * @return Status of the operation
   */
  Status getPowered(bool& powered);
  
  /**
   * @brief Set the powered state of the adapter
   * @param powered True to power on, false to power off
   * @return Status of the operation
   */
  Status setPowered(bool powered);

  /**
   * @brief Get the advertisement manager for this adapter
   * @return Shared pointer to the advertisement manager
   */

  std::shared_ptr<AdvertisementMgr> getAdvertisementMgr();
  
  /**
   * @brief Get the GATT server manager for this adapter
   * @param connections Maximum number of connections (-1 for unlimited)
   * @return Shared pointer to the GATT server manager
   */
  std::shared_ptr<gattServer::Server> getGattServerMgr(int connections = -1);

#ifdef AUDIO_SUPPORT
  // Returns a referenced WpNode (caller must g_object_unref), or nullptr if not found/ready
  WpNode* findWirePlumberAudioNode(const std::string& deviceMacAddress);
#endif

 private:
  friend class bluetooth::Manager;             ///< Manager class needs access to private members
  friend class bluetooth::gattServer::Server;  ///< GATT Server needs access to private members
  friend class bluetooth::Device;              ///< Device class needs access to private members

  class Impl;
  std::unique_ptr<Impl> m_impl;

  /**
   * @brief Set the pairable state of the adapter for internal pairing flow
   * @param pairable True to enable pairing, false to disable pairing
   * @return Status of the operation
   */
  Status setPairable(bool pairable);

  /**
   * @brief Remove a device from the adapter (used internally through Device class)
   * @param devicePath Object path of the device to remove
   * @return Status of the operation
   */
  Status remove(const std::string& devicePath);

  /**
   * @brief Object path of this adapter as a plain string
   */
  std::string path() const;

  /**
   * @brief Add multiple devices from managed objects
   * @param managedDevices Map of device object paths to their child (gatt) object paths
   */
  void addDevices(const std::map<std::string, std::vector<std::string>>& managedDevices);

  /**
   * @brief Handle discovery of a new device
   * @param devicePath Object path of the newly discovered device
   */
  void newDeviceFound(const std::string& devicePath);

  /**
   * @brief Handle removal of a device
   * @param devicePath Object path of the removed device
   */
  void deviceRemoved(const std::string& devicePath);

#ifdef AUDIO_SUPPORT
  struct AudioManagerState {
    explicit AudioManagerState(Manager* manager) : manager(manager) {}

    std::mutex mutex;
    Manager* manager;
  };

  std::shared_ptr<AudioManagerState> m_audioManagerState;
#endif
};

}  // namespace bluetooth


