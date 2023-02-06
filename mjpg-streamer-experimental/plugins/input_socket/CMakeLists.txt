
MJPG_STREAMER_PLUGIN_OPTION(input_socket "Socket input plugin" ONLYIF HAVE_SYS_INOTIFY_H)
MJPG_STREAMER_PLUGIN_COMPILE(input_socket input_socket.c)


