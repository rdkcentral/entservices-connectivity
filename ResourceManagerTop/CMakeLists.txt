#top plugin

set(PLUGIN_NAME ResourceManagerTop)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_TOP_STARTUPORDER "" CACHE STRING "To configure startup order of TOP plugin")

find_package(${NAMESPACE}Plugins     REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)

add_library(${MODULE_NAME} SHARED
        ResourceManagerTop.cpp
        Module.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

target_include_directories(${MODULE_NAME} PRIVATE
        ../helpers
)

target_link_libraries(${MODULE_NAME}
        PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
)

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})

#
# write_config(Calculator)
#        |
#        ├── reads  Calculator.config        (sets CMake variables)
#        └── reads  Calculator.conf.in
#                |
#                replaces @PLUGIN_CALCULATOR_STARTUPORDER@ etc.
#                |
#                └── generates  Calculator.json   ← Thunder reads this at boot
# 