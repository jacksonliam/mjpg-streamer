#  PROTOBUF_C_FOUND - System has libprotobuf-c
#  PROTOBUF_C_INCLUDE_DIRS - The libprotobuf-c include directories
#  PROTOBUF_C_LIBRARIES - The libraries needed to use libprotobuf-c
#  PROTOBUF_C_DEFINITIONS - Compiler switches required for using libprotobuf-c

function(PROTOBUF_C_GENERATE SRCS HDRS)
    if(NOT ARGN)
        message(SEND_ERROR "Error: PROTOBUFC_GENERATE() called without any proto files")
        return()
    endif()

    if(PROTOBUF_C_GENERATE_APPEND_PATH)
        # Create an include path for each file specified
        foreach(FIL ${ARGN})
            get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
            get_filename_component(ABS_PATH ${ABS_FIL} PATH)
            list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
            if(${_contains_already} EQUAL -1)
                list(APPEND _protobuf_include_path -I ${ABS_PATH})
            endif()
        endforeach()
    else()
        set(_protobuf_include_path -I ${CMAKE_CURRENT_SOURCE_DIR})
    endif()

    if(DEFINED PROTOBUFC_IMPORT_DIRS)
        foreach(DIR ${PROTOBUF_IMPORT_DIRS})
            get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
            list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
            if(${_contains_already} EQUAL -1)
                list(APPEND _protobuf_include_path -I ${ABS_PATH})
            endif()
        endforeach()
    endif()

    set(${SRCS})
    set(${HDRS})
    foreach(FIL ${ARGN})
        get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
        get_filename_component(FIL_WE ${FIL} NAME_WE)

        list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb-c.c")
        list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb-c.h")

        add_custom_command(
                OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb-c.c"
                "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb-c.h"
                COMMAND  ${PROTOBUFC_PROTOC_EXECUTABLE}
                ARGS --c_out  ${CMAKE_CURRENT_BINARY_DIR} ${_protobuf_include_path} ${ABS_FIL}
                DEPENDS ${ABS_FIL}
                COMMENT "Running C protocol buffer compiler on ${FIL}"
                VERBATIM )
    endforeach()

    set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
    set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction()

# Find the protoc Executable
find_program(PROTOBUFC_PROTOC_EXECUTABLE
        NAMES protoc-c
        DOC "protoc-c compiler"
        )

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_PROTOBUF_C QUIET protobuf-c)
    set(PROTOBUF_C_DEFINITIONS ${PC_PROTOBUF_C_CFLAGS_OTHER})
endif()

find_path(PROTOBUF_C_INCLUDE_DIR protobuf-c/protobuf-c.h
        HINTS ${PC_PROTOBUF_C_INCLUDEDIR} ${PC_PROTOBUF_C_INCLUDE_DIRS}
        PATH_SUFFIXES protobuf-c)

find_library(PROTOBUF_C_LIBRARY NAMES protobuf-c
        HINTS ${PC_PROTOBUF_C_LIBDIR} ${PC_PROTOBUF_C_LIBRARY_DIRS})

set(PROTOBUF_C_LIBRARIES ${PROTOBUF_C_LIBRARY} )
set(PROTOBUF_C_INCLUDE_DIRS ${PROTOBUF_C_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(protobuf-c  DEFAULT_MSG
        PROTOBUF_C_LIBRARY PROTOBUF_C_INCLUDE_DIR)

mark_as_advanced(PROTOBUF_C_INCLUDE_DIR YANG_LIBRARY)
