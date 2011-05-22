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

/*
  This output plugin is based on code from output_file.c
  Writen by Dimitrios Zachariadis
  Version 0.1, May 2010

  It provides a mechanism to take snapshots with a trigger from a UDP packet.
  The UDP msg contains the path for the snapshot jpeg file
  It echoes the message received back to the sender, after taking the snapshot
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <syslog.h>

#include <dirent.h>

#include "../../utils.h"
#include "../../mjpg_streamer.h"

#define OUTPUT_PLUGIN_NAME "RTSP output plugin"

enum RTSP_State {
    RTSP_State_Setup,
    RTSP_State_Playing,
    RTSP_State_Paused,
    RTSP_State_Teardown,
};

static pthread_t worker;
static globals *pglobal;
static int fd, max_frame_size;
static unsigned char *frame = NULL;
static char *command = NULL;
static int input_number = 0;

// UDP port
static int port = 554;

/******************************************************************************
Description.: print a help message
Input Value.: -
Return Value: -
******************************************************************************/
void help(void)
{
    fprintf(stderr, " ---------------------------------------------------------------\n" \
            " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
            " ---------------------------------------------------------------\n" \
            " The following parameters can be passed to this plugin:\n\n" \
            " [-p | --port ]..........: UDP port to listen for picture requests. UDP message is the filename to save\n\n" \
            " [-i | --input ].......: read frames from the specified input plugin (first input plugin between the arguments is the 0th)\n\n" \
            " ---------------------------------------------------------------\n");
}

/******************************************************************************
Description.: clean up allocated ressources
Input Value.: unused argument
Return Value: -
******************************************************************************/
void worker_cleanup(void *arg)
{
    static unsigned char first_run = 1;

    if(!first_run) {
        DBG("already cleaned up ressources\n");
        return;
    }

    first_run = 0;
    OPRINT("cleaning up ressources allocated by worker thread\n");

    if(frame != NULL) {
        free(frame);
    }
    close(fd);
}

/******************************************************************************
Description.: this is the main worker thread
              it loops forever, grabs a fresh frame and stores it to file
Input Value.:
Return Value:
******************************************************************************/
void *worker_thread(void *arg)
{
    int ok = 1, frame_size = 0, rc = 0;
    char buffer1[1024] = {0};
    unsigned char *tmp_framebuffer = NULL;

    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(worker_cleanup, NULL);

    // set UDP server data structures ---------------------------
    if(port <= 0) {
        OPRINT("a valid UDP port must be provided\n");
        return NULL;
    }
    struct sockaddr_in addr;
    int sd;
    int bytes;
    unsigned int addr_len = sizeof(addr);
    char udpbuffer[1024] = {0};
    sd = socket(PF_INET, SOCK_DGRAM, 0);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if(bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
        perror("bind");
    // -----------------------------------------------------------

    while(ok >= 0 && !pglobal->stop) {
        DBG("waiting for a UDP message\n");

        // UDP receive ---------------------------------------------
        memset(udpbuffer, 0, sizeof(udpbuffer));
        bytes = recvfrom(sd, udpbuffer, sizeof(udpbuffer), 0, (struct sockaddr*)&addr, &addr_len);
        // ---------------------------------------------------------



        DBG("waiting for fresh frame\n");
        pthread_mutex_lock(&pglobal->in[input_number].db);
        pthread_cond_wait(&pglobal->in[input_number].db_update, &pglobal->in[input_number].db);

        /* read buffer */
        frame_size = pglobal->in[input_number].size;

        /* check if buffer for frame is large enough, increase it if necessary */
        if(frame_size > max_frame_size) {
            DBG("increasing buffer size to %d\n", frame_size);

            max_frame_size = frame_size + (1 << 16);
            if((tmp_framebuffer = realloc(frame, max_frame_size)) == NULL) {
                pthread_mutex_unlock(&pglobal->in[input_number].db);
                LOG("not enough memory\n");
                return NULL;
            }

            frame = tmp_framebuffer;
        }

        /* copy frame to our local buffer now */
        memcpy(frame, pglobal->in[input_number].buf, frame_size);

        /* allow others to access the global buffer again */
        pthread_mutex_unlock(&pglobal->in[input_number].db);

        /* only save a file if a name came in with the UDP message */
        if(strlen(udpbuffer) > 0) {
            DBG("writing file: %s\n", udpbuffer);

            /* open file for write. Path must pre-exist */
            if((fd = open(udpbuffer, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
                OPRINT("could not open the file %s\n", udpbuffer);
                return NULL;
            }

            /* save picture to file */
            if(write(fd, frame, frame_size) < 0) {
                OPRINT("could not write to file %s\n", udpbuffer);
                perror("write()");
                close(fd);
                return NULL;
            }

            close(fd);
        }

        // send back client's message that came in udpbuffer
        sendto(sd, udpbuffer, bytes, 0, (struct sockaddr*)&addr, sizeof(addr));

        /* call the command if user specified one, pass current filename as argument */
        if(command != NULL) {
            memset(buffer1, 0, sizeof(buffer1));

            /* udpbuffer still contains the filename, pass it to the command as parameter */
            snprintf(buffer1, sizeof(buffer1), "%s \"%s\"", command, udpbuffer);
            DBG("calling command %s", buffer1);

            /* in addition provide the filename as environment variable */
            if((rc = setenv("MJPG_FILE", udpbuffer, 1)) != 0) {
                LOG("setenv failed (return value %d)\n", rc);
            }

            /* execute the command now */
            if((rc = system(buffer1)) != 0) {
                LOG("command failed (return value %d)\n", rc);
            }
        }
    }

    // close UDP port
    if(port > 0)
        close(sd);

    /* cleanup now */
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
int output_init(output_parameter *param)
{
    int i;

    param->argv[0] = OUTPUT_PLUGIN_NAME;

    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    reset_getopt();
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"h", no_argument, 0, 0
            },
            {"help", no_argument, 0, 0},
            {"p", required_argument, 0, 0},
            {"port", required_argument, 0, 0},
            {"i", required_argument, 0, 0},
            {"input", required_argument, 0, 0},
            {0, 0, 0, 0}
        };

        c = getopt_long_only(param->argc, param->argv, "", long_options, &option_index);

        /* no more options to parse */
        if(c == -1) break;

        /* unrecognized option */
        if(c == '?') {
            help();
            return 1;
        }

        switch(option_index) {
            /* h, help */
        case 0:
        case 1:
            DBG("case 0,1\n");
            help();
            return 1;
            break;
        case 2:
        case 3:
            DBG("case 2,3\n");
            port = atoi(optarg);
            break;
            /* i, input */
        case 4:
        case 5:
            DBG("case 4,5\n");
            input_number = atoi(optarg);
            break;
        }
    }

    pglobal = param->global;
    if(!(input_number < pglobal->incnt)) {
        OPRINT("ERROR: the %d input_plugin number is too much only %d plugins loaded\n", input_number, pglobal->incnt);
        return 1;
    }

    OPRINT("input plugin.....: %d: %s\n", input_number, pglobal->in[input_number].plugin);
    OPRINT("UDP port..........: %s\n", "disabled");
    return 0;
}

/******************************************************************************
Description.: calling this function stops the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_stop(int id)
{
    DBG("will cancel worker thread\n");
    pthread_cancel(worker);
    return 0;
}

/******************************************************************************
Description.: calling this function creates and starts the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_run(int id)
{
    DBG("launching worker thread\n");
    pthread_create(&worker, 0, worker_thread, NULL);
    pthread_detach(worker);
    return 0;
}


