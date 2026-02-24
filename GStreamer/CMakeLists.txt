set (PLUGIN_NAME GStreamer)
set (MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_GSTREAMER_STARTUPORDER "" CACHE STRING "To configure startup order of GStreamer plugin")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(GStreamer REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER_BASE REQUIRED gstreamer-base-1.0)
pkg_check_modules(GSTREAMER_AUDIO REQUIRED gstreamer-audio-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)

add_library(${MODULE_NAME} SHARED 
        GStreamer.cpp
        Module.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

target_include_directories(${MODULE_NAME} PRIVATE
        ../helpers
        ${GSTREAMER_BASE_INCLUDE_DIRS}
        ${GSTREAMER_AUDIO_INCLUDE_DIRS}
        ${GSTREAMER_VIDEO_INCLUDE_DIRS}
)

target_link_libraries(${MODULE_NAME}
        PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        GStreamer::GStreamer
        ${GSTREAMER_BASE_LIBRARIES}
        ${GSTREAMER_AUDIO_LIBRARIES}
        ${GSTREAMER_VIDEO_LIBRARIES}
)

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})