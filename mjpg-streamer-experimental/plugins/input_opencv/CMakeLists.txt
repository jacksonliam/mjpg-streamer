
# TODO: which components do I need?
# To fix the error: "undefined symbol: _ZN2cv12VideoCaptureC1Ev"
find_package(OpenCV COMPONENTS core imgproc highgui videoio)

MJPG_STREAMER_PLUGIN_OPTION(input_opencv "OpenCV input plugin"
                            ONLYIF OpenCV_FOUND)

if (PLUGIN_INPUT_OPENCV)
    include_directories(${OpenCV_INCLUDE_DIRS})

    MJPG_STREAMER_PLUGIN_COMPILE(input_opencv input_opencv.cpp)
    
    target_link_libraries(input_opencv ${OpenCV_LIBS})
    
    add_subdirectory(filters/cvfilter_cpp)
    add_subdirectory(filters/cvfilter_py)
    
endif()


