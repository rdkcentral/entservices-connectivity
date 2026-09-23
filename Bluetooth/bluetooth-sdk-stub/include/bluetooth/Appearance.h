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

#define BLUETOOTH_APPEARANCE_CATEGORY_MASK 0xffc0
#define BLUETOOTH_APPEARANCE_SUBCATEGORY_MASK 0x003f
/**
 * @file Appearance.h
 * @brief Bluetooth Appearance value definitions and utility class
 * 
 * This file contains the Bluetooth Appearance class which handles the standardized
 * appearance values used in Bluetooth Low Energy advertising and device identification.
 * The appearance value consists of a category and subcategory that describes the
 * physical form and capabilities of a Bluetooth device.
 */

namespace bluetooth {

/**
 * @class Appearance
 * @brief Represents a Bluetooth Appearance value with category and subcategory
 * 
 * The Appearance class encapsulates the Bluetooth Appearance value as defined in the
 * Bluetooth specification. It provides methods to construct, parse, and access both
 * the category and subcategory components of the 16-bit appearance value.
 * 
 * The appearance value is structured as:
 * - Bits 15-6: Category (10 bits)
 * - Bits 5-0: Subcategory (6 bits)
 * 
 * @see https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/
 */
class Appearance {
 public:
  /**
   * @enum Category
   * @brief Bluetooth device category classifications
   * 
   * Defines the main categories of Bluetooth devices as specified in the
   * Generic Access Profile (GAP) assigned numbers document.
   */
  enum class Category : uint16_t {
    Uncategorized = 0x0000,          ///< Unknown or unspecified device category
    Phone = 0x0040,                  ///< Phone devices
    Computer = 0x0080,               ///< Computer devices (desktop, laptop, etc.)
    Watch = 0x00c0,                  ///< Watch devices including smartwatches
    Clock = 0x0100,                  ///< Clock devices
    Display = 0x0140,                ///< Display devices
    RemoteControl = 0x0180,          ///< Remote control devices
    EyeGlasses = 0x01c0,             ///< Eye glasses including smart glasses
    Tag = 0x0200,                    ///< Tag devices (asset tracking, etc.)
    Keyring = 0x0240,                ///< Keyring accessories
    MediaPlayer = 0x0280,            ///< Media player devices
    BarcodeScanner = 0x02c0,         ///< Barcode scanner devices
    Thermometer = 0x0300,            ///< Thermometer devices
    HeartRate = 0x0340,              ///< Heart rate monitoring devices
    BloodPressure = 0x0380,          ///< Blood pressure monitoring devices
    HumanInterfaceDevice = 0x03c0,   ///< Human Interface Devices (HID)
    GlucoseMeter = 0x0400,           ///< Glucose meter devices
    RunningWalking = 0x0440,         ///< Running and walking sensor devices
    Cycling = 0x0480,                ///< Cycling sensor and computer devices
    PulseOximeter = 0x0c40,          ///< Pulse oximeter devices
    WeightScale = 0x0c80,            ///< Weight scale devices
    OutdoorSportActivity = 0x1440    ///< Outdoor sport activity devices
  };

  /**
   * @enum SubCategory
   * @brief Device subcategory classifications within each category
   * 
   * Defines specific subcategories that provide more detailed classification
   * of devices within their main category.
   */
  enum class SubCategory : uint16_t {
    Generic = 0x0000,                    ///< Generic/default subcategory for any category

    // Barcode Scanner subcategories
    BarcodeScanner = 0x0008,             ///< Generic barcode scanner

    // Blood Pressure Monitor subcategories
    BloodPressureArm = 0x0001,           ///< Arm-worn blood pressure monitor
    BloodPressureWrist = 0x0002,         ///< Wrist-worn blood pressure monitor

    // Cycling Device subcategories
    CyclingComputer = 0x0001,            ///< Cycling computer device
    CyclingSpeedSensor = 0x0002,         ///< Cycling speed sensor
    CyclingCadenceSensor = 0x0003,       ///< Cycling cadence sensor
    CyclingPowerSensor = 0x0004,         ///< Cycling power meter
    CyclingSpeedCadenceSensor = 0x0005,  ///< Combined speed and cadence sensor

    // Heart Rate Device subcategories
    HeartRateBelt = 0x0001,              ///< Heart rate chest strap/belt

    // Human Interface Device subcategories
    Keyboard = 0x0001,                   ///< Keyboard input device
    Mouse = 0x0002,                      ///< Mouse input device
    Joystick = 0x0003,                   ///< Joystick input device
    Gamepad = 0x0004,                    ///< Gamepad controller
    DigitizerTablet = 0x0005,            ///< Graphics tablet/digitizer
    CardReader = 0x0006,                 ///< Card reader device
    DigitalPen = 0x0007,                 ///< Digital pen/stylus

    // Location & Navigation subcategories
    LocationDisplay = 0x0001,            ///< Location display device
    LocationNavigationDisplay = 0x0002,  ///< Navigation display device
    LocationPod = 0x0003,                ///< Location tracking pod
    LocationNavigationPod = 0x0004,      ///< Navigation pod device

    // Pulse Oximeter subcategories
    OximeterFingertip = 0x0001,          ///< Fingertip pulse oximeter
    OximeterWristWorn = 0x0002,          ///< Wrist-worn pulse oximeter

    // Running & Walking Sensor subcategories
    RunningWalkingInShoe = 0x0001,       ///< In-shoe running/walking sensor
    RunningWalkingOnShoe = 0x0002,       ///< On-shoe running/walking sensor
    RunningWalkingOnHip = 0x0003,        ///< Hip-worn running/walking sensor

    // Watch subcategories
    SportsWatch = 0x0001,                ///< Sports watch device

    // Thermometer subcategories
    ThermometerEar = 0x0001              ///< Ear thermometer
  };

 public:
  /**
   * @brief Constructs an Appearance from a raw 16-bit value
   * @param value The raw 16-bit appearance value
   */
  explicit Appearance(uint16_t value) : m_value(value) {}

  /**
   * @brief Constructs an Appearance from category and subcategory
   * @param category The device category
   * @param subCategory The device subcategory
   * 
   * Combines the category and subcategory values into a properly formatted
   * 16-bit appearance value according to Bluetooth specifications.
   */
  Appearance(Category category, SubCategory subCategory)
      : m_value((static_cast<uint16_t>(category) & BLUETOOTH_APPEARANCE_CATEGORY_MASK) | (static_cast<uint16_t>(subCategory) & BLUETOOTH_APPEARANCE_SUBCATEGORY_MASK)) {}

  /**
   * @brief Extracts the category from the appearance value
   * @return The device category
   */
  inline Category category() const { return static_cast<Category>(m_value & BLUETOOTH_APPEARANCE_CATEGORY_MASK); }

  /**
   * @brief Extracts the subcategory from the appearance value
   * @return The device subcategory
   */
  inline SubCategory subCategory() const { return static_cast<SubCategory>(m_value & BLUETOOTH_APPEARANCE_SUBCATEGORY_MASK); }

  /**
   * @brief Gets the raw 16-bit appearance value
   * @return The complete appearance value as a 16-bit integer
   */
  inline uint16_t rawValue() const { return m_value; }

 private:
  uint16_t m_value; ///< The 16-bit appearance value containing category and subcategory
};

}  // namespace bluetooth
