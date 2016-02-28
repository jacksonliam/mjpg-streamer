
add_feature_option(ENABLE_HTTP_MANAGEMENT "Enable experimental HTTP management option" OFF)

if (ENABLE_HTTP_MANAGEMENT)
    add_definitions(-DMANAGMENT)
endif (ENABLE_HTTP_MANAGEMENT)

add_definitions(-D_GNU_SOURCE)

MJPG_STREAMER_PLUGIN_OPTION(output_http "HTTP server output plugin")
MJPG_STREAMER_PLUGIN_COMPILE(output_http httpd.c output_http.c)
