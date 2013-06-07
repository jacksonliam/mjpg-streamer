add_library (mmal_core SHARED
   mmal_format.c
   mmal_port.c
   mmal_port_clock.c
   mmal_component.c
   mmal_buffer.c
   mmal_queue.c
   mmal_pool.c
   mmal_events.c
   mmal_logging.c
   mmal_clock.c
)

target_link_libraries (mmal_core vcos)

install(TARGETS mmal_core DESTINATION lib)
