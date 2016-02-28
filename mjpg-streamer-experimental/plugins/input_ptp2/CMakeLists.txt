
find_package(Gphoto2)

MJPG_STREAMER_PLUGIN_OPTION(input_ptp2 "PTP2 input plugin"
                            ONLYIF GPHOTO2_FOUND)

if (PLUGIN_INPUT_PTP2)
    include_directories(${GPHOTO2_INCLUDE_DIR})

    MJPG_STREAMER_PLUGIN_COMPILE(input_ptp2 input_ptp2.c)
    
    target_link_libraries(input_ptp2 ${GPHOTO2_LIBRARIES})
    
endif()



