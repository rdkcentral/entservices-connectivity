cmake_minimum_required(VERSION 3.5)
project(BluetoothSDKStubIntegration)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# Build the SDK-compatible stub without external dependencies.
option(AUDIO_SUPPORT "Enable Bluetooth audio API symbols (no WirePlumber linking required)" OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(BLUETOOTH_SDK_STUB_LIBRARY_TYPE SHARED)
else()
    set(BLUETOOTH_SDK_STUB_LIBRARY_TYPE STATIC)
endif()

add_library(rdk_bluetooth ${BLUETOOTH_SDK_STUB_LIBRARY_TYPE}
    stub/Adapter.cpp
    stub/Manager.cpp
    stub/Device.cpp
    stub/Advertisement.cpp
    stub/Audio.cpp
    stub/GattClient.cpp
    stub/GattServer.cpp
    stub/Uuid.cpp
)

target_compile_options(rdk_bluetooth PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter -Wno-error=type-limits -Wno-error=extra)
set_target_properties(rdk_bluetooth PROPERTIES POSITION_INDEPENDENT_CODE ON)

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

if(BLUETOOTH_SDK_STUB_LIBRARY_TYPE STREQUAL "SHARED")
    set_target_properties(rdk_bluetooth PROPERTIES
        VERSION 1.0.0
        SOVERSION 1
    )
endif()

# Installation target: only install when this CMakeLists.txt is configured as
# its own top-level project (e.g. the standalone bluetooth-sdk-stub recipe).
# When added as a subdirectory, the stub is static and linked into the plugin;
# installing a second shared library would create a runtime dependency.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    install(TARGETS rdk_bluetooth
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

    install(DIRECTORY include/bluetooth DESTINATION include)
endif()
