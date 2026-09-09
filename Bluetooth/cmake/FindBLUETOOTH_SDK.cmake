# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# - Try to find the RDK Bluetooth SDK
# Once done this will define
#  BLUETOOTH_SDK_FOUND - System has the Bluetooth SDK
#  BLUETOOTH_SDK_INCLUDE_DIRS - The Bluetooth SDK include directories
#  BLUETOOTH_SDK_LIBRARIES - The libraries needed to use the Bluetooth SDK
#
# The Bluetooth SDK is provided by the bundled bluetooth-sdk-stub, which
# provides a stub implementation with SDK-compatible signatures.
#

# The Bluetooth SDK is provided by the bundled bluetooth-sdk-stub at a fixed
# location. This stub always exists in the source tree and is used unconditionally.
set(BLUETOOTH_SDK_STUB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bluetooth-sdk-stub")

if(NOT EXISTS "${BLUETOOTH_SDK_STUB_DIR}/include/bluetooth/Manager.h")
    message(FATAL_ERROR "BLUETOOTH_SDK: Could not find bundled stub at ${BLUETOOTH_SDK_STUB_DIR}")
endif()

message(STATUS "BLUETOOTH_SDK: Found bundled stub at ${BLUETOOTH_SDK_STUB_DIR}")

set(BLUETOOTH_SDK_INCLUDE_DIRS "${BLUETOOTH_SDK_STUB_DIR}/include")
set(BLUETOOTH_SDK_LIBRARIES "rdk_bluetooth")

set(BLUETOOTH_SDK_LIBRARIES ${BLUETOOTH_SDK_LIBRARIES} CACHE PATH "Path to Bluetooth SDK library")
set(BLUETOOTH_SDK_INCLUDE_DIRS ${BLUETOOTH_SDK_INCLUDE_DIRS} CACHE PATH "Path to Bluetooth SDK include")

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(BLUETOOTH_SDK DEFAULT_MSG BLUETOOTH_SDK_INCLUDE_DIRS BLUETOOTH_SDK_LIBRARIES)

mark_as_advanced(
        BLUETOOTH_SDK_FOUND
        BLUETOOTH_SDK_INCLUDE_DIRS
        BLUETOOTH_SDK_LIBRARIES)
