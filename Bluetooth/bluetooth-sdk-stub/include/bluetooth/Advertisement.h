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

#include <bluetooth/Appearance.h>
#include <bluetooth/Uuid.h>
#include <Status.h>

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * @file Advertisement.h
 * @brief Bluetooth Low Energy (BLE) advertisement management classes
 */
namespace bluetooth {
/**
 * @enum AdvertisementType
 * @brief Defines the types of BLE advertisements
 */
enum class AdvertisementType { Broadcast, Peripheral, Unknown };

/**
 * @class Advertisement
 * @brief Represents a Bluetooth Low Energy advertisement
 * 
 * Provides functionality to configure and manage BLE advertisements.
 */
class Advertisement {
 public:
  Advertisement();
  ~Advertisement();

  /** @brief Sets the advertisement type. */
  void Type(AdvertisementType type);
  /** @brief Sets the service UUIDs to advertise. */
  void ServiceUUIDs(std::vector<Uuid> serviceUuids);
  /** @brief Sets the manufacturer data. */
  void ManufacturerData(std::map<uint16_t, std::vector<uint8_t>>& data);
  /** @brief Sets the service data UUIDs. */
  void ServiceData(std::vector<Uuid>& serviceUuids);
  /** @brief Sets the discoverable timeout. */
  void DiscoverableTimeout(std::chrono::seconds value);
  /** @brief Sets the local name. */
  void LocalName(std::string name);
  /** @brief Sets the appearance value. */
  void AppearanceValue(Appearance appearance);
  /** @brief Sets the advertisement duration. */
  void Duration(std::chrono::seconds duration);
  /** @brief Sets the advertisement timeout. */
  void Timeout(std::chrono::seconds timeout);
  /** @brief Sets the minimum advertising interval. */
  void MinInterval(std::chrono::milliseconds interval);
  /** @brief Sets the maximum advertising interval. */
  void MaxInterval(std::chrono::milliseconds interval);
  /** @brief Sets the transmission power level. */
  void TxPower(int16_t txPower);
  /** @brief Sets the registration state (used by AdvertisementMgr). */
  void registered(bool registered);

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};


/**
 * @class AdvertisementMgr
 * @brief Manages Bluetooth Low Energy advertising operations
 */


class AdvertisementMgr {
 public:
/**
 * @brief Constructs a new AdvertisementMgr object
 * @param adapterPath Object path of the adapter to advertise on
 */
  explicit AdvertisementMgr(const std::string& adapterPath);
/**
 * @brief Destroys the AdvertisementMgr object
 */
  ~AdvertisementMgr();
/**
 * @brief Starts advertising with the given advertisement
 * @param advertisement Shared pointer to the advertisement to start
 * @return Status indicating success or failure
 */
  Status startAdvertising(std::shared_ptr<Advertisement> advertisement);

/**
 * @brief Stops the current advertisement
 * @return Status indicating success or failure
 */
  Status stopAdvertising();

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace bluetooth
