
find_package(SDL)

MJPG_STREAMER_PLUGIN_OPTION(output_viewer "SDL output viewer plugin"
                            ONLYIF JPEG_LIB SDL_FOUND)

if (PLUGIN_OUTPUT_VIEWER)
    include_directories(${SDL_INCLUDE_DIR})
    MJPG_STREAMER_PLUGIN_COMPILE(output_viewer output_viewer.c)
    target_link_libraries(output_viewer ${SDL_LIBRARY} ${JPEG_LIB})
endif()
