cmake_minimum_required(VERSION 3.10.0)

# Add cmake modules of this project to the module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# with at least one base tag, this can have dynamic (git) versioning:
#  git describe last tag = 1.1.0
#  git describe last tag + rev count = 1.1.0.45
#
# for now, set -DSCM_VERSION_INFO in the build env to override
if(NOT SCM_VERSION_INFO)
    set(PACKAGE_VERSION "1.0.0")
    set(SCM_VERSION_INFO ${PACKAGE_VERSION})
endif()

project(mjpg-streamer
    LANGUAGES C
    VERSION ${SCM_VERSION_INFO}
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

# eventually this should have some tests
option(BUILD_TESTING "build and run tests" ON)
option(ENABLE_WXP_COMPAT "" OFF)
option(ENABLE_HTTP_MANAGEMENT "" OFF)

# If the user doesn't manually specify a build type, use 'Release'
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE
        "Release"
        CACHE STRING "Default build type: ${CMAKE_BUILD_TYPE}" FORCE
    )
endif()
message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

include(CheckLibraryExists)
include(CheckIncludeFiles)
include(FeatureSummary)
include(CTest)
include(GNUInstallDirs)

set(MJPG_STREAMER_PLUGIN_INSTALL_PATH
    "${CMAKE_INSTALL_LIBDIR}/mjpg-streamer/${CMAKE_INSTALL_LIBDIR}"
    CACHE PATH "Install directory for plugin (.so) files"
)

set(MJPG_STREAMER_WWW_ROOT
    "${CMAKE_INSTALL_DATAROOTDIR}/mjpg-streamer"
    CACHE PATH "Install directory for example www server root"
)

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
    add_compile_definitions(GIT_HASH=\"${GIT_HASH}\")
endif()

#
# Options
#
# SCM_VERSION_INFO can be defined by cmake args and passed into the code as a
# define here (see PACKAGE_VERSION near the top of this file)
add_compile_definitions(SOURCE_VERSION=\"${SCM_VERSION_INFO}\")

add_compile_options(-Wall)

add_feature_option(WXP_COMPAT "Enable compatibility with WebcamXP" ${ENABLE_WXP_COMPAT})

if (WXP_COMPAT)
    add_definitions(-DWXP_COMPAT)
endif (WXP_COMPAT)

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

add_executable(mjpg_streamer mjpg_streamer.c
                             utils.c)

target_link_libraries(mjpg_streamer pthread dl)

install(TARGETS mjpg_streamer
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

if(BUILD_TESTING)
    enable_testing()

    # tell ctest to run main app as a test; note this is mainly for illustration
    add_test(NAME main_test
        COMMAND mjpg_streamer --version
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()

# what to do with this...
# MJPG_STREAMER_PLUGIN_INSTALL_PATH

#
# www directory
#

install(DIRECTORY ${CMAKE_SOURCE_DIR}/www DESTINATION "${MJPG_STREAMER_WWW_ROOT}")

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
