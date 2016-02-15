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

#include <linux/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>

#include "../../utils.h"
#include "v4l2uvc.h" // this header will includes the ../../mjpg_streamer.h

#ifndef NO_LIBJPEG
    #include "jpeg_utils.h"
    #include "huffman.h"
#endif

#include "dynctrl.h"

//#include "uvcvideo.h"

#define INPUT_PLUGIN_NAME "UVC webcam grabber"

/*
 * UVC resolutions mentioned at: (at least for some webcams)
 * http://www.quickcamteam.net/hcl/frame-format-matrix/
 */
static const struct {
    const char *string;
    const int width, height;
} resolutions[] = {
    { "QSIF", 160,  120  },
    { "QCIF", 176,  144  },
    { "CGA",  320,  200  },
    { "QVGA", 320,  240  },
    { "CIF",  352,  288  },
    { "VGA",  640,  480  },
    { "SVGA", 800,  600  },
    { "XGA",  1024, 768  },
    { "SXGA", 1280, 1024 }
};

static const struct {
    const char *string;
    const v4l2_std_id vstd;
} norms[] = {
    { "UNKNOWN", V4L2_STD_UNKNOWN },
    { "PAL", V4L2_STD_PAL },
    { "NTSC", V4L2_STD_NTSC },
    { "SECAM", V4L2_STD_SECAM }
};

/* private functions and variables to this plugin */
static globals *pglobal;
static int gquality = 80;
static unsigned int minimum_size = 0;
static int dynctrls = 1;
static unsigned int every = 1;

/* optional settings */
int sh_set = 0, sh = 0,
    co_set = 0, co = 0,
    br_set = 0, br_auto = 0, br = 0,
    sa_set = 0, sa = 0,
    wb_set = 0, wb_auto = 0, wb = 0,
    ex_set = 0, ex_auto = 0, ex = 0,
    bk_set = 0, bk = 0,
    rot_set = 0, rot = 0,
    hf_set = 0, hf = 0,
    vf_set = 0, vf = 0,
    pl_set = 0, pl = -1,
    gain_set = 0, gain_auto = 0, gain = 0,
    cagc_set = 0, cagc_auto = 0, cagc = 0,
    cb_set = 0, cb_auto = 0, cb = 0;

static const struct {
  const char * k;
  const int v;
} exposures[] = {
  { "auto", V4L2_EXPOSURE_AUTO },
  { "shutter-priority", V4L2_EXPOSURE_SHUTTER_PRIORITY },
  { "aperature-priority", V4L2_EXPOSURE_APERTURE_PRIORITY }
};

static const struct {
  const char * k;
  const int v;
} power_line[] = {
  { "disabled", V4L2_CID_POWER_LINE_FREQUENCY_DISABLED },
  { "50hz", V4L2_CID_POWER_LINE_FREQUENCY_50HZ },
  { "60hz", V4L2_CID_POWER_LINE_FREQUENCY_60HZ },
  { "auto", V4L2_CID_POWER_LINE_FREQUENCY_AUTO }
};

void *cam_thread(void *);
void cam_cleanup(void *);
void help(void);
int input_cmd(int plugin, unsigned int control, unsigned int group, int value, char *value_string);

const char *get_name_by_tvnorm(v4l2_std_id vstd) {
	int i;
	for (i=0;i<sizeof(norms);i++) {
		if (vstd == norms[i].vstd) {
			return norms[i].string;
		}
	}
	return norms[0].string;
}

#define OPTION_INT(idx, v) \
  case idx: \
    DBG("case " #idx); \
    if (sscanf(optarg, "%d", &v) != 1) { \
        fprintf(stderr, "Invalid value for -" #v " (integer required)\n"); \
        exit(EXIT_FAILURE); \
    } \
    v##_set = 1; \
    break;
    
#define OPTION_INT_AUTO(idx, v) \
  case idx: \
    DBG("case " #idx); \
    if (strcasecmp("auto", optarg) == 0) { \
        v##_auto = 1; \
    } else if (sscanf(optarg, "%d", &v) != 1) { \
        fprintf(stderr, "Invalid value for -" #v " (auto or integer required)\n"); \
        exit(EXIT_FAILURE); \
    } \
    v##_set = 1; \
    break;
    
#define OPTION_BOOL(idx, v) \
  case idx: \
    DBG("case " #idx); \
    if (strcasecmp("true", optarg) == 0) { \
        v = 1; \
    } else if (strcasecmp("false", optarg) == 0) { \
        v = 0; \
        fprintf(stderr, "Invalid value for -" #v " (true/false accepted)\n"); \
        exit(EXIT_FAILURE); \
    } \
    v##_set = 1; \
    break;
    
#define OPTION_MULTI(idx, var, table) \
  case idx: \
    DBG("case " #idx); \
    for(i = 0; i < LENGTH_OF(table); i++) { \
        if(strcasecmp(table[i].k, optarg) == 0) { \
            var = table[i].v; \
            break; \
        } \
    } \
    if (var == -1) { \
        fprintf(stderr, "Invalid value for -" #var "\n"); \
        exit(EXIT_FAILURE); \
    } \
    var##_set = 1; \
    break;
    
#define OPTION_MULTI_OR_INT(idx, var1, var1_default, var2, table) \
  case idx: \
    DBG("case " #idx); \
    var1 = var1_default; \
    for(i = 0; i < LENGTH_OF(table); i++) { \
        if(strcasecmp(table[i].k, optarg) == 0) { \
              printf("Hm, %d\n", table[i].v); \
            var1 = table[i].v; \
            break; \
        } \
    } \
    if (var1 == var1_default) { \
        if (sscanf(optarg, "%d", &var2) != 1) { \
            fprintf(stderr, "Invalid value for -" #var2 "\n"); \
            exit(EXIT_FAILURE); \
        } \
    } \
    var2##_set = 1; \
    break;

/*** plugin interface functions ***/
/******************************************************************************
Description.: This function ializes the plugin. It parses the commandline-
              parameter and stores the default and parsed values in the
              appropriate variables.
Input Value.: param contains among others the command-line string
Return Value: 0 if everything is fine
              1 if "--help" was triggered, in this case the calling programm
              should stop running and leave.
******************************************************************************/
int input_init(input_parameter *param, int id)
{
    char *dev = "/dev/video0", *s;
    int width = 640, height = 480, fps = -1, format = V4L2_PIX_FMT_MJPEG, i;
    v4l2_std_id tvnorm = V4L2_STD_UNKNOWN;

    /* initialize the mutes variable */
    if(pthread_mutex_init(&cams[id].controls_mutex, NULL) != 0) {
        IPRINT("could not initialize mutex variable\n");
        exit(EXIT_FAILURE);
    }

    param->argv[0] = INPUT_PLUGIN_NAME;

    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    /* parse the parameters */
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
            {"fps", required_argument, 0, 0},
            {"y", no_argument, 0, 0},
            {"yuv", no_argument, 0, 0},
            {"q", required_argument, 0, 0},
            {"quality", required_argument, 0, 0},
            {"m", required_argument, 0, 0},
            {"minimum_size", required_argument, 0, 0},
            {"n", no_argument, 0, 0},
            {"no_dynctrl", no_argument, 0, 0},
            {"l", required_argument, 0, 0},
            {"led", required_argument, 0, 0},
            {"fourcc", required_argument, 0, 0},
            {"t", required_argument, 0, 0 },
	        {"tvnorm", required_argument, 0, 0 },
            {"e", required_argument, 0, 0},
            {"every_frame", required_argument, 0, 0},
            {"sh", required_argument, 0, 0},
            {"co", required_argument, 0, 0},
            {"br", required_argument, 0, 0},
            {"sa", required_argument, 0, 0},
            {"wb", required_argument, 0, 0},
            {"ex", required_argument, 0, 0},
            {"bk", required_argument, 0, 0},
            {"rot", required_argument, 0, 0},
            {"hf", required_argument, 0, 0},
            {"vf", required_argument, 0, 0},
            {"pl", required_argument, 0, 0},
            {"gain", required_argument, 0, 0},
            {"cagc", required_argument, 0, 0},
            {"cb", required_argument, 0, 0},
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

        /* f, fps */
        case 6:
        case 7:
            DBG("case 6,7\n");
            fps = atoi(optarg);
            break;

        /* y, yuv */
        #ifndef NO_LIBJPEG
        case 8:
        case 9:
            DBG("case 8,9\n");
            format = V4L2_PIX_FMT_YUYV;
            break;
        #endif
        /* q, quality */
        #ifndef NO_LIBJPEG
        case 10:
        case 11:
            DBG("case 10,11\n");
            gquality = MIN(MAX(atoi(optarg), 0), 100);
            break;
        #endif
        /* m, minimum_size */
        case 12:
        case 13:
            DBG("case 12,13\n");
            minimum_size = MAX(atoi(optarg), 0);
            break;

        /* n, no_dynctrl */
        case 14:
        case 15:
            DBG("case 14,15\n");
            dynctrls = 0;
            break;

            /* l, led */
        case 16:
        case 17:/*
        DBG("case 16,17\n");
        if ( strcmp("on", optarg) == 0 ) {
          led = IN_CMD_LED_ON;
        } else if ( strcmp("off", optarg) == 0 ) {
          led = IN_CMD_LED_OFF;
        } else if ( strcmp("auto", optarg) == 0 ) {
          led = IN_CMD_LED_AUTO;
        } else if ( strcmp("blink", optarg) == 0 ) {
          led = IN_CMD_LED_BLINK;
        }*/
            break;
        /* fourcc */
        #ifndef NO_LIBJPEG
        case 18:
            DBG("case 18,19\n");
            if (strcmp(optarg, "RGBP") == 0) {
                format = V4L2_PIX_FMT_RGB565;
            } else {
                DBG("FOURCC %s not supported\n", optarg);
            }
            break;
        #endif
        /* t, tvnorm */
        case 19:
        case 20:
            DBG("case 19,20\n");
            if (strcasecmp("pal",optarg) == 0 ) {
	             tvnorm = V4L2_STD_PAL;
            } else if ( strcasecmp("ntsc",optarg) == 0 ) {
	             tvnorm = V4L2_STD_NTSC;
            } else if ( strcasecmp("secam",optarg) == 0 ) {
	             tvnorm = V4L2_STD_SECAM;
            }
            break;
        case 21:
        /* e, every */
        case 22:
            DBG("case 21,22\n");
            every = MAX(atoi(optarg), 1);
            break;

        /* options */
        OPTION_INT(23, sh)
        OPTION_INT(24, co)
        OPTION_INT_AUTO(25, br)
        OPTION_INT(26, sa)
        OPTION_INT_AUTO(27, wb)
        OPTION_MULTI_OR_INT(28, ex_auto, V4L2_EXPOSURE_MANUAL, ex, exposures)
        OPTION_INT(29, bk)
        OPTION_INT(30, rot)
        OPTION_BOOL(31, hf)
        OPTION_BOOL(32, vf)
        OPTION_MULTI(33, pl, power_line)
        OPTION_INT_AUTO(34, gain)
        OPTION_INT_AUTO(35, cagc)
        OPTION_INT_AUTO(36, cb)
    
        default:
            DBG("default case\n");
            help();
            return 1;
        }
    }
    DBG("input id: %d\n", id);
    cams[id].id = id;
    cams[id].pglobal = param->global;

    /* allocate webcam datastructure */
    cams[id].videoIn = malloc(sizeof(struct vdIn));
    if(cams[id].videoIn == NULL) {
        IPRINT("not enough memory for videoIn\n");
        exit(EXIT_FAILURE);
    }
    memset(cams[id].videoIn, 0, sizeof(struct vdIn));

    /* display the parsed values */
    IPRINT("Using V4L2 device.: %s\n", dev);
    IPRINT("Desired Resolution: %i x %i\n", width, height);
    IPRINT("Frames Per Second.: %i\n", fps);
    char *fmtString = NULL;
    switch (format) {
        case V4L2_PIX_FMT_MJPEG:
            fmtString = "JPEG";
            break;
        #ifndef NI_LIBJPG
            case V4L2_PIX_FMT_YUYV:
                fmtString = "YUYV";
                break;
            case V4L2_PIX_FMT_RGB565:
                fmtString = "RGB565";
                break;
        #endif
        default:
            fmtString = "Unknown format";
    }

    IPRINT("Format............: %s\n", fmtString);
    #ifndef NO_LIBJPEG
        if(format != V4L2_PIX_FMT_MJPEG)
            IPRINT("JPEG Quality......: %d\n", gquality);
    #endif

    if (tvnorm != V4L2_STD_UNKNOWN) {
        IPRINT("TV-Norm...........: %s\n", get_name_by_tvnorm(tvnorm));
    } else {
        IPRINT("TV-Norm...........: DEFAULT\n");
    }

    DBG("vdIn pn: %d\n", id);
    /* open video device and prepare data structure */
    if(init_videoIn(cams[id].videoIn, dev, width, height, fps, format, 1, cams[id].pglobal, id, tvnorm) < 0) {
        IPRINT("init_VideoIn failed\n");
        closelog();
        exit(EXIT_FAILURE);
    }
    /*
     * recent linux-uvc driver (revision > ~#125) requires to use dynctrls
     * for pan/tilt/focus/...
     * dynctrls must get initialized
     */
    if(dynctrls)
        initDynCtrls(cams[id].videoIn->fd);
    
    enumerateControls(cams[id].videoIn, cams[id].pglobal, id); // enumerate V4L2 controls after UVC extended mapping
    
    return 0;
}

/******************************************************************************
Description.: Stops the execution of worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int input_stop(int id)
{
    DBG("will cancel camera thread #%02d\n", id);
    pthread_cancel(cams[id].threadID);
    return 0;
}

/******************************************************************************
Description.: spins of a worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int input_run(int id)
{
    cams[id].pglobal->in[id].buf = malloc(cams[id].videoIn->framesizeIn);
    if(cams[id].pglobal->in[id].buf == NULL) {
        fprintf(stderr, "could not allocate memory\n");
        exit(EXIT_FAILURE);
    }

    DBG("launching camera thread #%02d\n", id);
    /* create thread and pass context to thread function */
    pthread_create(&(cams[id].threadID), NULL, cam_thread, &(cams[id]));
    pthread_detach(cams[id].threadID);
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
    "\n                          example: 640x480\n");

#ifndef NO_LIBJPEG
    fprintf(stderr, " [-f | --fps ]..........: frames per second\n" \
    "                          (activates YUYV format, disables MJPEG)\n" \
    " [-m | --minimum_size ].: drop frames smaller then this limit, useful\n" \
    "                          if the webcam produces small-sized garbage frames\n" \
    "                          may happen under low light conditions\n" \
    " [-e | --every_frame ]..: drop all frames except numbered\n" \
    " [-n | --no_dynctrl ]...: do not initalize dynctrls of Linux-UVC driver\n" \
    " [-l | --led ]..........: switch the LED \"on\", \"off\", let it \"blink\" or leave\n" \
    "                          it up to the driver using the value \"auto\"\n" \
    " ---------------------------------------------------------------\n\n"
    " [-t | --tvnorm ] ......: set TV-Norm pal, ntsc or secam\n"
    " ---------------------------------------------------------------\n");
#else
    fprintf(stderr, " [-f | --fps ]..........: frames per second\n" \
    "                          (activates YUYV format, disables MJPEG)\n" \
    " [-m | --minimum_size ].: drop frames smaller then this limit, useful\n" \
    "                          if the webcam produces small-sized garbage frames\n" \
    "                          may happen under low light conditions\n" \
    " [-e | --every_frame ]..: drop all frames except numbered\n" \
    " [-n | --no_dynctrl ]...: do not initalize dynctrls of Linux-UVC driver\n" \
    " [-l | --led ]..........: switch the LED \"on\", \"off\", let it \"blink\" or leave\n" \
    "                          it up to the driver using the value \"auto\"\n" \
    " [-t | --tvnorm ] ......: set TV-Norm pal, ntsc or secam\n"
    " ---------------------------------------------------------------\n");
#endif

    fprintf(stderr, "\n"\
    " Optional parameters (may not be supported by all cameras):\n\n"
    " [-br ].................: Set image brightness (auto or integer)\n"\
    " [-co ].................: Set image contrast (integer)\n"\
    " [-sh ].................: Set image sharpness (integer)\n"\
    " [-sa ].................: Set image saturation (integer)\n"\
    " [-cb ].................: Set color balance (auto or integer)\n"\
    " [-wb ].................: Set white balance (auto or integer)\n"\
    " [-ex ].................: Set exposure (auto, shutter-priority, aperature-priority, or integer)\n"\
    " [-bk ].................: Set backlight compensation (integer)\n"\
    " [-rot ]................: Set image rotation (0-359)\n"\
    " [-hf ].................: Set horizontal flip (true/false)\n"\
    " [-vf ].................: Set vertical flip (true/false)\n"\
    " [-pl ].................: Set power line filter (disabled, 50hz, 60hz, auto)\n"\
    " [-gain ]...............: Set gain (auto or integer)\n"\
    " [-cagc ]...............: Set chroma gain control (auto or integer)\n"\
    " ---------------------------------------------------------------\n\n"\
    );
}

/******************************************************************************
Description.: this thread worker grabs a frame and copies it to the global buffer
Input Value.: unused
Return Value: unused, always NULL
******************************************************************************/
void *cam_thread(void *arg)
{
    unsigned int every_count = 0;

    context *pcontext = arg;
    pglobal = pcontext->pglobal;

    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(cam_cleanup, pcontext);
    
    #define V4L_OPT_SET(vid, var, desc) \
      if (input_cmd(pcontext->id, vid, IN_CMD_V4L2, var, NULL) != 0) {\
          fprintf(stderr, "Failed to set " desc "\n"); \
      } else { \
          printf(" i: %-18s: %d\n", desc, var); \
      }
    
    #define V4L_INT_OPT(vid, var, desc) \
      if (var##_set) { \
          V4L_OPT_SET(vid, var, desc) \
      }
    
    /* V4L options */
    V4L_INT_OPT(V4L2_CID_SHARPNESS, sh, "sharpness")
    V4L_INT_OPT(V4L2_CID_CONTRAST, co, "contrast")
    V4L_INT_OPT(V4L2_CID_SATURATION, sa, "saturation")
    V4L_INT_OPT(V4L2_CID_BACKLIGHT_COMPENSATION, bk, "backlight compensation")
    V4L_INT_OPT(V4L2_CID_ROTATE, rot, "rotation")
    V4L_INT_OPT(V4L2_CID_HFLIP, hf, "hflip")
    V4L_INT_OPT(V4L2_CID_VFLIP, vf, "vflip")
    V4L_INT_OPT(V4L2_CID_VFLIP, pl, "power line filter")
    
    if (br_set) {
        V4L_OPT_SET(V4L2_CID_AUTOBRIGHTNESS, br_auto, "auto brightness mode")
        
        if (br_auto == 0) {
            V4L_OPT_SET(V4L2_CID_BRIGHTNESS, br, "brightness")
        }
    }
    
    if (wb_set) {
        V4L_OPT_SET(V4L2_CID_AUTO_WHITE_BALANCE, wb_auto, "auto white balance mode")
        
        if (wb_auto == 0) {
            V4L_OPT_SET(V4L2_CID_WHITE_BALANCE_TEMPERATURE, wb, "white balance temperature")
        }
    }
    
    if (ex_set) {
        V4L_OPT_SET(V4L2_CID_EXPOSURE_AUTO, ex_auto, "exposure mode")
        if (ex_auto == V4L2_EXPOSURE_MANUAL) {
            V4L_OPT_SET(V4L2_CID_EXPOSURE_ABSOLUTE, ex, "absolute exposure")
        }
    }
    
    if (gain_set) {
        V4L_OPT_SET(V4L2_CID_AUTOGAIN, gain_auto, "auto gain mode")
        
        if (gain_auto == 0) {
            V4L_OPT_SET(V4L2_CID_GAIN, gain, "gain")
        }
    }
    
    if (cagc_set) {
        V4L_OPT_SET(V4L2_CID_AUTO_WHITE_BALANCE, cagc_auto, "chroma gain mode")
        
        if (cagc_auto == 0) {
            V4L_OPT_SET(V4L2_CID_WHITE_BALANCE_TEMPERATURE, cagc, "chroma gain")
        }
    }
    
    if (cb_set) {
        V4L_OPT_SET(V4L2_CID_HUE_AUTO, cb_auto, "color balance mode")
        
        if (cb_auto == 0) {
            V4L_OPT_SET(V4L2_CID_HUE, cagc, "color balance")
        }
    }

    while(!pglobal->stop) {
        while(pcontext->videoIn->streamingState == STREAMING_PAUSED) {
            usleep(1); // maybe not the best way so FIXME
        }

        /* grab a frame */
        if(uvcGrab(pcontext->videoIn) < 0) {
            IPRINT("Error grabbing frames\n");
            exit(EXIT_FAILURE);
        }

        if ( every_count < every - 1 ) {
            DBG("dropping %d frame for every=%d\n", every_count + 1, every);
            ++every_count;
            continue;
        } else {
            every_count = 0;
        }

        //DBG("received frame of size: %d from plugin: %d\n", pcontext->videoIn->buf.bytesused, pcontext->id);

        /*
         * Workaround for broken, corrupted frames:
         * Under low light conditions corrupted frames may get captured.
         * The good thing is such frames are quite small compared to the regular pictures.
         * For example a VGA (640x480) webcam picture is normally >= 8kByte large,
         * corrupted frames are smaller.
         */
        if(pcontext->videoIn->buf.bytesused < minimum_size) {
            DBG("dropping too small frame, assuming it as broken\n");
            continue;
        }

        // use software frame dropping on low fps
        if (pcontext->videoIn->soft_framedrop == 1) {
            unsigned long last = pglobal->in[pcontext->id].timestamp.tv_sec * 1000 +
                                (pglobal->in[pcontext->id].timestamp.tv_usec/1000); // convert to ms
            unsigned long current = pcontext->videoIn->buf.timestamp.tv_sec * 1000 +
                                    pcontext->videoIn->buf.timestamp.tv_usec/1000; // convert to ms

            // if the requested time did not esplashed skip the frame
            if ((current - last) < pcontext->videoIn->frame_period_time) {
                //DBG("Last frame taken %d ms ago so drop it\n", (current - last));
                continue;
            }
            DBG("Lagg: %ld\n", (current - last) - pcontext->videoIn->frame_period_time);
        }

        /* copy JPG picture to global buffer */
        pthread_mutex_lock(&pglobal->in[pcontext->id].db);

        /*
         * If capturing in YUV mode convert to JPEG now.
         * This compression requires many CPU cycles, so try to avoid YUV format.
         * Getting JPEGs straight from the webcam, is one of the major advantages of
         * Linux-UVC compatible devices.
         */
        #ifndef NO_LIBJPEG
        if ((pcontext->videoIn->formatIn == V4L2_PIX_FMT_YUYV) || (pcontext->videoIn->formatIn == V4L2_PIX_FMT_RGB565)) {
            DBG("compressing frame from input: %d\n", (int)pcontext->id);
            pglobal->in[pcontext->id].size = compress_image_to_jpeg(pcontext->videoIn, pglobal->in[pcontext->id].buf, pcontext->videoIn->framesizeIn, gquality);
            /* copy this frame's timestamp to user space */
            pglobal->in[pcontext->id].timestamp = pcontext->videoIn->buf.timestamp;
        } else {
        #endif
            DBG("copying frame from input: %d\n", (int)pcontext->id);
            pglobal->in[pcontext->id].size = memcpy_picture(pglobal->in[pcontext->id].buf, pcontext->videoIn->tmpbuffer, pcontext->videoIn->tmpbytesused);
            /* copy this frame's timestamp to user space */
            pglobal->in[pcontext->id].timestamp = pcontext->videoIn->tmptimestamp;
        #ifndef NO_LIBJPEG
        }
        #endif

#if 0
        /* motion detection can be done just by comparing the picture size, but it is not very accurate!! */
        if((prev_size - global->size)*(prev_size - global->size) > 4 * 1024 * 1024) {
            DBG("motion detected (delta: %d kB)\n", (prev_size - global->size) / 1024);
        }
        prev_size = global->size;
#endif


        /* signal fresh_frame */
        pthread_cond_broadcast(&pglobal->in[pcontext->id].db_update);
        pthread_mutex_unlock(&pglobal->in[pcontext->id].db);
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
    context *pcontext = arg;
    pglobal = pcontext->pglobal;
    if(!first_run) {
        DBG("already cleaned up ressources\n");
        return;
    }

    first_run = 0;
    IPRINT("cleaning up ressources allocated by input thread\n");

    close_v4l2(pcontext->videoIn);
    if(pcontext->videoIn->tmpbuffer != NULL) free(pcontext->videoIn->tmpbuffer);
    if(pcontext->videoIn != NULL) free(pcontext->videoIn);
    if(pglobal->in[pcontext->id].buf != NULL)
        free(pglobal->in[pcontext->id].buf);
}

/******************************************************************************
Description.: process commands, allows to set v4l2 controls
Input Value.: * control specifies the selected v4l2 control's id
                see struct v4l2_queryctr in the videodev2.h
              * value is used for control that make use of a parameter.
Return Value: depends in the command, for most cases 0 means no errors and
              -1 signals an error. This is just rule of thumb, not more!
******************************************************************************/
int input_cmd(int plugin_number, unsigned int control_id, unsigned int group, int value, char *value_string)
{
    int ret = -1;
    int i = 0;
    DBG("Requested cmd (id: %d) for the %d plugin. Group: %d value: %d\n", control_id, plugin_number, group, value);
    switch(group) {
    case IN_CMD_GENERIC: {
            int i;
            for (i = 0; i<pglobal->in[plugin_number].parametercount; i++) {
                if ((pglobal->in[plugin_number].in_parameters[i].ctrl.id == control_id) &&
                    (pglobal->in[plugin_number].in_parameters[i].group == IN_CMD_GENERIC)){
                    DBG("Generic control found (id: %d): %s\n", control_id, pglobal->in[plugin_number].in_parameters[i].ctrl.name);
                    DBG("New %s value: %d\n", pglobal->in[plugin_number].in_parameters[i].ctrl.name, value);
                    return 0;
                }
            }
            DBG("Requested generic control (%d) did not found\n", control_id);
            return -1;
        } break;
    case IN_CMD_V4L2: {
            ret = v4l2SetControl(cams[plugin_number].videoIn, control_id, value, plugin_number, pglobal);
            if(ret == 0) {
                pglobal->in[plugin_number].in_parameters[i].value = value;
            } else {
                DBG("v4l2SetControl failed: %d\n", ret);
            }
            return ret;
        } break;
    case IN_CMD_RESOLUTION: {
        // the value points to the current formats nth resolution
        if(value > (pglobal->in[plugin_number].in_formats[pglobal->in[plugin_number].currentFormat].resolutionCount - 1)) {
            DBG("The value is out of range");
            return -1;
        }
        int height = pglobal->in[plugin_number].in_formats[pglobal->in[plugin_number].currentFormat].supportedResolutions[value].height;
        int width = pglobal->in[plugin_number].in_formats[pglobal->in[plugin_number].currentFormat].supportedResolutions[value].width;
        ret = setResolution(cams[plugin_number].videoIn, width, height);
        if(ret == 0) {
            pglobal->in[plugin_number].in_formats[pglobal->in[plugin_number].currentFormat].currentResolution = value;
        }
        return ret;
    } break;
    case IN_CMD_JPEG_QUALITY:
        if((value >= 0) && (value < 101)) {
            pglobal->in[plugin_number].jpegcomp.quality = value;
            if(IOCTL_VIDEO(cams[plugin_number].videoIn->fd, VIDIOC_S_JPEGCOMP, &pglobal->in[plugin_number].jpegcomp) != EINVAL) {
                DBG("JPEG quality is set to %d\n", value);
                ret = 0;
            } else {
                DBG("Setting the JPEG quality is not supported\n");
            }
        } else {
            DBG("Quality is out of range\n");
        }
        break;
    }
    return ret;
}

