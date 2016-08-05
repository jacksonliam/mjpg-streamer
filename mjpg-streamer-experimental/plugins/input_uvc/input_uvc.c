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
static unsigned int minimum_size = 0;
static int dynctrls = 1;
static unsigned int every = 1;

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

static context_settings * init_settings() {
    context_settings *settings;
    
    settings = calloc(1, sizeof(context_settings));
    if (settings == NULL) {
        IPRINT("error allocating context");
        exit(EXIT_FAILURE);
    }
    
    settings->quality = 80;
    return settings;
}


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
    context *pctx;
    context_settings *settings;
    
    pctx = calloc(1, sizeof(context));
    if (pctx == NULL) {
        IPRINT("error allocating context");
        exit(EXIT_FAILURE);
    }
    
    settings = pctx->init_settings = init_settings();
    pglobal = param->global;
    pglobal->in[id].context = pctx;

    /* initialize the mutes variable */
    if(pthread_mutex_init(&pctx->controls_mutex, NULL) != 0) {
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
            {"u", no_argument, 0, 0},
            {"uyvy", no_argument, 0, 0},
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
            dev = realpath(optarg, NULL);
            break;

        /* r, resolution */
        case 4:
        case 5:
            DBG("case 4,5\n");
            parse_resolution_opt(optarg, &width, &height);
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
	/* u, uyvy */
        #ifndef NO_LIBJPEG
        case 10:
        case 11:
            DBG("case 10,11\n");
            format = V4L2_PIX_FMT_UYVY;
            break;
        #endif
        /* q, quality */
        #ifndef NO_LIBJPEG
        case 12:
        OPTION_INT(13, quality)
            settings->quality = MIN(MAX(settings->quality, 0), 100);
            break;
        #endif
        /* m, minimum_size */
        case 14:
        case 15:
            DBG("case 14,15\n");
            minimum_size = MAX(atoi(optarg), 0);
            break;

        /* n, no_dynctrl */
        case 16:
        case 17:
            DBG("case 16,17\n");
            dynctrls = 0;
            break;

            /* l, led */
        case 18:
        case 19:/*
        DBG("case 18,19\n");
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
        case 20:
            DBG("case 20\n");
            if (strcmp(optarg, "RGBP") == 0) {
                format = V4L2_PIX_FMT_RGB565;
            } else {
              fprintf(stderr," i: FOURCC codec '%s' not supported\n", optarg);
            }
            break;
        #endif
        /* t, tvnorm */
        case 21:
        case 22:
            DBG("case 21,22\n");
            if (strcasecmp("pal",optarg) == 0 ) {
	             tvnorm = V4L2_STD_PAL;
            } else if ( strcasecmp("ntsc",optarg) == 0 ) {
	             tvnorm = V4L2_STD_NTSC;
            } else if ( strcasecmp("secam",optarg) == 0 ) {
	             tvnorm = V4L2_STD_SECAM;
            }
            break;
        case 23:
        /* e, every */
        case 24:
            DBG("case 24\n");
            every = MAX(atoi(optarg), 1);
            break;

        /* options */
        OPTION_INT(25, sh)
            break;
        OPTION_INT(26, co)
            break;
        OPTION_INT_AUTO(27, br)
            break;
        OPTION_INT(28, sa)
            break;
        OPTION_INT_AUTO(29, wb)
            break;
        OPTION_MULTI_OR_INT(30, ex_auto, V4L2_EXPOSURE_MANUAL, ex, exposures)
            break;
        OPTION_INT(31, bk)
            break;
        OPTION_INT(32, rot)
            break;
        OPTION_BOOL(33, hf)
            break;
        OPTION_BOOL(34, vf)
            break;
        OPTION_MULTI(35, pl, power_line)
            break;
        OPTION_INT_AUTO(36, gain)
            break;
        OPTION_INT_AUTO(37, cagc)
            break;
        OPTION_INT_AUTO(38, cb)
            break;
    
        default:
            DBG("default case\n");
            help();
            return 1;
        }
    }
    DBG("input id: %d\n", id);
    pctx->id = id;
    pctx->pglobal = param->global;

    /* allocate webcam datastructure */
    pctx->videoIn = calloc(1, sizeof(struct vdIn));
    if(pctx->videoIn == NULL) {
        IPRINT("not enough memory for videoIn\n");
        exit(EXIT_FAILURE);
    }
    
    /* display the parsed values */
    IPRINT("Using V4L2 device.: %s\n", dev);
    IPRINT("Desired Resolution: %i x %i\n", width, height);
    IPRINT("Frames Per Second.: %i\n", fps);
    char *fmtString = NULL;
    switch (format) {
        case V4L2_PIX_FMT_MJPEG:
            fmtString = "JPEG";
            break;
        #ifndef NO_LIBJPG
            case V4L2_PIX_FMT_YUYV:
                fmtString = "YUYV";
                break;
            case V4L2_PIX_FMT_UYVY:
                fmtString = "UYVY";
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
            IPRINT("JPEG Quality......: %d\n", settings->quality);
    #endif

    if (tvnorm != V4L2_STD_UNKNOWN) {
        IPRINT("TV-Norm...........: %s\n", get_name_by_tvnorm(tvnorm));
    } else {
        IPRINT("TV-Norm...........: DEFAULT\n");
    }

    DBG("vdIn pn: %d\n", id);
    /* open video device and prepare data structure */
    if(init_videoIn(pctx->videoIn, dev, width, height, fps, format, 1, pctx->pglobal, id, tvnorm) < 0) {
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
        initDynCtrls(pctx->videoIn->fd);
    
    enumerateControls(pctx->videoIn, pctx->pglobal, id); // enumerate V4L2 controls after UVC extended mapping
    
    return 0;
}

/******************************************************************************
Description.: Stops the execution of worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int input_stop(int id)
{
    input * in = &pglobal->in[id];
    context *pctx = (context*)in->context;
    
    DBG("will cancel camera thread #%02d\n", id);
    pthread_cancel(pctx->threadID);
    return 0;
}

/******************************************************************************
Description.: spins of a worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int input_run(int id)
{
    input * in = &pglobal->in[id];
    context *pctx = (context*)in->context;
    
    in->buf = malloc(pctx->videoIn->framesizeIn);
    if(in->buf == NULL) {
        fprintf(stderr, "could not allocate memory\n");
        exit(EXIT_FAILURE);
    }

    DBG("launching camera thread #%02d\n", id);
    /* create thread and pass context to thread function */
    pthread_create(&(pctx->threadID), NULL, cam_thread, in);
    pthread_detach(pctx->threadID);
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
    
    resolutions_help("                          ");

    fprintf(stderr,
    " [-f | --fps ]..........: frames per second\n" \
    "                          (activates YUYV format, disables MJPEG)\n" \
    " [-q | --quality ] .....: set quality of JPEG encoding\n" \
    " [-m | --minimum_size ].: drop frames smaller then this limit, useful\n" \
    "                          if the webcam produces small-sized garbage frames\n" \
    "                          may happen under low light conditions\n" \
    " [-e | --every_frame ]..: drop all frames except numbered\n" \
    " [-n | --no_dynctrl ]...: do not initalize dynctrls of Linux-UVC driver\n" \
    " [-l | --led ]..........: switch the LED \"on\", \"off\", let it \"blink\" or leave\n" \
    "                          it up to the driver using the value \"auto\"\n" \
    " [-t | --tvnorm ] ......: set TV-Norm pal, ntsc or secam\n" \
    " [-u | --uyvy ] ........: Use UYVY format, default: MJPEG (uses more cpu power)\n" \
    " [-y | --yuv  ] ........: Use YUV format, default: MJPEG (uses more cpu power)\n" \
    " [-fourcc ] ............: Use FOURCC codec 'argopt', \n" \
    "                          currently supported codecs are: RGBP \n" \
    " ---------------------------------------------------------------\n");

    fprintf(stderr, "\n"                                                \
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
    input * in = (input*)arg;
    context *pcontext = (context*)in->context;
    context_settings *settings = pcontext->init_settings;
    
    unsigned int every_count = 0;
    int quality = settings->quality;
    
    /* set cleanup handler to cleanup allocated resources */
    pthread_cleanup_push(cam_cleanup, in);
    
    #define V4L_OPT_SET(vid, var, desc) \
      if (input_cmd(pcontext->id, vid, IN_CMD_V4L2, settings->var, NULL) != 0) {\
          fprintf(stderr, "Failed to set " desc "\n"); \
      } else { \
          printf(" i: %-18s: %d\n", desc, settings->var); \
      }
    
    #define V4L_INT_OPT(vid, var, desc) \
      if (settings->var##_set) { \
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
    
    if (settings->br_set) {
        V4L_OPT_SET(V4L2_CID_AUTOBRIGHTNESS, br_auto, "auto brightness mode")
        
        if (settings->br_auto == 0) {
            V4L_OPT_SET(V4L2_CID_BRIGHTNESS, br, "brightness")
        }
    }
    
    if (settings->wb_set) {
        V4L_OPT_SET(V4L2_CID_AUTO_WHITE_BALANCE, wb_auto, "auto white balance mode")
        
        if (settings->wb_auto == 0) {
            V4L_OPT_SET(V4L2_CID_WHITE_BALANCE_TEMPERATURE, wb, "white balance temperature")
        }
    }
    
    if (settings->ex_set) {
        V4L_OPT_SET(V4L2_CID_EXPOSURE_AUTO, ex_auto, "exposure mode")
        if (settings->ex_auto == V4L2_EXPOSURE_MANUAL) {
            V4L_OPT_SET(V4L2_CID_EXPOSURE_ABSOLUTE, ex, "absolute exposure")
        }
    }
    
    if (settings->gain_set) {
        V4L_OPT_SET(V4L2_CID_AUTOGAIN, gain_auto, "auto gain mode")
        
        if (settings->gain_auto == 0) {
            V4L_OPT_SET(V4L2_CID_GAIN, gain, "gain")
        }
    }
    
    if (settings->cagc_set) {
        V4L_OPT_SET(V4L2_CID_AUTO_WHITE_BALANCE, cagc_auto, "chroma gain mode")
        
        if (settings->cagc_auto == 0) {
            V4L_OPT_SET(V4L2_CID_WHITE_BALANCE_TEMPERATURE, cagc, "chroma gain")
        }
    }
    
    if (settings->cb_set) {
        V4L_OPT_SET(V4L2_CID_HUE_AUTO, cb_auto, "color balance mode")
        
        if (settings->cb_auto == 0) {
            V4L_OPT_SET(V4L2_CID_HUE, cagc, "color balance")
        }
    }
    
    free(settings);
    settings = NULL;
    pcontext->init_settings = NULL;

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

        //DBG("received frame of size: %d from plugin: %d\n", pcontext->videoIn->tmpbytesused, pcontext->id);

        /*
         * Workaround for broken, corrupted frames:
         * Under low light conditions corrupted frames may get captured.
         * The good thing is such frames are quite small compared to the regular pictures.
         * For example a VGA (640x480) webcam picture is normally >= 8kByte large,
         * corrupted frames are smaller.
         */
        if(pcontext->videoIn->tmpbytesused < minimum_size) {
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
        if ((pcontext->videoIn->formatIn == V4L2_PIX_FMT_YUYV) ||
	    (pcontext->videoIn->formatIn == V4L2_PIX_FMT_UYVY) ||
	    (pcontext->videoIn->formatIn == V4L2_PIX_FMT_RGB565) ) {
            DBG("compressing frame from input: %d\n", (int)pcontext->id);
            pglobal->in[pcontext->id].size = compress_image_to_jpeg(pcontext->videoIn, pglobal->in[pcontext->id].buf, pcontext->videoIn->framesizeIn, quality);
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
    input * in = (input*)arg;
    context *pctx = (context*)in->context;
    
    IPRINT("cleaning up resources allocated by input thread\n");

    if (pctx->videoIn != NULL) {
        close_v4l2(pctx->videoIn);
        free(pctx->videoIn->tmpbuffer);
        free(pctx->videoIn);
        pctx->videoIn = NULL;
    }
    
    free(in->buf);
    in->buf = NULL;
    in->size = 0;
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
    input * in = &pglobal->in[plugin_number];
    context *pctx = (context*)in->context;
    
    int ret = -1;
    int i = 0;
    DBG("Requested cmd (id: %d) for the %d plugin. Group: %d value: %d\n", control_id, plugin_number, group, value);
    switch(group) {
    case IN_CMD_GENERIC: {
            int i;
            for (i = 0; i<in->parametercount; i++) {
                if ((in->in_parameters[i].ctrl.id == control_id) &&
                    (in->in_parameters[i].group == IN_CMD_GENERIC)){
                    DBG("Generic control found (id: %d): %s\n", control_id, in->in_parameters[i].ctrl.name);
                    DBG("New %s value: %d\n", in->in_parameters[i].ctrl.name, value);
                    return 0;
                }
            }
            DBG("Requested generic control (%d) did not found\n", control_id);
            return -1;
        } break;
    case IN_CMD_V4L2: {
            ret = v4l2SetControl(pctx->videoIn, control_id, value, plugin_number, pglobal);
            if(ret == 0) {
                in->in_parameters[i].value = value;
            } else {
                DBG("v4l2SetControl failed: %d\n", ret);
            }
            return ret;
        } break;
    case IN_CMD_RESOLUTION: {
        // the value points to the current formats nth resolution
        if(value > (in->in_formats[in->currentFormat].resolutionCount - 1)) {
            DBG("The value is out of range");
            return -1;
        }
        int height = in->in_formats[in->currentFormat].supportedResolutions[value].height;
        int width = in->in_formats[in->currentFormat].supportedResolutions[value].width;
        ret = setResolution(pctx->videoIn, width, height);
        if(ret == 0) {
            in->in_formats[in->currentFormat].currentResolution = value;
        }
        return ret;
    } break;
    case IN_CMD_JPEG_QUALITY:
        if((value >= 0) && (value < 101)) {
            in->jpegcomp.quality = value;
            if(IOCTL_VIDEO(pctx->videoIn->fd, VIDIOC_S_JPEGCOMP, &in->jpegcomp) != EINVAL) {
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

