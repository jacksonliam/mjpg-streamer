
add_definitions(-DLINUX -D_GNU_SOURCE)
    
MJPG_STREAMER_PLUGIN_OPTION(input_event "MM Event Player")

MJPG_STREAMER_PLUGIN_COMPILE(input_event input_event.c)


