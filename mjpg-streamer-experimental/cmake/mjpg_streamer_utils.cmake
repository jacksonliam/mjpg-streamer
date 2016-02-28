
include(CMakeDependentOption)
include(CMakeParseArguments)

#
# Adds an option to compile an mjpg-streamer plugin, but only enables if a set
# of requirements are met.
#
# First arg: module name
# Second arg: description
# Optional args: ONLYIF Condition1 [Condition2 ...]
#
macro(MJPG_STREAMER_PLUGIN_OPTION MODULE_NAME DESCRIPTION)

	cmake_parse_arguments(MSPOM
						  "" "" "ONLYIF" ${ARGN})
					  
	string(TOUPPER "PLUGIN_${MODULE_NAME}" OPT_ENABLE)
	string(TOUPPER "PLUGIN_${MODULE_NAME}_AVAILABLE" OPT_AVAILABLE)
	
	cmake_dependent_option(${OPT_ENABLE} "${DESCRIPTION}" ON "${MSPOM_ONLYIF}" OFF)
	
	if (${OPT_AVAILABLE})
		add_feature_info(${OPT_ENABLE} ${OPT_ENABLE} "${DESCRIPTION}")
	else()
		add_feature_info(${OPT_ENABLE} ${OPT_ENABLE} "${DESCRIPTION} (unmet dependencies)")
	endif()

endmacro()

#
# Conditionally compiles an mjpg-streamer plugin, must use
# MJPG_STREAMER_PLUGIN_OPTION first
# 
# First arg: module name,
# other args: source files
#
macro(MJPG_STREAMER_PLUGIN_COMPILE MODULE_NAME)
	
	string(TOUPPER "${MODULE_NAME}" ARGU)
	set(OPT_ENABLE "PLUGIN_${ARGU}")
	
	if (${OPT_ENABLE})
	
		set(MOD_SRC)
		foreach(arg ${ARGN})
			list(APPEND MOD_SRC "${arg}") 
		endforeach()
		
		add_library(${MODULE_NAME} SHARED ${MOD_SRC})
	    set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")
	    
		install(TARGETS ${MODULE_NAME} DESTINATION ${MJPG_STREAMER_PLUGIN_INSTALL_PATH})	
	endif()
	
endmacro()


macro(add_feature_option OPTVAR DESCRIPTION DEFAULT)
	option(${OPTVAR} "$DESCRIPTION" ${DEFAULT})
	add_feature_info(${OPTVAR} ${OPTVAR} "${DESCRIPTION}")
endmacro()
