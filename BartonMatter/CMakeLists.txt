set(PLUGIN_NAME BartonMatter)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0 gobject-2.0)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable -Wno-error=unused-variable" CACHE STRING "" FORCE)

add_library(${MODULE_NAME} SHARED
	BartonMatterPlugin.cpp
	BartonMatterImplementation.cpp
	MatterClusterDelegates.cpp
	NetworkCommissioningDriver.cpp
        Module.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES)

find_path(BARTON_INCLUDE_DIR
    NAMES barton-core-client.h
    PATHS
        ${CMAKE_SYSROOT}/usr/include/barton
        ${CMAKE_SYSROOT}/usr/include
    NO_DEFAULT_PATH
)

# Library path (libBartonCore.so)
find_library(BARTON_CORE_LIB
    NAMES BartonCore
    PATHS
        ${CMAKE_SYSROOT}/usr/lib
        ${CMAKE_SYSROOT}/usr/lib64
        ${CMAKE_SYSROOT}/lib
    NO_DEFAULT_PATH
)

if(NOT BARTON_INCLUDE_DIR)
    message(FATAL_ERROR "barton-core-client.h not found in sysroot!")
else()
    message(STATUS "Found Barton headers in: ${BARTON_INCLUDE_DIR}")
endif()

if(NOT BARTON_CORE_LIB)
    message(FATAL_ERROR "libBartonCore.so not found in sysroot!")
else()
    message(STATUS "Found Barton lib: ${BARTON_CORE_LIB}")
endif()
# --- Done ---
target_compile_definitions(${MODULE_NAME} PRIVATE
    CHIP_SYSTEM_CONFIG_USE_SOCKETS=1
    CHIP_HAVE_CONFIG_H=1
)
target_include_directories(${MODULE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../helpers
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BARTON_INCLUDE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${CMAKE_SYSROOT}/usr/include/matter
)

#target_include_directories(${MODULE_NAME} PRIVATE ../helpers $ENV{PKG_CONFIG_SYSROOT_DIR}/usr/include/jsruntime ${CMAKE_CURRENT_SOURCE_DIR} )
target_link_libraries(${MODULE_NAME} PRIVATE
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    ${NAMESPACE}Definitions::${NAMESPACE}Definitions
    ${BARTON_CORE_LIB}
    ${GLIB_LIBRARIES}
)

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
