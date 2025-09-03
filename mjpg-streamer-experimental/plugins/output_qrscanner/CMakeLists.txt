# Find required packages
find_package(PkgConfig REQUIRED)

# Check for quirc library (required QR decoder)
# Add explicit hints for quirc library locations
find_path(QUIRC_INCLUDE_DIR quirc.h)
find_library(QUIRC_LIBRARY NAMES quirc libquirc)

# Debug output
message(STATUS "QUIRC_INCLUDE_DIR: ${QUIRC_INCLUDE_DIR}")
message(STATUS "QUIRC_LIBRARY: ${QUIRC_LIBRARY}")

# Check for libjpeg (required for JPEG decoding)
find_package(JPEG)

# Build plugin only if all required dependencies are available
if(QUIRC_INCLUDE_DIR AND QUIRC_LIBRARY AND JPEG_FOUND)
    message(STATUS "QR scanner plugin: All dependencies found, building plugin")
    MJPG_STREAMER_PLUGIN_OPTION(output_qrscanner "QR code scanner output plugin with external program execution")

    if (PLUGIN_OUTPUT_QRSCANNER)
        MJPG_STREAMER_PLUGIN_COMPILE(output_qrscanner output_qrscanner.c)

        target_include_directories(output_qrscanner PRIVATE ${QUIRC_INCLUDE_DIR} ${JPEG_INCLUDE_DIR})
        target_link_libraries(output_qrscanner ${QUIRC_LIBRARY} ${JPEG_LIBRARIES})
    endif()
else()
    message(STATUS "QR scanner output plugin disabled - missing required dependencies:")
    if(NOT QUIRC_INCLUDE_DIR OR NOT QUIRC_LIBRARY)
        message(STATUS "  - quirc library not found (QUIRC_INCLUDE_DIR: ${QUIRC_INCLUDE_DIR}, QUIRC_LIBRARY: ${QUIRC_LIBRARY})")
    endif()
    if(NOT JPEG_FOUND)
        message(STATUS "  - JPEG library not found (libjpeg-dev)")
    endif()
endif()
