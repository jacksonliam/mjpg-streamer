## load in pkg-config support
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    ## use pkg-config to get hints for 0mq locations
    pkg_check_modules(PC_ZeroMQ QUIET zmq)
    set(PROTOBUF_C_DEFINITIONS ${PC_PROTOBUF_C_CFLAGS_OTHER})
endif()

## use the hint from above to find where 'zmq.hpp' is located
find_path(ZeroMQ_INCLUDE_DIR
        NAMES zmq.h
        HINTS ${PC_ZeroMQ_INCLUDEDIR} ${PC_ZeroMQ_INCLUDE_DIRS}
        )

## use the hint from about to find the location of libzmq
find_library(ZeroMQ_LIBRARY
        NAMES zmq
        HINTS ${PC_ZeroMQ_C_LIBDIR} ${PC_ZeroMQ_C_LIBRARY_DIRS}
        )
