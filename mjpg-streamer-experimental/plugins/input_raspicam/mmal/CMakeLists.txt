
add_definitions(-Wall -Werror)

add_library(mmal SHARED util/mmal_util.c)

add_subdirectory(core)
add_subdirectory(util)
add_subdirectory(vc)

#add_subdirectory(test/lua)
#add_subdirectory(test/standalone)

if(BUILD_MMAL_APPS)
add_subdirectory(components)
target_link_libraries(mmal mmal_core mmal_util mmal_vc_client vcos mmal_components)
else(BUILD_MMAL_APPS)
target_link_libraries(mmal mmal_core mmal_util mmal_vc_client)
endif(BUILD_MMAL_APPS)

install(TARGETS mmal DESTINATION lib)
