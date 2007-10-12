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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>

#include "../../mjpg_streamer.h"
#include "../../utils.h"
#include "../output.h"
#include "httpd.h"

#define OUTPUT_PLUGIN_NAME "HTTP output plugin"
#define MAX_ARGUMENTS 32

static pthread_t   server;
static globals     *pglobal;

extern int  port;
extern char *credentials, *www_folder;

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
void help(void) {
  fprintf(stderr, " ---------------------------------------------------------------\n" \
                  " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
                  " ---------------------------------------------------------------\n" \
                  " The following parameters can be passed to this plugin:\n\n" \
                  " [-w | --www ]...........: folder that contains webpages in \n" \
                  "                           flat hierarchy (no subfolders)\n" \
                  " [-p | --port ]..........: TCP port for this HTTP server\n" \
                  " [-c | --credentials ]...: check for \"username:password\"\n" \
                  " ---------------------------------------------------------------\n");
}

/*** plugin interface functions ***/
/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
int output_init(output_parameter *param) {
  char *argv[MAX_ARGUMENTS]={NULL};
  int argc=1, i;

  port = htons(8080);
  credentials = NULL;
  www_folder = NULL;

  /* convert the single parameter-string to an array of strings */
  argv[0] = OUTPUT_PLUGIN_NAME;
  if ( param->parameter_string != NULL && strlen(param->parameter_string) != 0 ) {
    char *arg, *saveptr, *token;

    arg=(char *)strdup(param->parameter_string);

    if ( strchr(arg, ' ') != NULL ) {
      token=strtok_r(arg, " ", &saveptr);
      if ( token != NULL ) {
        argv[argc] = strdup(token);
        argc++;
        while ( (token=strtok_r(NULL, " ", &saveptr)) != NULL ) {
          argv[argc] = strdup(token);
          argc++;
          if (argc >= MAX_ARGUMENTS) {
            OPRINT("ERROR: too many arguments to output plugin\n");
            return 1;
          }
        }
      }
    }
  }

  /* show all parameters for DBG purposes */
  for (i=0; i<argc; i++) {
    DBG("argv[%d]=%s\n", i, argv[i]);
  }

  reset_getopt();
  while(1) {
    int option_index = 0, c=0;
    static struct option long_options[] = \
    {
      {"h", no_argument, 0, 0},
      {"help", no_argument, 0, 0},
      {"p", required_argument, 0, 0},
      {"port", required_argument, 0, 0},
      {"c", required_argument, 0, 0},
      {"credentials", required_argument, 0, 0},
      {"w", required_argument, 0, 0},
      {"www", required_argument, 0, 0},
      {0, 0, 0, 0}
    };

    c = getopt_long_only(argc, argv, "", long_options, &option_index);

    /* no more options to parse */
    if (c == -1) break;

    /* unrecognized option */
    if (c == '?'){
      help();
      return 1;
    }

    switch (option_index) {
      /* h, help */
      case 0:
      case 1:
        DBG("case 0,1\n");
        help();
        return 1;
        break;

      /* p, port */
      case 2:
      case 3:
        DBG("case 2,3\n");
        port = htons(atoi(optarg));
        break;

      /* c, credentials */
      case 4:
      case 5:
        DBG("case 4,5\n");
        credentials = strdup(optarg);
        break;

      /* w, www */
      case 6:
      case 7:
        DBG("case 6,7\n");
        www_folder = malloc(strlen(optarg)+2);
        strcpy(www_folder, optarg);
        if ( optarg[strlen(optarg)-1] != '/' )
          strcat(www_folder, "/");
        break;
    }
  }

  pglobal = param->global;

  OPRINT("www-folder-path...: %s\n", (www_folder==NULL)?"disabled":www_folder);
  OPRINT("HTTP TCP port.....: %d\n", ntohs(port));
  OPRINT("username:password.: %s\n", (credentials==NULL)?"disabled":credentials);

  return 0;
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
int output_stop(void) {
  DBG("will cancel server thread\n");
  pthread_cancel(server);
  return 0;
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
int output_run(void) {
  DBG("launching server thread\n");
  pthread_create(&server, 0, server_thread, pglobal);
  pthread_detach(server);
  return 0;
}

int output_cmd(out_cmd_type cmd) {
  fprintf(stderr, "Hello command triggered\n");
  return 0;
}
