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
  IN_CMD_UNKNOWN = 0,
  IN_CMD_HELLO,
  IN_CMD_RESET,
  IN_CMD_RESET_PAN_TILT,
  IN_CMD_RESET_PAN_TILT_NO_MUTEX,
  IN_CMD_PAN_SET,
  IN_CMD_PAN_PLUS,
  IN_CMD_PAN_MINUS,
  IN_CMD_TILT_SET,
  IN_CMD_TILT_PLUS,
  IN_CMD_TILT_MINUS,
  IN_CMD_SATURATION_PLUS,
  IN_CMD_SATURATION_MINUS,
  IN_CMD_CONTRAST_PLUS,
  IN_CMD_CONTRAST_MINUS,
  IN_CMD_BRIGHTNESS_PLUS,
  IN_CMD_BRIGHTNESS_MINUS,
  IN_CMD_GAIN_PLUS,
  IN_CMD_GAIN_MINUS,
  IN_CMD_FOCUS_PLUS,
  IN_CMD_FOCUS_MINUS,
  IN_CMD_FOCUS_SET,
  IN_CMD_LED_ON,
  IN_CMD_LED_OFF,
  IN_CMD_LED_AUTO,
  IN_CMD_LED_BLINK
};

/* structure to store variables/functions for input plugin */
typedef struct _input input;
struct _input {
  char *plugin;
  void *handle;
  input_parameter param;

  int (*init)(input_parameter *);
  int (*stop)(void);
  int (*run)(void);
  int (*cmd)(in_cmd_type, int);
};
