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
#ifndef OUTPUT_H
#define OUTPUT_H

#define OUTPUT_PLUGIN_PREFIX " o: "
#define OPRINT(...) fprintf(stderr, "%s", OUTPUT_PLUGIN_PREFIX); fprintf(stderr, __VA_ARGS__)

/* parameters for output plugin */
typedef struct {
  char *parameter_string;
  void *global;
} output_parameter;

/* commands which can be send to the input plugin */
typedef enum {
  OUT_CMD_UNKNOWN = 0,
  OUT_CMD_HELLO
} out_cmd_type;

/* structure to store variables/functions for output plugin */
typedef struct {
  char *plugin;
  void *handle;
  output_parameter param;

  int (*init)(output_parameter *);
  int (*stop)(void);
  int (*run)(void);
  int (*cmd)(out_cmd_type cmd);
} output;

int output_init(output_parameter *);
int output_stop(void);
int output_run(void);
int output_cmd(out_cmd_type cmd);

#endif
