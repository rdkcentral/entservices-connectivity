cmake_minimum_required(VERSION 3.10)
project(BluetoothSDKStubIntegration)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# Build the fake librdk_bluetooth.so with no external dependencies
option(AUDIO_SUPPORT "Enable Bluetooth audio API symbols (no WirePlumber linking required)" OFF)

add_library(rdk_bluetooth SHARED
    stub/Adapter.cpp
    stub/Manager.cpp
    stub/Device.cpp
    stub/Advertisement.cpp
    stub/GattClient.cpp
    stub/GattServer.cpp
)

target_compile_options(rdk_bluetooth PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter -Wno-error=type-limits)

# AppleClang-specific: suppress unused private field warnings in stub
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(rdk_bluetooth PRIVATE -Wno-unused-private-field)
endif()

target_include_directories(rdk_bluetooth
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(rdk_bluetooth PUBLIC Threads::Threads)

# Enable audio API symbols (compile-time only; no WirePlumber linking needed for stub)
if(AUDIO_SUPPORT)
    target_compile_definitions(rdk_bluetooth PUBLIC AUDIO_SUPPORT)
endif()

# Library versioning
# VERSION: Full version number (1.0.0) -> librdk_bluetooth.so.1.0.0
# SOVERSION: ABI version (1) -> librdk_bluetooth.so.1 symlink for ABI compatibility
set_target_properties(rdk_bluetooth PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)

# Installation target (optional)
install(TARGETS rdk_bluetooth
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)

install(DIRECTORY include/bluetooth DESTINATION include)
