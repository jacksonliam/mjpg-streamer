
MJPG_STREAMER_PLUGIN_OPTION(output_motion "Motion detection output plugin")
MJPG_STREAMER_PLUGIN_COMPILE(output_motion output_motion.c)

if (JPEG_LIB)
  target_link_libraries(output_motion ${JPEG_LIB})
endif (JPEG_LIB)
