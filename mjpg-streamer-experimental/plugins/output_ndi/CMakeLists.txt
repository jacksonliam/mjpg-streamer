
MJPG_STREAMER_PLUGIN_OPTION(output_ndi "NDI output stream plugin" ONLYIF JPEG_LIB )
MJPG_STREAMER_PLUGIN_COMPILE(output_ndi output_ndi.cpp)

target_link_libraries(output_ndi ndi  ${JPEG_LIB})
