
add_definitions(-DLINUX -D_GNU_SOURCE -Wall)
    
MJPG_STREAMER_PLUGIN_OPTION(input_httpout "input from output_http")

MJPG_STREAMER_PLUGIN_COMPILE(input_httpout input_httpout.c)


