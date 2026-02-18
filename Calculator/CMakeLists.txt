set(PLUGIN_NAME Calculator)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

find_package(${NAMESPACE}Plugins REQUIRED)

add_library(${MODULE_NAME} SHARED
    Calculator.cpp
    Module.cpp
    src/add.cpp
    src/sub.cpp
    src/mul.cpp
    src/division.cpp
    src/mod.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
)

target_include_directories(${MODULE_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        $<TARGET_PROPERTY:${NAMESPACE}Plugins::${NAMESPACE}Plugins,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(${MODULE_NAME}
    PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
)

install(TARGETS ${MODULE_NAME}
    DESTINATION lib/${STORAGE_DIRECTORY}/plugins
)

write_config(${PLUGIN_NAME})
