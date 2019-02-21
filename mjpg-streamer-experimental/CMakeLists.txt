cmake_minimum_required(VERSION 2.8.3)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

project("mjpg-streamer" C)

# If the user doesn't manually specify a build type, use 'Release'
message("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  SET(CMAKE_BUILD_TYPE "Release")
endif()

SET(COMPILE_DEFINITIONS -Werror -Wall)

include(CheckLibraryExists) 
include(CheckIncludeFiles)
include(FeatureSummary)

include(mjpg_streamer_utils)

#
# Get the current git hash
#
execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  RESULT_VARIABLE GIT_RESULT
  OUTPUT_VARIABLE GIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(GIT_RESULT EQUAL 0)
  add_definitions("-DGIT_HASH=\"${GIT_HASH}\"")
endif()

#
# Options
#
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_feature_option(WXP_COMPAT "Enable compatibility with WebcamXP" OFF)

if (WXP_COMPAT)
    add_definitions(-DWXP_COMPAT)
endif (WXP_COMPAT)

set (MJPG_STREAMER_PLUGIN_INSTALL_PATH "lib/mjpg-streamer")

#
# Global dependencies
#

find_library(JPEG_LIB jpeg)


#
# Input plugins
#

add_subdirectory(plugins/input_file)
add_subdirectory(plugins/input_http)
add_subdirectory(plugins/input_opencv)
add_subdirectory(plugins/input_raspicam)
add_subdirectory(plugins/input_ptp2)
add_subdirectory(plugins/input_uvc)

#
# Output plugins
#

add_subdirectory(plugins/output_file)
add_subdirectory(plugins/output_http)
add_subdirectory(plugins/output_rtsp)
add_subdirectory(plugins/output_udp)
add_subdirectory(plugins/output_viewer)
add_subdirectory(plugins/output_zmqserver)

#
# mjpg_streamer executable
#

# This adds the plugin installation directory to the default DT_RUNPATH, so
# that the user shouldn't need to set LD_LIBRARY_PATH if using 'make install'
# ... however, DT_RUNPATH allows overriding via LD_LIBRARY_PATH if you really
#     need to do it

set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--enable-new-dtags")
set (CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set (CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/${MJPG_STREAMER_PLUGIN_INSTALL_PATH})
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


add_executable(mjpg_streamer mjpg_streamer.c
                             utils.c)

target_link_libraries(mjpg_streamer pthread dl)
install(TARGETS mjpg_streamer DESTINATION bin)

#
# www directory
#

install(DIRECTORY www DESTINATION share/mjpg-streamer)


#
# Show enabled/disabled features
#

feature_summary(WHAT ALL)

#
# Final warning
#

if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  message(WARNING "The source directory is the same as binary directory. \"make clean\" may damage the source tree")
endif()
