
if (EXISTS /opt/vc/include)
    set(HAS_RASPI ON)
else()
    set(HAS_RASPI OFF)
endif()

MJPG_STREAMER_PLUGIN_OPTION(input_raspicam "Raspberry Pi input camera plugin"
                            ONLYIF HAS_RASPI)

if (PLUGIN_INPUT_RASPICAM)

    include_directories(/opt/vc/include)
    include_directories(/opt/vc/include/interface/vcos)
    include_directories(/opt/vc/include/interface/vcos/pthreads)
    include_directories(/opt/vc/include/interface/vmcs_host)
    include_directories(/opt/vc/include/interface/vmcs_host/linux)

    link_directories(/opt/vc/lib)

    MJPG_STREAMER_PLUGIN_COMPILE(input_raspicam input_raspicam.c)

    target_link_libraries(input_raspicam mmal_core mmal_util mmal_vc_client vcos bcm_host)

endif()
