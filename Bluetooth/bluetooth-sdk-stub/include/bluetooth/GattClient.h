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

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "Status.h"

/**
 * @file GattClient.h
 * @brief GATT (Generic Attribute Profile) client implementation for Bluetooth communication
 * 
 * This file contains the GATT client classes that provide an interface for interacting
 * with GATT services, characteristics, and descriptors on remote Bluetooth devices.
 */

namespace bluetooth {

class Manager;
class Device;

/**
 * @namespace bluetooth::gattClient
 * @brief Contains GATT client-side classes for Bluetooth GATT profile implementation
 */
namespace gattClient {

class Characteristic;

/**
 * @class Descriptor
 * @brief Represents a GATT descriptor on a remote Bluetooth device
 * 
 * A GATT descriptor provides additional information about a characteristic.
 * This class allows reading from and writing to descriptors, as well as
 * accessing descriptor properties like UUID and value.
 */
class Descriptor {
 public:
  /**
   * @brief Constructs a Descriptor object
   * @param objectPath Object path for the descriptor
   */
  explicit Descriptor(const std::string& objectPath);

  /** @brief Destructor */
  ~Descriptor();

  /** @brief Gets the UUID of the descriptor. */
  std::string UUID();

  /** @brief Gets the current value of the descriptor. */
  std::vector<uint8_t> Value();

  /** @brief Reads the value from the descriptor. */
  Status ReadValue(std::vector<uint8_t>& value);

  /** @brief Writes a value to the descriptor. */
  Status WriteValue(const std::vector<uint8_t>& value);

  /** @brief Gets the characteristic that owns this descriptor. */
  std::shared_ptr<gattClient::Characteristic> Characteristic();

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
  friend class Client;
};

/**
 * @class Characteristic
 * @brief Represents a GATT characteristic on a remote Bluetooth device
 * 
 * A GATT characteristic is a data value transferred between client and server.
 * This class provides methods to read, write, and receive notifications from
 * characteristics, as well as access to associated descriptors.
 */
class Characteristic {
 public:
  /**
   * @brief Constructs a Characteristic object
   * @param objectPath Object path for the characteristic
   */
  explicit Characteristic(const std::string& objectPath);

  /** @brief Destructor */
  ~Characteristic();

  /** @brief Gets the UUID of the characteristic. */
  std::string UUID();

  /** @brief Gets the current value of the characteristic. */
  std::vector<uint8_t> Value();

  /** @brief Reads the value from the characteristic. */
  Status readValue(std::vector<uint8_t>& value);

  /** @brief Writes a value to the characteristic. */
  Status writeValue(const std::vector<uint8_t>& value);

  /** @brief Starts notifications for the characteristic. */
  Status StartNotify(std::function<void(std::vector<uint8_t>)> notifyCb = nullptr);

  /** @brief Stops notifications for the characteristic. */
  Status StopNotify();

  /** @brief Gets the service path that owns this characteristic. */
  std::string Service();

  /** @brief Checks if notifications are currently enabled. */
  bool isNotifyEnable();

  /** @brief Gets the flags/properties of the characteristic. */
  std::vector<std::string> Flags();

  /** @brief Gets a specific descriptor by its path. */
  std::shared_ptr<Descriptor> getDescriptor(const std::string& descriptorPath);

  /** @brief Gets all descriptors associated with this characteristic. */
  const std::map<std::string, std::shared_ptr<Descriptor>>& getDescriptors() const;

 protected:
  /** @brief Adds a descriptor to this characteristic. */
  bool addDescriptor(const std::string& descriptorPath);

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
  friend class Client;
};

/**
 * @class Service
 * @brief Represents a GATT service on a remote Bluetooth device
 * 
 * A GATT service is a collection of characteristics that encapsulate
 * the behavior of part of a device. This class provides access to
 * service properties and associated characteristics.
 */
class Service {
 public:
  /**
   * @brief Constructs a Service object
   * @param objectPath Object path for the service
   */
  explicit Service(const std::string& objectPath);

  /** @brief Destructor */
  ~Service();

  /** @brief Gets the UUID of the service. */
  std::string UUID();

  /** @brief Gets the device path that owns this service. */
  std::string Device();

  /** @brief Checks if this is a primary service. */
  bool Primary();

  /** @brief Gets the list of included services. */
  std::vector<std::string> Includes();

  /** @brief Gets a specific characteristic by its path. */
  std::shared_ptr<Characteristic> getCharacteristic(const std::string& characteristicPath);

  /** @brief Gets all characteristics associated with this service. */
  const std::map<std::string, std::shared_ptr<Characteristic>>& getCharacteristics() const;

 protected:
  /** @brief Adds a characteristic to this service. */
  bool addCharacteristic(const std::string& characteristicPath);

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
  friend class Client;
};

using gattClient::Service;
using gattClient::Characteristic;
using gattClient::Descriptor;
/**
 * @class Client
 * @brief Main GATT client class for managing services, characteristics, and descriptors
 * 
 * The Client class provides the primary interface for GATT client operations.
 * It manages the hierarchy of services, characteristics, and descriptors for
 * a specific Bluetooth device and provides methods to discover and access them.
 */
class Client {
 public:
  /**
   * @brief Constructs a GATT Client for a specific device
   * @param devicePath Object path for the target device
   */
  explicit Client(const std::string& devicePath);
  
  /**
   * @brief Destructor
   */
  ~Client();
  
  /**
   * @brief Gets a specific service by its path
   * @param servicePath Path of the service
   * @return Shared pointer to the service, nullptr if not found
   */
  std::shared_ptr<Service> getService(const std::string& servicePath);
  
  /**
   * @brief Gets all services available on the device
   * @return Const reference to map of service paths to service objects
   */
  const std::map<std::string, std::shared_ptr<Service>>& getServices();
  
  /**
   * @brief Finds a service by its UUID
   * @param uuid UUID string of the service to find
   * @return Shared pointer to the service, nullptr if not found
   */
  std::shared_ptr<Service> GetServiceByUUID(const std::string& uuid);
  
  /**
   * @brief Finds a characteristic by its UUID across all services
   * @param uuid UUID string of the characteristic to find
   * @return Shared pointer to the characteristic, nullptr if not found
   */
  std::shared_ptr<Characteristic> GetCharacteristicByUUID(const std::string& uuid);
  
  /**
   * @brief Finds a descriptor by its UUID across all characteristics
   * @param uuid UUID string of the descriptor to find
   * @return Shared pointer to the descriptor, nullptr if not found
   */
  std::shared_ptr<Descriptor> GetDescriptorByUUID(const std::string& uuid);

 protected:
  /**
   * @brief Adds a service to the client's service collection
   * @param servicePath Path of the service to add
   * @return True if successfully added, false otherwise
   */
  bool addService(const std::string& servicePath);
  
  /**
   * @brief Adds a characteristic to the appropriate service
   * @param characteristicPath Path of the characteristic to add
   * @return True if successfully added, false otherwise
   */
  bool addCharacteristic(const std::string& characteristicPath);
  
  /**
   * @brief Adds a descriptor to the appropriate characteristic
   * @param descriptorPath Path of the descriptor to add
   * @return True if successfully added, false otherwise
   */
  bool addDescriptor(const std::string& descriptorPath);

 private:
  std::string m_devicePath;
  std::map<std::string, std::shared_ptr<Service>> m_services;

  const std::map<std::string, std::shared_ptr<Characteristic>>& getCharacteristics();
  std::vector<std::shared_ptr<Descriptor>> getDescriptorPaths(
      const std::shared_ptr<Characteristic>& characteristicPath);

  friend class bluetooth::Manager;
  friend class bluetooth::Device;
};

} // namespace gattClient
} // namespace bluetooth
