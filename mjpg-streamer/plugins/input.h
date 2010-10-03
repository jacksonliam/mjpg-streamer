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

#include <linux/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>
#define INPUT_PLUGIN_PREFIX " i: "
#define IPRINT(...) { char _bf[1024] = {0}; snprintf(_bf, sizeof(_bf)-1, __VA_ARGS__); fprintf(stderr, "%s", INPUT_PLUGIN_PREFIX); fprintf(stderr, "%s", _bf); syslog(LOG_INFO, "%s", _bf); }

/* parameters for input plugin */
typedef struct _input_parameter input_parameter;
struct _input_parameter {
  char *parameter_string;
  struct _globals *global;
};


/* commands which can be send to the input plugin */
typedef enum _in_cmd_type in_cmd_type;
enum _in_cmd_type {
    IN_CMD_V4L2 = 0,
    IN_CMD_RESOLUTION = 1,
    IN_CMD_JPEG_QUALITY = 2,
};

typedef struct _input_control input_control;
struct _input_control {
    struct v4l2_queryctrl ctrl;
    int value;
    struct v4l2_querymenu *menuitems;
    struct v4l2_capability cap;
    in_cmd_type type;
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
  void *handle;
  input_parameter param;
  input_control *in_parameters;
  int parametercount;

  struct v4l2_jpegcompression jpegcomp;

  input_format *in_formats;
  int formatCount;
  int currentFormat; // holds the current format number

    int (*init)(input_parameter *);
    int (*stop)(void);
    int (*run)(void);
    int (*cmd)(int , int);
    int (*cmd_new)(__u32 control, __s32 value, __u32 type);
};
