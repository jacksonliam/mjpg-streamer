cmake_minimum_required(VERSION 2.8)

SET(COMPILE_DEFINITIONS -Werror)

include_directories(/opt/vc/include)
include_directories(/opt/vc/include/interface/vcos)
include_directories(/opt/vc/include/interface/vcos/pthreads)
include_directories(/opt/vc/include/interface/vmcs_host)
include_directories(/opt/vc/include/interface/vmcs_host/linux)

set(CMAKE_C_FLAGS "-DDEBUG")

link_directories(/opt/vc/lib)

add_library(input_raspicam SHARED input_raspicam.c)

target_link_libraries(input_raspicam mmal_core mmal_util mmal_vc_client vcos bcm_host)
