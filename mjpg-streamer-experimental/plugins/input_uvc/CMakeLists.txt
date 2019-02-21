
check_include_files(linux/videodev2.h HAVE_LINUX_VIDEODEV2_H)

MJPG_STREAMER_PLUGIN_OPTION(input_uvc "Video 4 Linux input plugin"
                            ONLYIF HAVE_LINUX_VIDEODEV2_H)

if (PLUGIN_INPUT_UVC)
    
    add_definitions(-DLINUX -D_GNU_SOURCE)
    
    find_library(V4L2_LIB v4l2)
    find_library(JPEG_LIB jpeg)
    
    if (V4L2_LIB)
        add_definitions(-DUSE_LIBV4L2)
    endif (V4L2_LIB)
    
    if (NOT JPEG_LIB)
        add_definitions(-DNO_LIBJPEG)
    endif (NOT JPEG_LIB)

    MJPG_STREAMER_PLUGIN_COMPILE(input_uvc dynctrl.c
                                           input_uvc.c
                                           jpeg_utils.c
                                           v4l2uvc.c)

    if (V4L2_LIB)
        target_link_libraries(input_uvc ${V4L2_LIB})
    endif (V4L2_LIB)

    if (JPEG_LIB)
        target_link_libraries(input_uvc ${JPEG_LIB})
    endif (JPEG_LIB)

endif()
