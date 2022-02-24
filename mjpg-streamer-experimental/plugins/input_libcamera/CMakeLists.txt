find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA libcamera)
find_library(LIBCAMERA_LIBRARY libcamera.so)
find_library(LIBCAMERA_BASE_LIBRARY libcamera-base.so)
set(LIBCAMERA_LIBRARIES "${LIBCAMERA_LIBRARY}" "${LIBCAMERA_BASE_LIBRARY}")

find_library(JPEG_LIB jpeg)

MJPG_STREAMER_PLUGIN_OPTION(input_libcamera "Libcamera input plugin"
                            ONLYIF LIBCAMERA_FOUND JPEG_LIB)

if (PLUGIN_INPUT_LIBCAMERA)
    message("PLUGIN_INPUT_LIBCAMERA")
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 17)
    include_directories(${LIBCAMERA_INCLUDE_DIRS})

    MJPG_STREAMER_PLUGIN_COMPILE(input_libcamera input_libcamera.cpp LibCamera.cpp jpeg_utils.c)

    target_link_libraries(input_libcamera ${LIBCAMERA_LIBRARIES} ${JPEG_LIB})
    
endif()


