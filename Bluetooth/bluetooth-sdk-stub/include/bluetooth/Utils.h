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


/**
 * @file Utils.h
 * @brief Utility functions and types for Bluetooth object path handling.
 *
 * This file provides helper functions for creating and parsing BlueZ object paths,
 * as well as determining Bluetooth object types.
 */

#pragma once
#include <sdbus-c++/sdbus-c++.h>
#include <string>

namespace bluetooth {

/**
 * @enum BtObjectType
 * @brief Represents the type of a Bluetooth object based on its DBus object path.
 */
enum class BtObjectType {
    Adapter,        /**< Bluetooth adapter object. */
    Device,         /**< Bluetooth device object. */
    Service,        /**< GATT service object. */
    Characteristic, /**< GATT characteristic object. */
    Descriptor,     /**< GATT descriptor object. */
    NotSupported    /**< Unsupported or unknown object type. */
};

/**
 * @brief Creates a DBus object path for an adapter based on its ID.
 * @param id The adapter ID.
 * @return The DBus object path for the adapter.
 */
sdbus::ObjectPath createAdapterPathById(int id);

/**
 * @brief Extracts the adapter ID from a DBus object path.
 * @param path The DBus object path.
 * @return The adapter ID as an integer.
 */
int getAdapterIdFromObjectPath(const sdbus::ObjectPath& path);

/**
 * @brief Retrieves the device path from a given GATT object path.
 * @param path The GATT object path.
 * @return The device path as a string.
 */
std::string getDevicePathFromGattPath(const sdbus::ObjectPath& path);

/**
 * @brief Extracts the characteristic path from a descriptor path.
 * @param descriptorPath The descriptor's DBus object path.
 * @return The characteristic path as a string.
 */
std::string extractCharacteristicPath(const std::string& descriptorPath);

/**
 * @brief Extracts the service path from a characteristic path.
 * @param characteristicPath The characteristic's DBus object path.
 * @return The service path as a string.
 */
std::string extractServicePath(const std::string& characteristicPath);

/**
 * @brief Retrieves the MAC address from a device's DBus object path.
 * @param device The device's DBus object path.
 * @return The MAC address as a string.
 */
std::string getMacFromObjectPath(const sdbus::ObjectPath& device);

/**
 * @brief Determines the Bluetooth object type from a DBus object path.
 * @param path The DBus object path.
 * @return The corresponding BtObjectType.
 */
BtObjectType getObjectTypeFromObjectPath(const sdbus::ObjectPath& path);

/**
 * @brief Validates whether a string is a well-formed MAC address (XX:XX:XX:XX:XX:XX).
 * @param mac The string to validate.
 * @return true if valid, false otherwise.
 */
bool isValidMac(const std::string& mac);

}  // namespace bluetooth

