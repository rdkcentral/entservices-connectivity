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

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLUGIN_NAME Bluetooth)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_BLUETOOTH_STARTUPORDER "" CACHE STRING "To configure startup order of Bluetooth plugin")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Helpers REQUIRED)

set(BLUETOOTH_PLUGIN_SOURCES
        Bluetooth.cpp
        BluetoothDeviceManager.cpp
        Module.cpp
)

if (BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION)
    list(APPEND BLUETOOTH_PLUGIN_SOURCES BluetoothPersistenceAdapter.cpp)
endif ()

add_library(${MODULE_NAME} SHARED ${BLUETOOTH_PLUGIN_SOURCES})

if (BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION)
    target_compile_definitions(${MODULE_NAME} PRIVATE BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION=1)
    if (BLUETOOTH_PERSISTENT_FILE_PATH)
        target_compile_definitions(${MODULE_NAME} PRIVATE BLUETOOTH_PERSISTENT_FILE_PATH="${BLUETOOTH_PERSISTENT_FILE_PATH}")
    endif ()
endif ()

list(APPEND CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

if (RDK_SERVICE_L2_TEST)
    find_library(TESTMOCKLIB_LIBRARIES NAMES TestMocklib)
    if (TESTMOCKLIB_LIBRARIES)
        message ("linking mock libraries ${TESTMOCKLIB_LIBRARIES} library")
        target_link_libraries(${MODULE_NAME} PRIVATE ${TESTMOCKLIB_LIBRARIES})
    else (TESTMOCKLIB_LIBRARIES)
        message ("Require ${TESTMOCKLIB_LIBRARIES} library")
    endif (TESTMOCKLIB_LIBRARIES)
endif (RDK_SERVICE_L2_TEST)

find_package(BTMGR)
if(BTMGR_FOUND)
    message("Found BTMGR")
    target_include_directories(${MODULE_NAME} PRIVATE ${BTMGR_INCLUDE_DIRS})
    target_link_libraries(${MODULE_NAME} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins ${BTMGR_LIBRARIES})
endif(BTMGR_FOUND)

find_package(IARMBus)
target_include_directories(${MODULE_NAME} PRIVATE ${IARMBUS_INCLUDE_DIRS})
target_link_libraries(${MODULE_NAME} PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${IARMBUS_LIBRARIES}
        ${NAMESPACE}::${NAMESPACE}Helpers)

if (NOT RDK_SERVICES_L1_TEST)
    target_compile_options(${MODULE_NAME} PRIVATE -Wno-error=deprecated)
endif ()

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(PLUGINS ${PLUGIN_NAME})
