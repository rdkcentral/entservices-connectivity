# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
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

set(PLUGIN_NAME Calculator)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

# Find Thunder / WPEFramework Plugins
find_package(${NAMESPACE}Plugins REQUIRED)

# Build the Calculator plugin as a shared library
add_library(${MODULE_NAME} SHARED
        Calculator.cpp
        Module.cpp
        src/add.cpp
        src/sub.cpp
        src/mul.cpp
        src/division.cpp
        src/mod.cpp
)

# C++ standard
set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
)

# Calculator local headers
target_include_directories(${MODULE_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against Thunder Plugins (required for plugins/plugins.h)
target_link_libraries(${MODULE_NAME}
    PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
)

# Install the plugin into Thunder plugins directory
install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins
)

# Generate plugin configuration
write_config(${PLUGIN_NAME})
