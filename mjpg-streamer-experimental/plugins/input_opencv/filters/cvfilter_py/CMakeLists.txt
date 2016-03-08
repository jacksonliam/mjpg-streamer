
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Required for old versions of cmake
set(Python_ADDITIONAL_VERSIONS 3.4 3.5)

# find python and numpy
find_package(PythonLibs)
find_package(Numpy)

MJPG_STREAMER_PLUGIN_OPTION(cvfilter_py "OpenCV python filter"
                            ONLYIF PYTHONLIBS_FOUND NUMPY_FOUND)
                            
if (PLUGIN_CVFILTER_PY)
    include_directories(${PYTHON_INCLUDE_DIRS})
    include_directories(${NUMPY_INCLUDES})
    

    MJPG_STREAMER_PLUGIN_COMPILE(cvfilter_py filter_py.cpp conversion.cpp)
    
    target_link_libraries(cvfilter_py ${OpenCV_LIBS})
    target_link_libraries(cvfilter_py ${PYTHON_LIBRARIES})
endif()
