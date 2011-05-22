/*******************************************************************************
# Linux-UVC streaming input-plugin for MJPG-streamer                           #
#                                                                              #
# This package work with the Logitech UVC based webcams with the mjpeg feature #
#                                                                              #
# Copyright (C) 2005 2006 Laurent Pinchart &&  Michel Xhaard                   #
#                    2007 Lucas van Staden                                     #
#                    2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; either version 2 of the License, or            #
# (at your option) any later version.                                          #
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
#include <fcntl.h>
#include <linux/videodev2.h>
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

#include "../../utils.h"
#include "../../mjpg_streamer.h"
#include "dynctrl.h"

#define INPUT_PLUGIN_NAME "UVC webcam control"

typedef struct input_uvc {
    int fd;
    char *videodevice;
    int min_res;
} input_uvc;

/* private functions and variables to this plugin */
pthread_t cam;
pthread_mutex_t controls_mutex;
input_uvc *this;
static globals *pglobal;

void *cam_thread(void *);
void cam_cleanup(void *);
void help(void);
int input_cmd(in_cmd_type, int);

/*** plugin interface functions ***/
/******************************************************************************
Description.: This function initializes the plugin. It parses the commandline-
              parameter and stores the default and parsed values in the
              appropriate variables.
Input Value.: param contains among others the command-line string
Return Value: 0 if everything is fine
              1 if "--help" was triggered, in this case the calling programm
              should stop running and leave.
******************************************************************************/
int input_init(input_parameter *param, int id)
{
    int i;
    char *dev = "/dev/video0";

    /* initialize the mutes variable */
    if(pthread_mutex_init(&controls_mutex, NULL) != 0) {
        IPRINT("could not initialize mutex variable\n");
        exit(EXIT_FAILURE);
    }

    /* keep a pointer to the global variables */
    pglobal = param->global;

    /* allocate webcam datastructure */
    this = malloc(sizeof(struct input_uvc));
    if(this == NULL) {
        IPRINT("not enough memory for videoIn\n");
        exit(EXIT_FAILURE);
    }
    memset(this, 0, sizeof(struct input_uvc));
    this->min_res = MIN_RES;

    /* display the parsed values */
    IPRINT("Using V4L2 device.: %s\n", dev);

    param->argv[0] = INPUT_PLUGIN_NAME;

    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    /* parse the parameters */
    reset_getopt();
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = \ {
            {"h", no_argument, 0, 0
            },
            {"help", no_argument, 0, 0},
            {"d", required_argument, 0, 0},
            {"device", required_argument, 0, 0},
            {"m", required_argument, 0, 0},
            {"min", required_argument, 0, 0},
            {0, 0, 0, 0}
        };

        /* parsing all parameters according to the list above is sufficent */
        c = getopt_long_only(param->argc, param->argv, "", long_options, &option_index);

        /* no more options to parse */
        if(c == -1) break;

        /* unrecognized option */
        if(c == '?') {
            help();
            return 1;
        }

        /* dispatch the given options */
        switch(option_index) {
            /* h, help */
        case 0:
        case 1:
            DBG("case 0,1\n");
            help();
            return 1;
            break;

            /* d, device */
        case 2:
        case 3:
            DBG("case 2,3\n");
            dev = strdup(optarg);
            break;

        case 4:
        case 5: {
            DBG("case 4,5\n");
            float min_res  = strtof(optarg, 0);
            if(min_res > 0.0) {
                min_res *= ONE_DEGREE;
                this->min_res = (int)min_res;
            }
        }
        break;

        default:
            DBG("default case\n");
            help();
            return 1;
        }
    }

    this->videodevice = (char *) calloc(1, 16 * sizeof(char));
    snprintf(this->videodevice, 12, "%s", dev);

    if((this->fd = open(this->videodevice, O_RDWR)) == -1) {
        perror("ERROR opening V4L interface");
        return -1;
    }

    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(struct v4l2_capability));
    int ret = ioctl(this->fd, VIDIOC_QUERYCAP, &cap);
    if(ret < 0) {
        fprintf(stderr, "Error opening device %s: unable to query device.\n", this->videodevice);
        return -1;
    }

    if(!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "%s does not support streaming, cap=%x\n", this->videodevice, cap.capabilities);
        return -1;
    }

    initDynCtrls(this->fd);

    return 0;
}

/******************************************************************************
Description.: Stops the execution of worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int input_stop(void)
{
    DBG("will cancel input thread\n");
    pthread_cancel(cam);

    return 0;
}

/******************************************************************************
Description.: spins of a worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int input_run(void)
{

    pthread_create(&cam, 0, cam_thread, NULL);
    pthread_detach(cam);

    return 0;
}

/******************************************************************************
Description.: process commands, allows to set certain runtime configurations
              and settings like pan/tilt, colors, saturation etc.
Input Value.: * cmd specifies the command, a complete list is maintained in
                the file "input.h"
              * value is used for commands that make use of a parameter.
Return Value: depends in the command, for most cases 0 means no errors and
              -1 signals an error. This is just rule of thumb, not more!
******************************************************************************/
int input_cmd(in_cmd_type cmd, int value)
{
    int res = 0;
    static int pan = 0, tilt = 0, pan_tilt_valid = -1;
    const int one_degree = ONE_DEGREE;

    /* certain commands do not need the mutex */
    if(cmd != IN_CMD_RESET_PAN_TILT_NO_MUTEX)
        pthread_mutex_lock(&controls_mutex);

    switch(cmd) {
    case IN_CMD_HELLO:
        fprintf(stderr, "Hello from input plugin\n");
        break;

    case IN_CMD_RESET_PAN_TILT:
    case IN_CMD_RESET_PAN_TILT_NO_MUTEX:
        DBG("about to set pan/tilt to default position\n");
        if(uvcPanTilt(this->fd, 0, 0, 3) != 0) {
            res = -1;
            break;
        }
        pan_tilt_valid = 1;
        pan = tilt = 0;
        sleep(4);
        break;

    case IN_CMD_PAN_SET:
        DBG("set pan to %d degrees\n", value);

        /* in order to calculate absolute positions we must check for initialized values */
        if(pan_tilt_valid != 1) {
            if(input_cmd(IN_CMD_RESET_PAN_TILT_NO_MUTEX, 0) == -1) {
                res = -1;
                break;
            }
        }

        /* limit pan-value to min and max, multiply it with constant "one_degree" */
        value = MIN(MAX(value * one_degree, MIN_PAN), MAX_PAN);

        /* calculate the relative degrees to move to the desired absolute pan-value */
        if((res = value - pan) == 0) {
            /* do not move if this would mean to move by 0 degrees */
            res = pan / one_degree;
            break;
        }

        /* move it */
        pan = value;
        uvcPanTilt(this->fd, res, 0, 0);
        res = pan / one_degree;

        DBG("pan: %d\n", pan);
        break;

    case IN_CMD_PAN_PLUS:
        DBG("pan +\n");

        if(pan_tilt_valid != 1) {
            if(input_cmd(IN_CMD_RESET_PAN_TILT_NO_MUTEX, 0) == -1) {
                res = -1;
                break;
            }
        }

        if((MAX_PAN) >= (pan + this->min_res)) {
            pan += this->min_res;
            uvcPanTilt(this->fd, this->min_res, 0, 0);
        }
        res = pan / one_degree;

        DBG("pan: %d\n", pan);
        break;

    case IN_CMD_PAN_MINUS:
        DBG("pan -\n");

        if(pan_tilt_valid != 1) {
            if(input_cmd(IN_CMD_RESET_PAN_TILT_NO_MUTEX, 0) == -1) {
                res = -1;
                break;
            }
        }

        if((MIN_PAN) <= (pan - this->min_res)) {
            pan -= this->min_res;
            uvcPanTilt(this->fd, -this->min_res, 0, 0);
        }
        res = pan / one_degree;

        DBG("pan: %d\n", pan);
        break;

    case IN_CMD_TILT_SET:
        DBG("set tilt to %d degrees\n", value);

        if(pan_tilt_valid != 1) {
            if(input_cmd(IN_CMD_RESET_PAN_TILT_NO_MUTEX, 0) == -1) {
                res = -1;
                break;
            }
        }

        /* limit pan-value to min and max, multiply it with constant "one_degree" */
        value = MIN(MAX(value * one_degree, MIN_TILT), MAX_TILT);

        /* calculate the relative degrees to move to the desired absolute pan-value */
        if((res = value - tilt) == 0) {
            /* do not move if this would mean to move by 0 degrees */
            res = tilt / one_degree;
            break;
        }

        /* move it */
        tilt = value;
        uvcPanTilt(this->fd, 0, res, 0);
        res = tilt / one_degree;

        DBG("tilt: %d\n", tilt);
        break;

    case IN_CMD_TILT_PLUS:
        DBG("tilt +\n");

        if(pan_tilt_valid != 1) {
            if(input_cmd(IN_CMD_RESET_PAN_TILT_NO_MUTEX, 0) == -1) {
                res = -1;
                break;
            }
        }

        if((MAX_TILT) >= (tilt + this->min_res)) {
            tilt += this->min_res;
            uvcPanTilt(this->fd, 0, this->min_res, 0);
        }
        res = tilt / one_degree;

        DBG("tilt: %d\n", tilt);
        break;

    case IN_CMD_TILT_MINUS:
        DBG("tilt -\n");

        if(pan_tilt_valid != 1) {
            if(input_cmd(IN_CMD_RESET_PAN_TILT_NO_MUTEX, 0) == -1) {
                res = -1;
                break;
            }
        }

        if((MIN_TILT) <= (tilt - this->min_res)) {
            tilt -= this->min_res;
            uvcPanTilt(this->fd, 0, -this->min_res, 0);
        }
        res = tilt / one_degree;

        DBG("tilt: %d\n", tilt);
        break;

    default:
        DBG("nothing matched\n");
        res = -1;
    }

    if(cmd != IN_CMD_RESET_PAN_TILT_NO_MUTEX)
        pthread_mutex_unlock(&controls_mutex);

    return res;
}

/*** private functions for this plugin below ***/
/******************************************************************************
Description.: print a help message to stderr
Input Value.: -
Return Value: -
******************************************************************************/
void help(void)
{
    fprintf(stderr, " ---------------------------------------------------------------\n" \
    " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
    " ---------------------------------------------------------------\n" \
    " The following parameters can be passed to this plugin:\n\n" \
    " [-d | --device ]...: video device to open (your camera)\n" \
    " [-m | --min ]......: set the minimum step size in degrees (defalt 5)\n");
}

/******************************************************************************
Description.: this thread worker grabs a frame and copies it to the global buffer
Input Value.: unused
Return Value: unused, always NULL
******************************************************************************/
void *cam_thread(void *arg)
{
    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(cam_cleanup, NULL);

    while(!pglobal->stop) {

        sleep(1);
        pthread_cond_broadcast(&pglobal->db_update);  //this keeps the output stream alive
        DBG("spin loop\n");
    }

    DBG("leaving input thread, calling cleanup function now\n");
    pthread_cleanup_pop(1);

    return NULL;
}

/******************************************************************************
Description.:
Input Value.:
Return Value:
******************************************************************************/
void cam_cleanup(void *arg)
{
    static unsigned char first_run = 1;

    if(!first_run) {
        DBG("already cleaned up ressources\n");
        return;
    }

    first_run = 0;
}




