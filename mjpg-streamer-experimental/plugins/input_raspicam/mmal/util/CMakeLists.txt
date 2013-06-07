add_library (mmal_util SHARED
   mmal_il.c
   mmal_util.c
   mmal_connection.c
   mmal_graph.c
   mmal_list.c
   mmal_param_convert.c
   mmal_util_params.c
   mmal_component_wrapper.c
   mmal_util_rational.c
)

target_link_libraries (mmal_util vcos)

install(TARGETS mmal_util DESTINATION lib)
