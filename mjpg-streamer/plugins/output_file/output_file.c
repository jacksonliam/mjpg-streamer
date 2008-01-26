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
#include <fcntl.h>
#include <time.h>
#include <syslog.h>

#include "../../utils.h"
#include "../../mjpg_streamer.h"

#define OUTPUT_PLUGIN_NAME "FILE output plugin"
#define MAX_ARGUMENTS 32

static pthread_t worker;
static globals *pglobal;
static int fd, delay, bytes;
static char *folder = "/tmp";
static unsigned char *frame=NULL;
static char *command = NULL;

/******************************************************************************
Description.: print a help message
Input Value.: -
Return Value: -
******************************************************************************/
void help(void) {
  fprintf(stderr, " ---------------------------------------------------------------\n" \
                  " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
                  " ---------------------------------------------------------------\n" \
                  " The following parameters can be passed to this plugin:\n\n" \
                  " [-f | --folder ]........: folder to save pictures\n" \
                  " [-d | --delay ].........: delay after saving pictures in ms\n" \
                  " [-b | --bytes ].........: save only on change in picture-size\n" \
                  " [-c | --command ].......: execute command after saveing picture\n\n" \
                  " ---------------------------------------------------------------\n");
}

/******************************************************************************
Description.: clean up allocated ressources
Input Value.: unused argument
Return Value: -
******************************************************************************/
void worker_cleanup(void *arg) {
  static unsigned char first_run=1;

  if ( !first_run ) {
    DBG("already cleaned up ressources\n");
    return;
  }

  first_run = 0;
  OPRINT("cleaning up ressources allocated by worker thread\n");

  free(frame);
  close(fd);
}

/******************************************************************************
Description.: this is the main worker thread
              it loops forever, grabs a fresh frame and stores it to file
Input Value.: 
Return Value: 
******************************************************************************/
void *worker_thread( void *arg ) {
  int ok = 1, frame_size=0, previous_frame_size=0;
  char buffer1[1024] = {0}, buffer2[1024] = {0};
  unsigned long long counter=0;
  time_t t;
  struct tm *tmp;

  if ( (frame = malloc(512*1024)) == NULL ) {
    OPRINT("not enough memory for worker thread\n");
    exit(EXIT_FAILURE);
  }

  /* set cleanup handler to cleanup allocated ressources */
  pthread_cleanup_push(worker_cleanup, NULL);

  while ( ok >= 0 && !pglobal->stop ) {
    DBG("waiting for fresh frame\n");
    pthread_cond_wait(&pglobal->db_update, &pglobal->db);

    /* read buffer */
    frame_size = pglobal->size;
    memcpy(frame, pglobal->buf, frame_size);

    pthread_mutex_unlock( &pglobal->db );

    /*
       if previous picture and current picture differ more than "--bytes" bytes
       it is __perhaps__ a motion that caused this difference

      This works well for image-sources with low noise only (webcam in bright areas for example)

      At daylight a value of 4000 works well, but the same image-source performed very bad at night (noise)
    */
    if ( bytes != 0 && ABS(previous_frame_size-frame_size) <= bytes ) {
      previous_frame_size = frame_size;
      continue;
    }
    DBG("size-diff: %d kB\n", (previous_frame_size-frame_size)/1024);
    previous_frame_size = frame_size;

    /* prepare filename */
    memset(buffer1, 0, sizeof(buffer1));
    memset(buffer2, 0, sizeof(buffer2));

    t = time(NULL);
    tmp = localtime(&t);
    if (tmp == NULL) {
      perror("localtime");
      return NULL;
    }

    if (strftime(buffer1, sizeof(buffer1), "%%s/%Y_%m_%d_%H_%M_%S_picture_%%09llu.jpg", tmp) == 0) {
      OPRINT("strftime returned 0\n");
      exit(EXIT_FAILURE);
    }

    snprintf(buffer2, sizeof(buffer2), buffer1, folder, counter++);
    DBG("writing file: %s\n", buffer2);

    /* open file for write */
    if( (fd = open(buffer2, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0 ) {
      OPRINT("could not open the file %s\n", buffer2);
      return NULL;
    }

    /* save picture to file */
    if( write(fd, frame, frame_size) < 0 ) {
      OPRINT("could not write to file %s\n", buffer2);
      perror("write()");
      close(fd);
      return NULL;
    }

    close(fd);

    /* if parameters are needed, use a BASH-script as argument */
    if ( command != NULL ) {
      system(command);
    }

    if (delay > 0) {
      usleep(1000*delay);
    }
  }

  pthread_cleanup_pop(1);

  return NULL;
}

/*** plugin interface functions ***/
/******************************************************************************
Description.: this function is called first, in order to initialise
              this plugin and pass a parameter string
Input Value.: parameters
Return Value: 0 if everything is ok, non-zero otherwise
******************************************************************************/
int output_init(output_parameter *param) {
  char *argv[MAX_ARGUMENTS]={NULL};
  int argc=1, i;

  delay = 0;
  bytes = 0;

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
      {"f", required_argument, 0, 0},
      {"folder", required_argument, 0, 0},
      {"d", required_argument, 0, 0},
      {"delay", required_argument, 0, 0},
      {"b", required_argument, 0, 0},
      {"bytes", required_argument, 0, 0},
      {"c", required_argument, 0, 0},
      {"command", required_argument, 0, 0},
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

      /* f, folder */
      case 2:
      case 3:
        DBG("case 2,3\n");
        folder = malloc(strlen(optarg)+1);
        strcpy(folder, optarg);
        if ( folder[strlen(folder)-1] == '/' )
          folder[strlen(folder)-1] = '\0';
        break;

      /* d, delay */
      case 4:
      case 5:
        DBG("case 4,5\n");
        delay = atoi(optarg);
        break;

      /* b, bytes */
      case 6:
      case 7:
        DBG("case 6,7\n");
        bytes = atoi(optarg);
        break;

      /* c, command */
      case 8:
      case 9:
        DBG("case 8,9\n");
        command = strdup(optarg);
        break;
    }
  }

  pglobal = param->global;

  OPRINT("output folder.....: %s\n", folder);
  OPRINT("delay after save..: %d\n", delay);
  OPRINT("picture diff-bytes: %d\n", bytes);
  OPRINT("command...........: %s\n", (command==NULL)?"disabled":command);
  return 0;
}

/******************************************************************************
Description.: calling this function stops the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_stop(int id) {
  DBG("will cancel worker thread\n");
  pthread_cancel(worker);
  return 0;
}

/******************************************************************************
Description.: calling this function creates and starts the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_run(int id) {
  DBG("launching worker thread\n");
  pthread_create(&worker, 0, worker_thread, NULL);
  pthread_detach(worker);
  return 0;
}
