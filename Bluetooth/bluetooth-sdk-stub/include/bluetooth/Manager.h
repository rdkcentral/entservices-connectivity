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
 * @file Manager.h
 * @brief Bluetooth Manager and Agent classes for handling BlueZ integration.
 *
 * This file defines the Manager class responsible for managing Bluetooth adapters,
 * authorization modes, and agent registration with BlueZ. It also includes the Agent
 * and AgentManager classes for handling pairing and authorization requests.
 *
 */

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <variant>
#include <Status.h>
#include "LogRedirect.h"

/**
 * @enum LogLocation
 * @brief Specifies where logs should be directed.
 */
enum class LogLocation {
  Stdout,      /**< Log to standard output. */
  File,        /**< Log to a file. */
  LogRedirect  /**< Log using a custom redirect mechanism. */
};

namespace bluetooth {

class Adapter;
class Device;

/**
 * @enum AuthorisationMode
 * @brief Defines how authorization is handled for Bluetooth events.
 */
enum class AuthorisationMode {
  NoAuthorisation,       /**< No authorization; events are not checked. */
  AutoAccept,            /**< Automatically accept all events. */
  ExternalAuthorisation  /**< External application handles authorization via callback. */
};

/**
 * @enum AuthorisationType
 * @brief Represents the type of authorization request.
 */
enum class AuthorisationType {
  PairingRequest,    /**< Authorization for pairing. */
  ConnectionRequest  /**< Authorization for connection. */
};

/**
 * @class Manager
 * @brief Manages Bluetooth adapters and authorization using BlueZ.
 *
 * The Manager class provides functionality to retrieve adapters, handle authorization,
 * and manage BlueZ agents for pairing and connection requests.
 */
class Manager {
 public:
  /**
   * @brief Constructs a Manager with optional authorization and logging.
   * @param isAuthManager Authorization mode.
   * @param authManagerCallback Callback for external authorization (optional).
   * @param logOutput Logging destination.
   * @param output Log output configuration (file path or LogRedirect instance).
   * @param agentCapability The capability string for the BlueZ agent (defaults to "DisplayYesNo").
   */
  Manager(AuthorisationMode isAuthManager = AuthorisationMode::NoAuthorisation,
          std::function<bool(AuthorisationType authType, std::shared_ptr<Device>)> authManagerCallback = {},
          LogLocation logOutput = LogLocation::Stdout,
          std::variant<std::string, std::unique_ptr<LogRedirect>> output = {},
          std::string agentCapability = "DisplayYesNo");

  /**
   * @brief Constructs a Manager with a specific agent capability.
   * @param agentCapability The capability string for the BlueZ agent.
   */
  explicit Manager(std::string agentCapability);

  /**
   * @brief Destructor for Manager.
   */
  ~Manager();

  /**
   * @brief Retrieves the default Bluetooth adapter.
   * @param adapter Reference to store the default adapter.
   * @return Status indicating success or failure.
   */
  Status getDefaultAdapter(std::shared_ptr<bluetooth::Adapter>& adapter);

  /**
   * @brief Retrieves all available Bluetooth adapters.
   * @return A vector of shared pointers to adapters.
   */
  std::vector<std::shared_ptr<bluetooth::Adapter>> getAdapters();

  /**
   * @brief Unregisters the BlueZ agent that was registered by this Manager.
   *
   * Removes this application's agent from BlueZ. After this call BlueZ will
   * re-select a default agent from any other registered agents, or run with no
   * default agent if none remain. Safe to call when no agent is registered.
   */
  void unregisterAgent();

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;

  /**
   * @brief Authorized accessor for Device's protected reconnection-cooldown state.
   *
   * Device grants friendship to Manager (only). Since the pimpl-nested
   * Manager::Impl::Agent is a distinct class and not itself a friend of Device,
   * it must route protected access through this static helper, which is a member
   * of the befriended Manager type. Returns false for a null device.
   */
  static bool isDeviceInCooldown(const std::shared_ptr<Device>& device);
};

}  // namespace bluetooth



