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

#define INPUT_PLUGIN_NAME "FILE input plugin"
#define MAX_ARGUMENTS 32

/* private functions and variables to this plugin */
static pthread_t   worker;
static globals     *pglobal;

void *worker_thread( void *);
void worker_cleanup(void *);
void help(void);

static int delay = 1000;
static char *folder = NULL;

/*** plugin interface functions ***/
int input_init(input_parameter *param) {
  char *argv[MAX_ARGUMENTS]={NULL};
  int argc=1, i;

  /* convert the single parameter-string to an array of strings */
  argv[0] = INPUT_PLUGIN_NAME;
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
            IPRINT("ERROR: too many arguments to input plugin\n");
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
      {"d", required_argument, 0, 0},
      {"delay", required_argument, 0, 0},
      {"f", required_argument, 0, 0},
      {"folder", required_argument, 0, 0},
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

      /* d, delay */
      case 2:
      case 3:
        DBG("case 2,3\n");
        delay = atoi(optarg);
        break;

      /* f, folder */
      case 4:
      case 5:
        DBG("case 4,5\n");
        folder = malloc(strlen(optarg)+2);
        strcpy(folder, optarg);
        if ( optarg[strlen(optarg)-1] != '/' )
          strcat(folder, "/");
        break;

      default:
        DBG("default case\n");
        help();
        return 1;
    }
  }

  pglobal = param->global;

  IPRINT("JPG input folder..: %s\n", folder);
  IPRINT("delay.............: %i\n", delay);

  return 0;
}

int input_stop(void) {
  DBG("will cancel input thread\n");
  pthread_cancel(worker);

  return 0;
}

int input_run(void) {
  pglobal->buf = malloc(256*1024);
  if (pglobal->buf == NULL) {
    fprintf(stderr, "could not allocate memory\n");
    exit(1);
  }

  if( pthread_create(&worker, 0, worker_thread, NULL) != 0) {
    free(pglobal->buf);
    fprintf(stderr, "could not start worker thread\n");
    exit(EXIT_FAILURE);
  }
  pthread_detach(worker);

  return 0;
}

/*** private functions for this plugin below ***/
void help(void) {
    fprintf(stderr, " ---------------------------------------------------------------\n" \
                    " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
                    " ---------------------------------------------------------------\n" \
                    " The following parameters can be passed to this plugin:\n\n" \
                    " [-d | --delay ]........: delay to pause between frames\n" \
                    " [-f | --folder ].......: folder containing the JPG-files\n" \
                    " ---------------------------------------------------------------\n");
}

/* the single writer thread */
void *worker_thread( void *arg ) {
  /* set cleanup handler to cleanup allocated ressources */
  pthread_cleanup_push(worker_cleanup, NULL);

  while( !pglobal->stop ) {

    /* grab a frame */
    if( ... ) {
      fprintf(stderr, "Error grabbing frames\n");
      exit(EXIT_FAILURE);
    }

    /* copy JPG picture to global buffer */
    pthread_mutex_lock( &pglobal->db );
    pglobal->size = memcpy_picture(pglobal->buf, videoIn->tmpbuffer, videoIn->buf.bytesused);

    /* signal fresh_frame */
    pthread_cond_broadcast(&pglobal->db_update);
    pthread_mutex_unlock( &pglobal->db );

    usleep(1000*delay);
  }

  DBG("leaving input thread, calling cleanup function now\n");
  pthread_cleanup_pop(1);

  return NULL;
}

void worker_cleanup(void *arg) {
  static unsigned char first_run=1;

  if ( !first_run ) {
    DBG("already cleaned up ressources\n");
    return;
  }

  first_run = 0;
  DBG("cleaning up ressources allocated by input thread\n");

  if (pglobal->buf != NULL) free(pglobal->buf);
}




