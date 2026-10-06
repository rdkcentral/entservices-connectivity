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

#include <sdbus-c++/sdbus-c++.h>

/**
 * @file Connection.h
 * @brief D-Bus connection management utilities for the Bluetooth SDK
 */

namespace dbus {

/**
 * @brief Get the default system D-Bus connection
 * 
 * Retrieves a reference to the default system D-Bus connection instance.
 * This connection is typically used for communicating with system services
 * over the D-Bus message bus.
 * 
 * @return Reference to the system D-Bus connection interface
 * @throws sdbus::Error if connection cannot be established
 */
sdbus::IConnection& get_sdbus_connection();

/**
 * @brief Get a named D-Bus connection
 * 
 * Retrieves a reference to a named D-Bus connection instance. This allows
 * for establishing connections with specific service names on the D-Bus.
 * 
 * @return Reference to the named D-Bus connection interface
 * @throws sdbus::Error if named connection cannot be established
 */
sdbus::IConnection& get_sdbus_named_connection();

}  // namespace dbus
