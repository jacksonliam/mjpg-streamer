# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)

# Check if GStreamer is available
if(GSTREAMER_FOUND)
    set(HAS_GSTREAMER ON)
else()
    set(HAS_GSTREAMER OFF)
endif()

MJPG_STREAMER_PLUGIN_OPTION(input_gstreamer "GStreamer input plugin"
                            ONLYIF HAS_GSTREAMER)

if (PLUGIN_INPUT_GSTREAMER)
    enable_language(CXX)
    
    # Include directories for GStreamer
    include_directories(${GSTREAMER_INCLUDE_DIRS})
    include_directories(/usr/include/gstreamer-1.0)

    # Compile the plugin
    MJPG_STREAMER_PLUGIN_COMPILE(input_gstreamer input_gstreamer.cpp)

    # Link the plugin against GStreamer libraries
    target_link_libraries(input_gstreamer ${GSTREAMER_LIBRARIES})
endif()
