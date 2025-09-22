set(PLUGIN_NAME BartonMatter)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)

add_library(${MODULE_NAME} SHARED
	BartonMatterPlugin.cpp
	BartonMatterImplementation.cpp
        Module.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES)

#target_include_directories(${MODULE_NAME} PRIVATE ../helpers $ENV{PKG_CONFIG_SYSROOT_DIR}/usr/include/jsruntime ${CMAKE_CURRENT_SOURCE_DIR} )

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../build/BartonMatter)

target_link_libraries(${MODULE_NAME} PRIVATE
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    ${NAMESPACE}Definitions::${NAMESPACE}Definitions
)

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
