# Calculator Plugin

set(PLUGIN_NAME Calculator)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_CALC_STARTUPORDER "" CACHE STRING "To configure startup order of Calculator plugin")

find_package(${NAMESPACE}Plugins REQUIRED)

add_library(${MODULE_NAME} SHARED
        Calc.cpp
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
)

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
