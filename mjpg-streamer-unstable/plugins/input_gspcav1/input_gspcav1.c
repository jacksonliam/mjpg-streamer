/*******************************************************************************
# GSPCAV1 streaming input-plugin for MJPG-streamer                             #
#                                                                              #
# This plugin is intended to work with gspcav1 compatible devices              #
# Intention is to use webcams that support JPG encoding by the webcam itself   #
#                                                                              #
# Copyright (C) 2007 Tom Stöveken                                              #
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <getopt.h>
#include <pthread.h>

#include "spcaframe.h"
#include "spcav4l.h"
#include "utils.h"

#include "../../utils.h"
#include "../../mjpg_streamer.h"

#define INPUT_PLUGIN_NAME "GSPCAV1 webcam grabber"

static const struct {
    const char *string;
    const int width, height;
} resolutions[] = {
    { "QSIF", 160,  120  },
    { "QCIF", 176,  144  },

    { "CGA",  320,  200  },
    { "CIF",  352,  288  },
    { "HVGA",  480,  320  },
    { "WVGA",  800,  480  },
    { "WVGA(16:9)",  854,  480  },
    { "PAL(16:9)",  854,  480  },
    { "WSVGA", 1024,  600  },

    { "HD 720", 1280,  720  },
    { "WXGA", 1280,  720  },
    { "WXGA+", 1680,  1050  },
    { "HD 1080", 1920,  1080  },
    { "2K", 2048,  1080  },
    { "WUXGA", 1920,  1200  },
    { "WQXGA", 2560,  1600  },

    { "QVGA", 320,  240  },
    { "SIF",  384,  288  },
    { "VGA",  640,  480  },
    { "PAL",  768,  576  },
    { "SVGA", 800,  600  },
    { "XGA",  1024, 768  },
    { "XGA+",  1152, 864  },
    { "SXGA", 1280, 1024 },
    { "SXGA+", 1400, 1050 },
    { "UXGA", 1600, 1200 },
    { "QXGA", 2048, 1536 },

    { "960x720", 960, 720 },
};

static const struct {
    const char *string;
    const int format;
} formats[] = {
    { "r16", VIDEO_PALETTE_RGB565  },
    { "r24", VIDEO_PALETTE_RGB24   },
    { "r32", VIDEO_PALETTE_RGB32   },
    { "yuv", VIDEO_PALETTE_YUV420P },
    { "jpg", VIDEO_PALETTE_JPEG    }
};


/* private functions and variables to this plugin */
pthread_t cam;
struct vdIn *videoIn;

static int plugin_number;
static globals *pglobal;

void *cam_thread(void *);
void cam_cleanup(void *);
void help(void);

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
int input_init(input_parameter *param, int plugin_no)
{
    plugin_number = plugin_no;
    char *dev = "/dev/video0", *s;
    int width = 640, height = 480, format = VIDEO_PALETTE_JPEG, i;

    param->argv[0] = INPUT_PLUGIN_NAME;

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
            {"d", required_argument, 0, 0},
            {"device", required_argument, 0, 0},
            {"r", required_argument, 0, 0},
            {"resolution", required_argument, 0, 0},
            {"f", required_argument, 0, 0},
            {"format", required_argument, 0, 0},
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

            /* d, device */
        case 2:
        case 3:
            DBG("case 2,3\n");
            dev = strdup(optarg);
            break;

            /* r, resolution */
        case 4:
        case 5:
            DBG("case 4,5\n");
            width = -1;
            height = -1;

            /* try to find the resolution in lookup table "resolutions" */
            for(i = 0; i < LENGTH_OF(resolutions); i++) {
                if(strcmp(resolutions[i].string, optarg) == 0) {
                    width  = resolutions[i].width;
                    height = resolutions[i].height;
                }
            }
            /* done if width and height were set */
            if(width != -1 && height != -1)
                break;
            /* parse value as decimal value */
            width  = strtol(optarg, &s, 10);
            height = strtol(s + 1, NULL, 10);
            break;

            /* f, format */
        case 6:
        case 7:
            DBG("case 6,7\n");

            /* try to find in lookup table */
            for(i = 0; i < LENGTH_OF(formats); i++) {
                if(strcmp(formats[i].string, optarg) == 0) {
                    format  = formats[i].format;
                }
            }
            break;

        default:
            DBG("default case\n");
            help();
            return 1;
        }
    }

    /* keep a pointer to the global variables */
    pglobal = param->global;

    /* allocate webcam datastructure */
    videoIn = malloc(sizeof(struct vdIn));
    if(videoIn == NULL) {
        IPRINT("not enough memory for videoIn\n");
        exit(EXIT_FAILURE);
    }
    memset(videoIn, 0, sizeof(struct vdIn));

    /* display the parsed values */
    IPRINT("Using V4L1 device.: %s\n", dev);
    IPRINT("Desired Resolution: %i x %i\n", width, height);

    /* open video device and prepare data structure */
    if(init_videoIn(videoIn, dev, width, height, format, 1) != 0) {
        IPRINT("init_VideoIn failed\n");
        closelog();
        exit(EXIT_FAILURE);
    }

    return 0;
}

/******************************************************************************
Description.: Stops the execution of worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int input_stop(int id)
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
int input_run(int id)
{
    pglobal->in[id].buf = malloc(videoIn->framesizeIn);
    if(pglobal->in[id].buf == NULL) {
        fprintf(stderr, "could not allocate memory\n");
        exit(EXIT_FAILURE);
    }

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
int input_cmd(int plugin, unsigned int control_id, unsigned int group, int value)
{
    DBG("Command interface is not implemented for the %s\n", INPUT_PLUGIN_NAME);
    return 0;
}

/*** private functions for this plugin below ***/
/******************************************************************************
Description.: print a help message to stderr
Input Value.: -
Return Value: -
******************************************************************************/
void help(void)
{
    int i;

    fprintf(stderr, " ---------------------------------------------------------------\n" \
    " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
    " ---------------------------------------------------------------\n" \
    " The following parameters can be passed to this plugin:\n\n" \
    " [-d | --device ].......: video device to open (your camera)\n" \
    " [-r | --resolution ]...: the resolution of the video device,\n" \
    "                          can be one of the following strings:\n" \
    "                          ");

    for(i = 0; i < LENGTH_OF(resolutions); i++) {
        fprintf(stderr, "%s ", resolutions[i].string);
        if((i + 1) % 6 == 0)
            fprintf(stderr, "\n                          ");
    }
    fprintf(stderr, "\n                          or a custom value like the following" \
    "\n                          example: 640x480\n" \
    " [ -f | --format ]......: grabbing format, should be set to 'jpg'\n" \
    "                          can be: ");
    for(i = 0; i < LENGTH_OF(formats); i++) {
        fprintf(stderr, "%s ", formats[i].string);
        if((i + 1) % 6 == 0)
            fprintf(stderr, "\n                          ");
    }
    fprintf(stderr, "\n");
    fprintf(stderr, " ---------------------------------------------------------------\n\n");
}

/******************************************************************************
Description.: this thread worker grabs a frame and copies it to the global buffer
Input Value.: unused
Return Value: unused, always NULL
******************************************************************************/
void *cam_thread(void *arg)
{
    int iframe = 0;
    unsigned char *pictureData = NULL;
    struct frame_t *headerframe;

    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(cam_cleanup, NULL);

    while(!pglobal->stop) {

        /* grab a frame */
        if(v4lGrab(videoIn) < 0) {
            IPRINT("Error grabbing frames\n");
            exit(EXIT_FAILURE);
        }

        iframe = (videoIn->frame_cour + (OUTFRMNUMB - 1)) % OUTFRMNUMB;
        videoIn->framelock[iframe]++;
        headerframe = (struct frame_t*)videoIn->ptframe[iframe];
        pictureData = videoIn->ptframe[iframe] + sizeof(struct frame_t);
        videoIn->framelock[iframe]--;

        /* copy JPG picture to global buffer */
        pthread_mutex_lock(&pglobal->in[plugin_number].db);

        pglobal->in[plugin_number].size = get_jpegsize(pictureData, headerframe->size);
        memcpy(pglobal->in[plugin_number].buf, pictureData, pglobal->in[plugin_number].size);

        /* signal fresh_frame */
        pthread_cond_broadcast(&pglobal->in[plugin_number].db_update);
        pthread_mutex_unlock(&pglobal->in[plugin_number].db);
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
    IPRINT("cleaning up ressources allocated by input thread\n");

    close_v4l(videoIn);
    //if (videoIn->tmpbuffer != NULL) free(videoIn->tmpbuffer);
    if(videoIn != NULL) free(videoIn);
    if(pglobal->in[plugin_number].buf != NULL) free(pglobal->in[plugin_number].buf);
}




