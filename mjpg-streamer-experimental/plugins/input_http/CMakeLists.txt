
find_library(EXIF_LIB exif)

MJPG_STREAMER_PLUGIN_OPTION(input_http "HTTP input proxy plugin"
                            ONLYIF JPEG_LIB EXIF_LIB)

MJPG_STREAMER_PLUGIN_COMPILE(input_http input_http.c misc.c mjpg-proxy.c libepeg/epeg_main.c)

if (PLUGIN_INPUT_HTTP)
    target_link_libraries(input_http ${JPEG_LIB} ${EXIF_LIB})
endif()
