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
        # BtAdapter.cpp unconditionally instantiates both adapters as static
        # globals and picks one at runtime (see BtAdapter::ensureImpl()), so
        # both must always be compiled in, in every configuration.
        BtMgrAdapterImpl.cpp
        BtSdkAdapterImpl.cpp
        EventBridge.cpp
        AuthBridge.cpp
        DeviceTypeClassifier.cpp
)

# FindBTMGR.cmake / FindBLUETOOTH_SDK.cmake live here; must be on the module path
# before any find_package() call below.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

# BtSdkAdapterImpl uses the real bluetooth::Adapter/Device/Manager C++ types.
# The SDK team has agreed to refactor those public headers to drop the
# sdbus-c++/D-Bus dependency, but hasn't shipped that yet, so
# bluetooth-sdk-mock/ stands in for them in every configuration (test and
# production alike) — see bluetooth-sdk-mock/Status.h. Once the real headers
# ship, delete bluetooth-sdk-mock/ and point this at BLUETOOTH_SDK_INCLUDE_DIRS
# instead.
set(BLUETOOTH_TEST_BACKEND "BTMGR" CACHE STRING "Bluetooth adapter backend for L1/L2 test builds (BTMGR or SDK)")
set_property(CACHE BLUETOOTH_TEST_BACKEND PROPERTY STRINGS BTMGR SDK)

if (RDK_SERVICES_L1_TEST OR RDK_SERVICE_L2_TEST)
    string(TOUPPER "${BLUETOOTH_TEST_BACKEND}" BLUETOOTH_TEST_BACKEND)
    if (NOT BLUETOOTH_TEST_BACKEND STREQUAL "BTMGR" AND NOT BLUETOOTH_TEST_BACKEND STREQUAL "SDK")
        message(FATAL_ERROR "Bluetooth plugin: BLUETOOTH_TEST_BACKEND must be BTMGR or SDK")
    endif()
    message(STATUS "Bluetooth plugin: test build - exercising ${BLUETOOTH_TEST_BACKEND} backend (both are compiled; BLUETOOTH_TEST_BACKEND only selects the L2 test harness)")

    # BtSdkAdapterImpl.cpp is always compiled now (see above), so its Manager
    # symbols always need a definition in test builds too, regardless of which
    # backend a given test harness exercises via BtAdapter::setImpl().
    list(APPEND BLUETOOTH_PLUGIN_SOURCES
        ${CMAKE_SOURCE_DIR}/Tests/mocks/bluetoothSDK.cpp
    )
else()
    # Production builds now always link BTMGR and the real bluetooth-sdk
    # library, regardless of whether a given platform will use the SDK backend
    # at runtime (see BtAdapter::ensureImpl()/useSdkBackend()). Until the SDK
    # team ships their sdbus-c++-free header refactor, the library's currently
    # installed API doesn't match bluetooth-sdk-mock/, so this links but the
    # SDK backend isn't expected to resolve/work end-to-end yet.
    find_package(BTMGR REQUIRED)
    find_package(BLUETOOTH_SDK REQUIRED)

    message(STATUS "Bluetooth plugin: production build - compiling BTMGR adapter and SDK adapter, linking both backends")
endif()


if (BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION)
    list(APPEND BLUETOOTH_PLUGIN_SOURCES BluetoothPersistenceAdapter.cpp)
endif ()

add_library(${MODULE_NAME} SHARED ${BLUETOOTH_PLUGIN_SOURCES})



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
if (RDK_SERVICES_L1_TEST OR RDK_SERVICE_L2_TEST)
    target_compile_definitions(${MODULE_NAME} PRIVATE BLUETOOTH_HAS_SDK=1 BLUETOOTH_HAS_BTMGR=1)
    # BtSdkAdapterImpl.cpp is always compiled (see above), so its Manager
    # symbols always need Tests/mocks/bluetoothSDK.cpp's definitions, and its
    # bluetooth/*.h includes always need shadowing with GMock-friendly stubs,
    # regardless of which backend BLUETOOTH_TEST_BACKEND selects for the L2
    # test harness.
    target_include_directories(${MODULE_NAME} BEFORE PRIVATE
        ${CMAKE_SOURCE_DIR}/../entservices-connectivity/Tests/mocks)
else()
    # Production build: both backends are linked unconditionally now (see
    # BtAdapter::ensureImpl()). bluetooth-sdk-mock/ shadows the real
    # (currently sdbus-c++-based) bluetooth-sdk headers until the SDK team's
    # refactored ones ship; BLUETOOTH_SDK_LIBRARIES is still the real
    # librdk_bluetooth.so, so this links even though the ABI won't line up
    # end-to-end until then.
    target_compile_definitions(${MODULE_NAME} PRIVATE BLUETOOTH_HAS_BTMGR=1 BLUETOOTH_HAS_SDK=1)
    target_include_directories(${MODULE_NAME} BEFORE PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/bluetooth-sdk-mock)
    target_include_directories(${MODULE_NAME} PRIVATE ${BTMGR_INCLUDE_DIRS})
    target_link_libraries(${MODULE_NAME} PRIVATE ${BTMGR_LIBRARIES} ${BLUETOOTH_SDK_LIBRARIES})
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

write_config(PLUGINS ${PLUGIN_NAME})
