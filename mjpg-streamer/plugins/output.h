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

#define OUTPUT_PLUGIN_PREFIX " o: "
#define OPRINT(...) { char _bf[1024] = {0}; snprintf(_bf, sizeof(_bf)-1, __VA_ARGS__); fprintf(stderr, "%s", OUTPUT_PLUGIN_PREFIX); fprintf(stderr, "%s", _bf); syslog(LOG_INFO, "%s", _bf); }

/* parameters for output plugin */
typedef struct _output_parameter output_parameter;
struct _output_parameter {
  int id;
  char *parameter_string;
  struct _globals *global;
};

/* commands which can be send to the input plugin */
typedef enum _out_cmd_type out_cmd_type;
enum _out_cmd_type {
  OUT_CMD_UNKNOWN = 0,
  OUT_CMD_HELLO
};

/* structure to store variables/functions for output plugin */
typedef struct _output output;
struct _output {
  char *plugin;
  void *handle;
  output_parameter param;

  int (*init)(output_parameter *);
  int (*stop)(int);
  int (*run)(int);
  int (*cmd)(int, out_cmd_type, int);
};

int output_init(output_parameter *);
int output_stop(int id);
int output_run(int id);
int output_cmd(int id, out_cmd_type cmd, int value);
