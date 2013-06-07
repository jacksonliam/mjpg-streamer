add_library(mmal_vc_client SHARED mmal_vc_client.c mmal_vc_shm.c mmal_vc_api.c mmal_vc_opaque_alloc.c mmal_vc_msgnames.c mmal_vc_api_drm.c)
target_link_libraries(mmal_vc_client vchiq_arm vcos)

if(BUILD_MMAL_APPS)
add_executable(mmal_vc_diag mmal_vc_diag.c)
target_link_libraries(mmal_vc_diag mmal mmal_vc_client debug_sym vcos)
endif(BUILD_MMAL_APPS)

install(TARGETS mmal_vc_client DESTINATION lib)
