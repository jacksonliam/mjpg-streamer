/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include <syslog.h>
#include "../mjpg_streamer.h"
#define INPUT_PLUGIN_PREFIX " i: "
#define IPRINT(...) { char _bf[1024] = {0}; snprintf(_bf, sizeof(_bf)-1, __VA_ARGS__); fprintf(stderr, "%s", INPUT_PLUGIN_PREFIX); fprintf(stderr, "%s", _bf); syslog(LOG_INFO, "%s", _bf); }

/* parameters for input plugin */
typedef struct _input_parameter input_parameter;
struct _input_parameter {
    int id;
    char *parameters;
    int argc;
    char *argv[MAX_PLUGIN_ARGUMENTS];
    struct _globals *global;
};

typedef struct _input_resolution input_resolution;
struct _input_resolution {
    unsigned int width;
    unsigned int height;
};

typedef struct _input_format input_format;
struct _input_format {
    struct v4l2_fmtdesc format;
    input_resolution *supportedResolutions;
    int resolutionCount;
    char currentResolution;
};

/* structure to store variables/functions for input plugin */
typedef struct _input input;
struct _input {
    char *plugin;
    char *name;
    void *handle;

    input_parameter param; // this holds the command line arguments

    // input plugin parameters
    struct _control *in_parameters;
    int parametercount;


    struct v4l2_jpegcompression jpegcomp;

    /* signal fresh frames */
    pthread_mutex_t db;
    pthread_cond_t  db_update;

    /* global JPG frame, this is more or less the "database" */
    unsigned char *buf;
    int size;

    /* v4l2_buffer timestamp */
    struct timeval timestamp;

    input_format *in_formats;
    int formatCount;
    int currentFormat; // holds the current format number
    
    void *context; // private data for the plugin

    int (*init)(input_parameter *, int id);
    int (*stop)(int);
    int (*run)(int);
    int (*cmd)(int plugin, unsigned int control_id, unsigned int group, int value, char *value_str);
};
