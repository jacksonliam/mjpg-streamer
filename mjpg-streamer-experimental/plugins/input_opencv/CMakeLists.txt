
# TODO: which components do I need?
# To fix the error: "undefined symbol: _ZN2cv12VideoCaptureC1Ev"
find_package(OpenCV COMPONENTS core imgproc imgcodecs highgui videoio)


set(TARGET_ARCH_ABI "armv8")
set(PADDLE_LITE_DIR "../../Paddle-Lite")
message(STATUS "TARGET ARCH ABI: ${TARGET_ARCH_ABI}")
message(STATUS "PADDLE LITE DIR: ${PADDLE_LITE_DIR}")
include_directories(${PADDLE_LITE_DIR}/include)
link_directories(${PADDLE_LITE_DIR}/libs/${TARGET_ARCH_ABI})

MJPG_STREAMER_PLUGIN_OPTION(input_opencv "OpenCV input plugin")

if (PLUGIN_INPUT_OPENCV)
    enable_language(CXX)
    include_directories(${OpenCV_INCLUDE_DIRS})
    message(STATUS "OpenCV library status:")
    message(STATUS "    version: ${OpenCV_VERSION}")
    message(STATUS "    libraries: ${OpenCV_LIBS}")
    message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")

    MJPG_STREAMER_PLUGIN_COMPILE(input_opencv input_opencv.cpp)

    link_libraries(paddle_light_api_shared ${OpenCV_LIBS})
    target_link_libraries(input_opencv paddle_light_api_shared ${OpenCV_LIBS})

    add_subdirectory(filters/cvfilter_cpp)
    add_subdirectory(filters/cvfilter_py)

endif()
