include(FindZeroMQ)
include(FindProtobuf-c)

MJPG_STREAMER_PLUGIN_OPTION(output_zmqserver "ZMQ Server output plugin"
                            ONLYIF ZeroMQ_LIBRARY PROTOBUF_C_LIBRARY)


if (PLUGIN_OUTPUT_ZMQSERVER)
    protobuf_c_generate(PROTO_SRC PROTO_HEADER package.proto)
    include_directories(${ZeroMQ_INCLUDE_DIR})
    include_directories(${PROTOBUF_C_INCLUDE_DIR})
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    MJPG_STREAMER_PLUGIN_COMPILE(output_zmqserver ${PROTO_SRC} output_zmqserver.c)
    target_link_libraries(output_zmqserver ${ZeroMQ_LIBRARY} ${PROTOBUF_C_LIBRARY})
endif()
