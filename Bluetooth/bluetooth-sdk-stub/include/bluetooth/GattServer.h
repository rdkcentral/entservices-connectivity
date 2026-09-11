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

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * @file GattServer.h
 * @brief GATT (Generic Attribute Profile) server implementation for Bluetooth communication
 * 
 * This file contains the GATT server classes that provide an interface for defining and
 * managing GATT services, characteristics, and descriptors.
 */

namespace bluetooth {

class Adapter;

namespace gattServer {

/**
 * @enum GattProperty
 * @brief Defines the possible GATT properties for characteristics.
 *
 * These properties determine how a characteristic can be accessed or interacted with.
 */
enum GattProperty {
  BROADCAST = 1 << 0,              /**< Characteristic supports broadcasting. */
  READ = 1 << 1,                   /**< Characteristic can be read. */
  WRITE = 1 << 2,                  /**< Characteristic can be written. */
  WRITE_WITHOUT_RESPONSE = 1 << 3, /**< Write without requiring a response. */
  RELIABLE_WRITE = 1 << 4,         /**< Supports reliable writes. */
  NOTIFY = 1 << 5,                 /**< Supports notifications. */
  INDICATE = 1 << 6                /**< Supports indications. */
};

/**
 * @typedef GattProperties
 * @brief Represents a bitmask of GATT properties.
 */
typedef uint32_t GattProperties;

/**
 * @brief Forward declaration of Characteristic class.
 */
class Characteristic;

/**
 * @brief Forward declaration of Service class.
 */
class Service;

/**
 * @def CHECK_GATT_PROPERTY
 * @brief Macro to check if a specific GATT property is set.
 *
 * @param properties The bitmask of properties.
 * @param property The property to check.
 */
#define CHECK_GATT_PROPERTY(properties, property) (properties & property)

/**
 * @class Descriptor
 * @brief Represents a GATT Descriptor in the Bluetooth GATT server.
 *
 * A descriptor provides additional information about a characteristic.
 */
class Descriptor {
 public:
  /** @brief Constructs a Descriptor with a constant value. */
  Descriptor(std::string UUID, std::vector<uint8_t> value, uint16_t handle = 0);

  /** @brief Constructs a Descriptor with read and write callbacks. */
  Descriptor(std::string UUID, std::function<std::vector<uint8_t>()> readCb,
             std::function<void(std::vector<uint8_t>)> writeCb, uint16_t handle = 0);

  /** @brief Destructor for Descriptor. */
  ~Descriptor();

  /** @brief Gets the UUID of the descriptor. */
  std::string UUID();

  /** @brief Gets the current value of the descriptor. */
  std::vector<uint8_t> Value();

  /** @brief Sets the value of the descriptor. */
  void setValue(std::vector<uint8_t>);

  std::string m_path; /**< Object path for the descriptor. */

 protected:
  /** @brief Associates the descriptor with a characteristic. */
  void Characteristic(std::shared_ptr<gattServer::Characteristic> characteristic);

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
  friend gattServer::Characteristic;
};

/**
 * @class Characteristic
 * @brief Represents a GATT Characteristic in the Bluetooth GATT server.
 *
 * A characteristic holds a value and optional descriptors.
 */
class Characteristic : public std::enable_shared_from_this<Characteristic> {
 public:
  /** @brief Constructs a Characteristic with a constant value. */
  Characteristic(std::string UUID, GattProperties properties, std::vector<uint8_t> value, uint16_t handle = 0,
                 uint16_t mtu = 200);

  /** @brief Constructs a Characteristic with read and write callbacks. */
  Characteristic(std::string UUID, GattProperties properties, std::function<std::vector<uint8_t>()> readCb,
                 std::function<void(std::vector<uint8_t>)> writeCb, uint16_t handle = 0, uint16_t mtu = 200);

  /** @brief Destructor for Characteristic. */
  ~Characteristic();

  /** @brief Gets the UUID of the characteristic. */
  std::string UUID();

  /** @brief Gets all descriptors associated with this characteristic. */
  const std::map<std::string, std::shared_ptr<Descriptor>>& getDescriptors() const;

  /** @brief Adds a descriptor to the characteristic. */
  bool addDescriptor(std::shared_ptr<Descriptor> descriptor);

  /** @brief Gets the current value of the characteristic. */
  std::vector<uint8_t> Value();

  /** @brief Sets the value of the characteristic. */
  void setValue(std::vector<uint8_t>);

  std::string m_path; /**< Object path for the characteristic. */

 protected:
  /** @brief Associates the characteristic with a service. */
  void Service(std::shared_ptr<gattServer::Service> service);

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
  friend gattServer::Service;
};



/**
 * @class Service
 * @brief Bluetooth GATT Service implementation.
 * 
 * This class represents a GATT (Generic Attribute Profile) service in a Bluetooth Low Energy
 * application and provides functionality to manage characteristics and service properties.
 * 
 * The service can be configured as primary or secondary and supports service inclusion.
 * It maintains a collection of characteristics.
 */
class Service : public std::enable_shared_from_this<Service> {
 public:
  /** @brief Constructs a new GATT Service. */
  explicit Service(std::string UUID, bool primary = true, uint16_t handle = 0,
                   std::vector<std::shared_ptr<Service>> includes = {});
  /** @brief Destructor for the Service. */
  ~Service();
  /** @brief Gets the service UUID. */
  std::string UUID();
  /** @brief Gets the characteristics associated with this service. */
  const std::map<std::string, std::shared_ptr<Characteristic>>& getCharacteristics() const;
  /** @brief Adds a characteristic to this service. */
  bool addCharacteristic(std::shared_ptr<Characteristic>);
  std::string m_path;

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

/**
 * @class Server
 * @brief GATT Server implementation for Bluetooth Low Energy services
 * 
 * The Server class manages GATT services, characteristics, and descriptors for a Bluetooth
 * Low Energy peripheral device. It handles service registration with the BlueZ GATT manager
 * and manages client connections.
 */
class Server {
 public:
  /**
   * @brief Constructs a new GATT Server instance
   * 
   * @param adapter Shared pointer to the Bluetooth adapter to use for this server
   * @param connections Maximum number of concurrent client connections (default: unlimited)
   */
  explicit Server(std::shared_ptr<Adapter> adapter, uint8_t connections = -1);
  
  /**
   * @brief Destroys the GATT Server and cleans up resources
   */
  ~Server();
  
  /**
   * @brief Gets all registered services
   * 
   * @return const std::map<std::string, std::shared_ptr<Service>>& Map of service UUIDs to Service objects
   */
  const std::map<std::string, std::shared_ptr<Service>>& getServices();
  
  /**
   * @brief Finds a service by its UUID
   * 
   * @param uuid The UUID string of the service to find
   * @return std::shared_ptr<Service> Pointer to the service, or nullptr if not found
   */
  std::shared_ptr<Service> GetServiceByUUID(const std::string& uuid);
  
  /**
   * @brief Finds a characteristic by its UUID across all services
   * 
   * @param uuid The UUID string of the characteristic to find
   * @return std::shared_ptr<Characteristic> Pointer to the characteristic, or nullptr if not found
   */
  std::shared_ptr<Characteristic> GetCharacteristicByUUID(const std::string& uuid);
  
  /**
   * @brief Finds a descriptor by its UUID across all services and characteristics
   * 
   * @param uuid The UUID string of the descriptor to find
   * @return std::shared_ptr<Descriptor> Pointer to the descriptor, or nullptr if not found
   */
  std::shared_ptr<Descriptor> GetDescriptorByUUID(const std::string& uuid);
  
  /**
   * @brief Adds a service to the GATT server
   * 
   * @param service Shared pointer to the service to add
   * @return true if the service was successfully added, false otherwise
   */
  bool addService(std::shared_ptr<Service> service);
  
  /**
   * @brief Removes a service from the GATT server
   * 
   * @param service Shared pointer to the service to remove
   * @return true if the service was successfully removed, false otherwise
   */
  bool removeService(std::shared_ptr<Service> service);
  
  /**
   * @brief Starts the GATT server and registers it with BlueZ
   * 
   * @return Status indicating success or failure of the operation
   */
  Status start();
  
  /**
   * @brief Stops the GATT server and unregisters it from BlueZ
   * 
   * @return Status indicating success or failure of the operation
   */
  Status stop();

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
  friend class bluetooth::Adapter;
};

}  // namespace gattServer
}  // namespace bluetooth
