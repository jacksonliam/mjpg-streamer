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

/* this output plugin can send commands to the input plugin, so include it */
#include "../input.h"
#include "../output.h"

#define IO_BUFFER 256
#define BUFFER_SIZE 512

/* the boundary is used for the M-JPEG stream, it separates the pictures */
#define BOUNDARY "boundarydonotcross"

/* this defines the buffer size for a JPG-frame */
#define MAX_FRAME_SIZE (256*1024)

/* standard header to be send along with other header information like mimetype */
#define STD_HEADER "Connection: close\r\n" \
                   "Server: MJPG-Streamer/0.1\r\n" \
                   "Cache-Control: no-store, no-cache, must-revalidate\r\n" \
                   "Pragma: no-cache\r\n" \
                   "Expires: Mon, 3 Jan 2000 12:34:56 GMT\r\n"

/* only the following filestypes are supported */
static const struct {
  const char *dot_extension;
  const char *mimetype;
} mimetypes[] = {
  { ".html", "text/html" },
  { ".htm",  "text/html" },
  { ".css",  "text/css" },
  { ".js",   "text/javascript" },
  { ".txt",  "text/plain" },
  { ".jpg",  "image/jpeg" },
  { ".jpeg", "image/jpeg" },
  { ".png",  "image/png"},
  { ".gif",  "image/gif" },
  { ".ico",  "image/x-icon" },
  { ".swf",  "application/x-shockwave-flash" },
  { ".cab",  "application/x-shockwave-flash" },
  { ".jar",  "application/java-archive" }
};

/*
 * mapping between command string and command type
 * it is used to find the command for a certain string
 */
static const struct {
  const char *string;
  const in_cmd_type cmd;
} in_cmd_mapping[] = {
  { "reset", IN_CMD_RESET },
  { "reset_pan_tilt", IN_CMD_RESET_PAN_TILT },
  { "pan_plus", IN_CMD_PAN_PLUS },
  { "pan_minus", IN_CMD_PAN_MINUS },
  { "tilt_plus", IN_CMD_TILT_PLUS },
  { "tilt_minus", IN_CMD_TILT_MINUS },
  { "saturation_plus", IN_CMD_SATURATION_PLUS },
  { "saturation_minus", IN_CMD_SATURATION_MINUS },
  { "contrast_plus", IN_CMD_CONTRAST_PLUS },
  { "contrast_minus", IN_CMD_CONTRAST_MINUS },
  { "brightness_plus", IN_CMD_BRIGHTNESS_PLUS },
  { "brightness_minus", IN_CMD_BRIGHTNESS_MINUS },
  { "gain_plus", IN_CMD_GAIN_PLUS },
  { "gain_minus", IN_CMD_GAIN_MINUS }
};

/* mapping between command string and command type */
static const struct {
  const char *string;
  const out_cmd_type cmd;
} out_cmd_mapping[] = {
  { "hello_output", OUT_CMD_HELLO }
};

/* the webserver determines between these values for an answer */
typedef enum { A_UNKNOWN, A_SNAPSHOT, A_STREAM, A_COMMAND, A_FILE } answer_t;

/*
 * the client sends gives information in his request
 * this is used to store it in order to give an answer
 */
typedef struct {
  answer_t type;
  char *parameter;
  char *client;
  char *credentials;
} request;

/* the iobuffer structure is used to read from the HTTP-client */
typedef struct {
  int level;              /* how full is the buffer */
  char buffer[IO_BUFFER]; /* the data */
} iobuffer;

/* prototypes */
void *server_thread(void *arg);









