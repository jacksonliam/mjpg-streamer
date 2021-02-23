
check_include_files(turbojpeg.h HAVE_TURBOJPEG_H)

MJPG_STREAMER_PLUGIN_OPTION(output_modect "Motion Detection Plugin"
                            ONLYIF HAVE_TURBOJPEG_H)

add_feature_option(ENABLE_MM_FRONTEND "Enable MM system interface" OFF)

if (ENABLE_MM_FRONTEND)
    add_definitions(-DMM_FRONTEND)
endif (ENABLE_MM_FRONTEND)

if (PLUGIN_OUTPUT_MODECT)
  
    add_definitions(-DLINUX -D_GNU_SOURCE -Wall)

    MJPG_STREAMER_PLUGIN_COMPILE(output_modect output_modect.c
                                               modect_funcs.c
                                               fileio.c
                                               motion_status.c)

    target_link_libraries(output_modect turbojpeg)

endif()

