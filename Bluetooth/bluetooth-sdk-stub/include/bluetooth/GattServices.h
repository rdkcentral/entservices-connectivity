
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
#include <memory>
#include <vector>

#include "bluetooth/GattServer.h"
#include "bluetooth/Uuid.h"
/**
 * @file GattServices.h
 * @brief Implementation of common GATT services for Bluetooth communication
 * 
 * This file contains the some common GATT (Generic Attribute Profile) services such as
 * Heart Rate Service, Battery Level Service, and Current Time Service. These services
 * can be used in a Bluetooth GATT server to provide standard functionality. 
 */
namespace bluetooth {
namespace gattServer {

/**
 * @enum BodyLocation
 * @brief Represents the body location for heart rate measurement.
 */
enum class BodyLocation {
  OTHER = 0, /**< Other location. */
  CHEST,     /**< Chest location. */
  WRIST,     /**< Wrist location. */
  FINGERS,   /**< Fingers location. */
  HAND,      /**< Hand location. */
  EAR_LOBE,  /**< Ear lobe location. */
  FOOT       /**< Foot location. */
};

/**
 * @class HeartRateService
 * @brief Implements the Bluetooth Heart Rate Service.
 *
 * Provides a characteristic for heart rate measurement and updates its value.
 */
class HeartRateService {
 public:
  /**
   * @brief Constructs a HeartRateService.
   * @param heartRate Initial heart rate value.
   * @param location Body location of the sensor (default: WRIST).
   */
  explicit HeartRateService(uint8_t heartRate, BodyLocation location = BodyLocation::WRIST);

  /**
   * @brief Updates the heart rate value.
   * @param heartRate New heart rate value.
   */
  void updateHR(uint8_t heartRate);

  /** @brief Heart Rate Measurement characteristic. */
  std::shared_ptr<Characteristic> m_heartRateCharacteristic;

  /** @brief GATT service instance for Heart Rate. */
  std::shared_ptr<Service> m_service;
};

/**
 * @class BatteryLevelService
 * @brief Implements the Bluetooth Battery Level Service.
 *
 * Provides a characteristic for battery level and updates its value.
 */
class BatteryLevelService {
 public:
  /**
   * @brief Constructs a BatteryLevelService.
   * @param batteryLevel Initial battery level (0-100).
   */
  explicit BatteryLevelService(uint8_t batteryLevel);

  /**
   * @brief Updates the battery level value.
   * @param batteryLevel New battery level (0-100).
   */
  void updateBatteryLevel(uint8_t batteryLevel);

  /** @brief Battery Level characteristic. */
  std::shared_ptr<Characteristic> m_batteryLevelCharacteristic;

  /** @brief GATT service instance for Battery Level. */
  std::shared_ptr<Service> m_service;
};

/**
 * @class CurrentTimeService
 * @brief Implements the Bluetooth Current Time Service.
 *
 * Provides a characteristic that returns the current time.
 */
class CurrentTimeService {
 public:
  /**
   * @brief Constructs a CurrentTimeService.
   */
  CurrentTimeService();

  /** @brief GATT service instance for Current Time. */
  std::shared_ptr<Service> m_service;

 private:
  /** @brief Current Time characteristic. */
  std::shared_ptr<Characteristic> m_currentTimeCharacteristic;

  /**
   * @brief Retrieves the current time in Bluetooth format.
   * @return A vector of bytes representing the current time.
   */
  static std::vector<uint8_t> getCurrentTime();
};

}  // namespace gattServer
}  // namespace bluetooth