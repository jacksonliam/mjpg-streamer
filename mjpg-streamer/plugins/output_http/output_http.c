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
#include <syslog.h>

#include "../../mjpg_streamer.h"
#include "../../utils.h"
#include "httpd.h"

#define OUTPUT_PLUGIN_NAME "HTTP output plugin"
#define MAX_ARGUMENTS 32

/*
 * keep context for each server
 */
context servers[MAX_OUTPUT_PLUGINS];

/******************************************************************************
Description.: print help for this plugin to stdout
Input Value.: -
Return Value: -
******************************************************************************/
void help(void) {
  fprintf(stderr, " ---------------------------------------------------------------\n" \
                  " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
                  " ---------------------------------------------------------------\n" \
                  " The following parameters can be passed to this plugin:\n\n" \
                  " [-w | --www ]...........: folder that contains webpages in \n" \
                  "                           flat hierarchy (no subfolders)\n" \
                  " [-p | --port ]..........: TCP port for this HTTP server\n" \
                  " [-c | --credentials ]...: ask for \"username:password\" on connect\n" \
                  " [-n | --nocommands ]....: disable execution of commands\n"
                  " ---------------------------------------------------------------\n");
}

/*** plugin interface functions ***/
/******************************************************************************
Description.: Initialize this plugin.
              parse configuration parameters,
              store the parsed values in global variables
Input Value.: All parameters to work with.
              Among many other variables the "param->id" is quite important -
              it is used to distinguish between several server instances
Return Value: 0 if everything is OK, other values signal an error
******************************************************************************/
int output_init(output_parameter *param) {
  char *argv[MAX_ARGUMENTS]={NULL};
  int  argc=1, i;
  int  port;
  char *credentials, *www_folder;
  char nocommands;

  DBG("output #%02d\n", param->id);

  port = htons(8080);
  credentials = NULL;
  www_folder = NULL;
  nocommands = 0;

  /* convert the single parameter-string to an array of strings */
  argv[0] = OUTPUT_PLUGIN_NAME;
  if ( param->parameter_string != NULL && strlen(param->parameter_string) != 0 ) {
    char *arg=NULL, *saveptr=NULL, *token=NULL;

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
      {"n", no_argument, 0, 0},
      {"nocommands", no_argument, 0, 0},
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

      /* n, nocommands */
      case 8:
      case 9:
        DBG("case 8,9\n");
        nocommands = 1;
        break;
    }
  }

  servers[param->id].id = param->id;
  servers[param->id].pglobal = param->global;
  servers[param->id].conf.port = port;
  servers[param->id].conf.credentials = credentials;
  servers[param->id].conf.www_folder = www_folder;
  servers[param->id].conf.nocommands = nocommands;

  OPRINT("www-folder-path...: %s\n", (www_folder==NULL)?"disabled":www_folder);
  OPRINT("HTTP TCP port.....: %d\n", ntohs(port));
  OPRINT("username:password.: %s\n", (credentials==NULL)?"disabled":credentials);
  OPRINT("commands..........: %s\n", (nocommands)?"disabled":"enabled");

  return 0;
}

/******************************************************************************
Description.: this will stop the server thread, client threads
              will not get cleaned properly, because they run detached and
              no pointer is kept. This is not a huge issue, because this
              funtion is intended to clean up the biggest mess on shutdown.
Input Value.: id determines which server instance to send commands to
Return Value: always 0
******************************************************************************/
int output_stop(int id) {

  DBG("will cancel server thread #%02d\n", id);
  pthread_cancel(servers[id].threadID);

  return 0;
}

/******************************************************************************
Description.: This creates and starts the server thread
Input Value.: id determines which server instance to send commands to
Return Value: always 0
******************************************************************************/
int output_run(int id) {
  DBG("launching server thread #%02d\n", id);

  /* create thread and pass context to thread function */
  pthread_create(&(servers[id].threadID), NULL, server_thread, &(servers[id]));
  pthread_detach(servers[id].threadID);

  return 0;
}

/******************************************************************************
Description.: This is just an example function, to show how the output
              plugin could implement some special command.
              If you want to control some GPIO Pin this is a good place to
              implement it. Dont forget to add command types and a mapping.
Input Value.: cmd is the command type
              id determines which server instance to send commands to
Return Value: 0 indicates success, other values indicate an error
******************************************************************************/
int output_cmd(int id, out_cmd_type cmd, int value) {
  fprintf(stderr, "command (%d, value: %d) triggered for plugin instance #%02d\n", cmd, value, id);
  return 0;
}
