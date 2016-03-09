#-------------------------------------------------------------------------------
# Copyright (c) 2013, Lars Baehren <lbaehren@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#  * Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#-------------------------------------------------------------------------------

# - Check for the presence of NumPy
#
# The following variables are set when NumPy is found:
#  NUMPY_FOUND      = Set to true, if all components of NUMPY have been found.
#  NUMPY_INCLUDES   = Include path for the header files of NUMPY
#  NUMPY_LIBRARIES  = Link these to use NUMPY
#  NUMPY_LFLAGS     = Linker flags (optional)

if (NOT NUMPY_FOUND)

    if (NOT NUMPY_ROOT_DIR)
        set (NUMPY_ROOT_DIR ${CMAKE_INSTALL_PREFIX})
    endif (NOT NUMPY_ROOT_DIR)

    if (NOT PYTHON_FOUND)
        find_package (PythonInterp)
    endif (NOT PYTHON_FOUND)

    ##__________________________________________________________________________
    ## Check for the header files

    ## Use Python to determine the include directory
    execute_process (
        COMMAND ${PYTHON_EXECUTABLE} -c import\ sys,\ numpy\;\ sys.stdout.write\(numpy.get_include\(\)\)\;
        ERROR_VARIABLE NUMPY_FIND_ERROR
        RESULT_VARIABLE NUMPY_FIND_RESULT
        OUTPUT_VARIABLE NUMPY_FIND_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    ## process the output from the execution of the command
    if (NOT NUMPY_FIND_RESULT)
        set (NUMPY_INCLUDES ${NUMPY_FIND_OUTPUT})
    endif (NOT NUMPY_FIND_RESULT)

    ##__________________________________________________________________________
    ## Check for the library

    unset (NUMPY_LIBRARIES)

    if (PYTHON_SITE_PACKAGES)
        find_library (NUMPY_NPYMATH_LIBRARY npymath
            HINTS ${PYTHON_SITE_PACKAGES}/numpy/core
            PATH_SUFFIXES lib
            )
        if (NUMPY_NPYMATH_LIBRARY)
            list (APPEND NUMPY_LIBRARIES ${NUMPY_NPYMATH_LIBRARY})
        endif (NUMPY_NPYMATH_LIBRARY)
    endif (PYTHON_SITE_PACKAGES)

    ##__________________________________________________________________________
    ## Get API version of NumPy from 'numpy/numpyconfig.h'

    if (PYTHON_EXECUTABLE)
        execute_process (
            COMMAND ${PYTHON_EXECUTABLE} -c import\ sys,\ numpy\;\ sys.stdout.write\(numpy.__version__\)\;
            ERROR_VARIABLE NUMPY_API_VERSION_ERROR
            RESULT_VARIABLE NUMPY_API_VERSION_RESULT
            OUTPUT_VARIABLE NUMPY_API_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            )
    else ()
        ## Backup procedure: extract version number directly from the header file
        if (NUMPY_INCLUDES)
            find_file (HAVE_NUMPYCONFIG_H numpy/numpyconfig.h
                HINTS ${NUMPY_INCLUDES}
                )
        endif (NUMPY_INCLUDES)
    endif ()

    ## Dissect full version number into major, minor and patch version
    if (NUMPY_API_VERSION)
        string (REGEX REPLACE "\\." ";" _tmp ${NUMPY_API_VERSION})
        list (GET _tmp 0 NUMPY_API_VERSION_MAJOR)
        list (GET _tmp 1 NUMPY_API_VERSION_MINOR)
        list (GET _tmp 2 NUMPY_API_VERSION_PATCH)
    endif (NUMPY_API_VERSION)

    ##__________________________________________________________________________
    ## Actions taken when all components have been found

    find_package_handle_standard_args (NUMPY DEFAULT_MSG NUMPY_INCLUDES)

    if (NUMPY_FOUND)
        if (NOT NUMPY_FIND_QUIETLY)
            message (STATUS "Found components for NumPy")
            message (STATUS "NUMPY_ROOT_DIR    = ${NUMPY_ROOT_DIR}")
            message (STATUS "NUMPY_INCLUDES    = ${NUMPY_INCLUDES}")
            message (STATUS "NUMPY_LIBRARIES   = ${NUMPY_LIBRARIES}")
            message (STATUS "NUMPY_API_VERSION = ${NUMPY_API_VERSION}")
        endif (NOT NUMPY_FIND_QUIETLY)
    else (NUMPY_FOUND)
        if (NUMPY_FIND_REQUIRED)
            message (FATAL_ERROR "Could not find NUMPY!")
        endif (NUMPY_FIND_REQUIRED)
    endif (NUMPY_FOUND)

    ##__________________________________________________________________________
    ## Mark advanced variables

  mark_as_advanced (
    NUMPY_ROOT_DIR
    NUMPY_INCLUDES
    NUMPY_LIBRARIES
    )

endif (NOT NUMPY_FOUND)
