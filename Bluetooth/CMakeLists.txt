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

project(Bluetooth)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLUGIN_NAME Bluetooth)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_BLUETOOTH_STARTUPORDER "" CACHE STRING "To configure startup order of Bluetooth plugin")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Helpers REQUIRED)

set(BLUETOOTH_PLUGIN_SOURCES
        Bluetooth.cpp
        BluetoothDeviceManager.cpp
        BtAdapter.cpp
        DeviceRegistry.cpp
        Module.cpp
)

# FindBTMGR.cmake / FindBLUETOOTH_SDK.cmake live here; must be on the module path
# before any find_package() call below.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

# Production builds retain BTMgr in the main plugin and load the SDK backend
# module only when the real SDK is installed. Test builds select their backend
# explicitly and compile against repository-provided mocks.
set(BLUETOOTH_TEST_BACKEND "BTMGR" CACHE STRING "Bluetooth adapter backend for L1/L2 test builds (BTMGR or SDK)")
set_property(CACHE BLUETOOTH_TEST_BACKEND PROPERTY STRINGS BTMGR SDK)

if (RDK_SERVICES_L1_TEST OR RDK_SERVICE_L2_TEST)
    string(TOUPPER "${BLUETOOTH_TEST_BACKEND}" BLUETOOTH_TEST_BACKEND)
    if (NOT BLUETOOTH_TEST_BACKEND STREQUAL "BTMGR" AND NOT BLUETOOTH_TEST_BACKEND STREQUAL "SDK")
        message(FATAL_ERROR "Bluetooth plugin: BLUETOOTH_TEST_BACKEND must be BTMGR or SDK")
    endif()

    if (BLUETOOTH_TEST_BACKEND STREQUAL "SDK")
        message(STATUS "Bluetooth plugin: test build - using bluetooth-sdk mock backend")
        list(APPEND BLUETOOTH_PLUGIN_SOURCES
            BtSdkAdapterImpl.cpp
            DeviceTypeClassifier.cpp
            EventBridge.cpp
            AuthBridge.cpp
            ${CMAKE_SOURCE_DIR}/../entservices-connectivity/Tests/mocks/bluetoothSDK.cpp
        )
    else()
        message(STATUS "Bluetooth plugin: test build - using BTMgr mock backend")
        list(APPEND BLUETOOTH_PLUGIN_SOURCES BtMgrAdapterImpl.cpp)
    endif()
else()
    find_package(BLUETOOTH_SDK REQUIRED)
    find_package(BTMGR REQUIRED)

    list(APPEND BLUETOOTH_PLUGIN_SOURCES BtMgrAdapterImpl.cpp)
endif()


if (BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION)
    list(APPEND BLUETOOTH_PLUGIN_SOURCES BluetoothPersistenceAdapter.cpp)
endif ()

add_library(${MODULE_NAME} SHARED ${BLUETOOTH_PLUGIN_SOURCES})

if (NOT RDK_SERVICES_L1_TEST AND NOT RDK_SERVICE_L2_TEST)
    set(BLUETOOTH_SDK_BACKEND_TARGET ${MODULE_NAME}SdkBackend)
    add_library(${BLUETOOTH_SDK_BACKEND_TARGET} SHARED
        BtSdkBackend.cpp
        BtSdkAdapterImpl.cpp
        DeviceTypeClassifier.cpp
        EventBridge.cpp
        AuthBridge.cpp
    )
    target_include_directories(${BLUETOOTH_SDK_BACKEND_TARGET} PRIVATE ${BLUETOOTH_SDK_INCLUDE_DIRS})
    target_link_libraries(${BLUETOOTH_SDK_BACKEND_TARGET} PRIVATE
        ${BLUETOOTH_SDK_LIBRARIES}
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}::${NAMESPACE}Helpers
    )
    target_compile_definitions(${MODULE_NAME} PRIVATE
        BLUETOOTH_SDK_BACKEND_FILENAME="$<TARGET_FILE_NAME:${BLUETOOTH_SDK_BACKEND_TARGET}>"
    )
endif()

target_compile_definitions(${MODULE_NAME} PRIVATE MODULE_NAME=Plugin_${PLUGIN_NAME})

if (BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION)
    target_compile_definitions(${MODULE_NAME} PRIVATE BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION=1)
    if (BLUETOOTH_PERSISTENT_FILE_PATH)
        target_compile_definitions(${MODULE_NAME} PRIVATE BLUETOOTH_PERSISTENT_FILE_PATH="${BLUETOOTH_PERSISTENT_FILE_PATH}")
    endif ()
endif ()

if (RDK_SERVICE_L2_TEST)
    find_library(TESTMOCKLIB_LIBRARIES NAMES TestMocklib)
    if (TESTMOCKLIB_LIBRARIES)
        message ("linking mock libraries ${TESTMOCKLIB_LIBRARIES} library")
        target_link_libraries(${MODULE_NAME} PRIVATE ${TESTMOCKLIB_LIBRARIES})
    else (TESTMOCKLIB_LIBRARIES)
        message ("Require ${TESTMOCKLIB_LIBRARIES} library")
    endif (TESTMOCKLIB_LIBRARIES)
endif (RDK_SERVICE_L2_TEST)

# Per-backend link and include configuration.
if ((RDK_SERVICES_L1_TEST OR RDK_SERVICE_L2_TEST) AND BLUETOOTH_TEST_BACKEND STREQUAL "SDK")
    target_compile_definitions(${MODULE_NAME} PRIVATE BLUETOOTH_HAS_SDK=1)
    # Shadow the real bluetooth-sdk headers with test stubs.
    target_include_directories(${MODULE_NAME} BEFORE PRIVATE
        ${CMAKE_SOURCE_DIR}/../entservices-connectivity/Tests/mocks)
elseif ((RDK_SERVICES_L1_TEST OR RDK_SERVICE_L2_TEST) AND BLUETOOTH_TEST_BACKEND STREQUAL "BTMGR")
    target_compile_definitions(${MODULE_NAME} PRIVATE BLUETOOTH_HAS_BTMGR=1)
elseif (NOT RDK_SERVICES_L1_TEST AND NOT RDK_SERVICE_L2_TEST)
    target_compile_definitions(${MODULE_NAME} PRIVATE BLUETOOTH_HAS_BTMGR=1)
    target_include_directories(${MODULE_NAME} PRIVATE ${BTMGR_INCLUDE_DIRS})
    target_link_libraries(${MODULE_NAME} PRIVATE ${BTMGR_LIBRARIES} dl)
endif()

find_package(IARMBus)
if (IARMBUS_FOUND OR IARMBus_FOUND)
    target_include_directories(${MODULE_NAME} PRIVATE ${IARMBUS_INCLUDE_DIRS})
    target_link_libraries(${MODULE_NAME} PRIVATE ${IARMBUS_LIBRARIES})
endif()

target_link_libraries(${MODULE_NAME} PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}::${NAMESPACE}Helpers)

if(BLUETOOTH_AUDIO_SUPPORT)
    target_compile_definitions(${MODULE_NAME} PRIVATE BLUETOOTH_AUDIO_SUPPORT=1)
endif()

if (NOT RDK_SERVICES_L1_TEST)
    target_compile_options(${MODULE_NAME} PRIVATE -Wno-error=deprecated)
else()
    target_compile_definitions(${MODULE_NAME} PRIVATE RDK_SERVICES_L1_TEST=1)
endif ()

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

if (BLUETOOTH_SDK_BACKEND_TARGET)
    install(TARGETS ${BLUETOOTH_SDK_BACKEND_TARGET}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)
endif()

write_config(PLUGINS ${PLUGIN_NAME})
