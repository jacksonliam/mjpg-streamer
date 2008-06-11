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

#include "../../utils.h"
#include "../../mjpg_streamer.h"
#include "v4l2uvc.h"
#include "huffman.h"
#include "jpeg_utils.h"
#include "dynctrl.h"

#define INPUT_PLUGIN_NAME "UVC webcam grabber"
#define MAX_ARGUMENTS 32

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

/* private functions and variables to this plugin */
pthread_t cam;
pthread_mutex_t controls_mutex;
struct vdIn *videoIn;
static globals *pglobal;
static int gquality = 80;
static unsigned int minimum_size = 0;
static int dynctrls = 1;

void *cam_thread( void *);
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
int input_init(input_parameter *param) {
  char *argv[MAX_ARGUMENTS]={NULL}, *dev = "/dev/video0", *s;
  int argc=1, width=640, height=480, fps=5, format=V4L2_PIX_FMT_MJPEG, i;
  in_cmd_type led = IN_CMD_LED_AUTO;

  /* initialize the mutes variable */
  if( pthread_mutex_init(&controls_mutex, NULL) != 0 ) {
    IPRINT("could not initialize mutex variable\n");
    exit(EXIT_FAILURE);
  }

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

  /* parse the parameters */
  reset_getopt();
  while(1) {
    int option_index = 0, c=0;
    static struct option long_options[] = \
    {
      {"h", no_argument, 0, 0},
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
      {0, 0, 0, 0}
    };

    /* parsing all parameters according to the list above is sufficent */
    c = getopt_long_only(argc, argv, "", long_options, &option_index);

    /* no more options to parse */
    if (c == -1) break;

    /* unrecognized option */
    if (c == '?'){
      help();
      return 1;
    }

    /* dispatch the given options */
    switch (option_index) {
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
        for ( i=0; i < LENGTH_OF(resolutions); i++ ) {
          if ( strcmp(resolutions[i].string, optarg) == 0 ) {
            width  = resolutions[i].width;
            height = resolutions[i].height;
          }
        }
        /* done if width and height were set */
        if(width != -1 && height != -1)
          break;
        /* parse value as decimal value */
        width  = strtol(optarg, &s, 10);
        height = strtol(s+1, NULL, 10);
        break;

      /* f, fps */
      case 6:
      case 7:
        DBG("case 6,7\n");
        fps=atoi(optarg);
        break;

      /* y, yuv */
      case 8:
      case 9:
        DBG("case 8,9\n");
        format = V4L2_PIX_FMT_YUYV;
        break;

      /* q, quality */
      case 10:
      case 11:
        DBG("case 10,11\n");
        format = V4L2_PIX_FMT_YUYV;
        gquality = MIN(MAX(atoi(optarg), 0), 100);
        break;

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
      case 17:
        DBG("case 16,17\n");
        if ( strcmp("on", optarg) == 0 ) {
          led = IN_CMD_LED_ON;
        } else if ( strcmp("off", optarg) == 0 ) {
          led = IN_CMD_LED_OFF;
        } else if ( strcmp("auto", optarg) == 0 ) {
          led = IN_CMD_LED_AUTO;
        } else if ( strcmp("blink", optarg) == 0 ) {
          led = IN_CMD_LED_BLINK;
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
  if ( videoIn == NULL ) {
    IPRINT("not enough memory for videoIn\n");
    exit(EXIT_FAILURE);
  }
  memset(videoIn, 0, sizeof(struct vdIn));

  /* display the parsed values */
  IPRINT("Using V4L2 device.: %s\n", dev);
  IPRINT("Desired Resolution: %i x %i\n", width, height);
  IPRINT("Frames Per Second.: %i\n", fps);
  IPRINT("Format............: %s\n", (format==V4L2_PIX_FMT_YUYV)?"YUV":"MJPEG");
  if ( format == V4L2_PIX_FMT_YUYV )
    IPRINT("JPEG Quality......: %d\n", gquality);

  /* open video device and prepare data structure */
  if (init_videoIn(videoIn, dev, width, height, fps, format, 1) < 0) {
    IPRINT("init_VideoIn failed\n");
    closelog();
    exit(EXIT_FAILURE);
  }

  /*
   * recent linux-uvc driver (revision > ~#125) requires to use dynctrls
   * for pan/tilt/focus/...
   * dynctrls must get initialized
   */
  if (dynctrls)
    initDynCtrls(videoIn->fd);

  /*
   * switch the LED according to the command line parameters (if any)
   */
  input_cmd(led, 0);

  return 0;
}

/******************************************************************************
Description.: Stops the execution of worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int input_stop(void) {
  DBG("will cancel input thread\n");
  pthread_cancel(cam);

  return 0;
}

/******************************************************************************
Description.: spins of a worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int input_run(void) {
  pglobal->buf = malloc(videoIn->framesizeIn);
  if (pglobal->buf == NULL) {
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
int input_cmd(in_cmd_type cmd, int value) {
  int res=0;
  static int pan=0, tilt=0, pan_tilt_valid=-1;
  static int focus=-1;
  const int one_degree = ONE_DEGREE;

  /* certain commands do not need the mutex */
  if ( cmd != IN_CMD_RESET_PAN_TILT_NO_MUTEX )
    pthread_mutex_lock( &controls_mutex );

  switch (cmd) {
    case IN_CMD_HELLO:
      fprintf(stderr, "Hello from input plugin\n");
      break;

    case IN_CMD_RESET:
      DBG("about to reset all image controls to defaults\n");
      res = v4l2ResetControl(videoIn, V4L2_CID_BRIGHTNESS);
      res |= v4l2ResetControl(videoIn, V4L2_CID_CONTRAST);
      res |= v4l2ResetControl(videoIn, V4L2_CID_SATURATION);
      res |= v4l2ResetControl(videoIn, V4L2_CID_GAIN);
      if ( res != 0 ) res = -1;
      break;

    case IN_CMD_RESET_PAN_TILT:
    case IN_CMD_RESET_PAN_TILT_NO_MUTEX:
      DBG("about to set pan/tilt to default position\n");
      if ( uvcPanTilt(videoIn->fd, 0, 0, 3) != 0 ) {
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
      if ( pan_tilt_valid != 1 ) {
        if ( input_cmd(IN_CMD_RESET_PAN_TILT_NO_MUTEX, 0) == -1 ) {
          res = -1;
          break;
        }
      }

      /* limit pan-value to min and max, multiply it with constant "one_degree" */
      value = MIN(MAX(value*one_degree, MIN_PAN), MAX_PAN);

      /* calculate the relative degrees to move to the desired absolute pan-value */
      if( (res = value - pan) == 0 ) {
        /* do not move if this would mean to move by 0 degrees */
        res = pan/one_degree;
        break;
      }

      /* move it */
      pan = value;
      uvcPanTilt(videoIn->fd, res, 0, 0);
      res = pan/one_degree;

      DBG("pan: %d\n", pan);
      break;

    case IN_CMD_PAN_PLUS:
      DBG("pan +\n");

      if ( pan_tilt_valid != 1 ) {
        if ( input_cmd(IN_CMD_RESET_PAN_TILT_NO_MUTEX, 0) == -1 ) {
          res = -1;
          break;
        }
      }

      if ( (MAX_PAN) >= (pan+MIN_RES) ) {
        pan += MIN_RES;
        uvcPanTilt(videoIn->fd, MIN_RES, 0, 0);
      }
      res = pan/one_degree;

      DBG("pan: %d\n", pan);
      break;

    case IN_CMD_PAN_MINUS:
      DBG("pan -\n");

      if ( pan_tilt_valid != 1 ) {
        if ( input_cmd(IN_CMD_RESET_PAN_TILT_NO_MUTEX, 0) == -1 ) {
          res = -1;
          break;
        }
      }

      if ( (MIN_PAN) <= (pan-MIN_RES) ) {
        pan -= MIN_RES;
        uvcPanTilt(videoIn->fd, -MIN_RES, 0, 0);
      }
      res = pan/one_degree;

      DBG("pan: %d\n", pan);
      break;

    case IN_CMD_TILT_SET:
      DBG("set tilt to %d degrees\n", value);

      if ( pan_tilt_valid != 1 ) {
        if ( input_cmd(IN_CMD_RESET_PAN_TILT_NO_MUTEX, 0) == -1 ) {
          res = -1;
          break;
        }
      }

      /* limit pan-value to min and max, multiply it with constant "one_degree" */
      value = MIN(MAX(value*one_degree, MIN_TILT), MAX_TILT);

      /* calculate the relative degrees to move to the desired absolute pan-value */
      if( (res = value - tilt) == 0 ) {
        /* do not move if this would mean to move by 0 degrees */
        res = tilt/one_degree;
        break;
      }

      /* move it */
      tilt = value;
      uvcPanTilt(videoIn->fd, 0, res, 0);
      res = tilt/one_degree;

      DBG("tilt: %d\n", tilt);
      break;

    case IN_CMD_TILT_PLUS:
      DBG("tilt +\n");

      if ( pan_tilt_valid != 1 ) {
        if ( input_cmd(IN_CMD_RESET_PAN_TILT_NO_MUTEX, 0) == -1 ) {
          res = -1;
          break;
        }
      }

      if ( (MAX_TILT) >= (tilt+MIN_RES) ) {
        tilt += MIN_RES;
        uvcPanTilt(videoIn->fd, 0, MIN_RES, 0);
      }
      res = tilt/one_degree;

      DBG("tilt: %d\n", tilt);
      break;

    case IN_CMD_TILT_MINUS:
      DBG("tilt -\n");

      if ( pan_tilt_valid != 1 ) {
        if ( input_cmd(IN_CMD_RESET_PAN_TILT_NO_MUTEX, 0) == -1 ) {
          res = -1;
          break;
        }
      }

      if ( (MIN_TILT) <= (tilt-MIN_RES) ) {
        tilt -= MIN_RES;
        uvcPanTilt(videoIn->fd, 0, -MIN_RES, 0);
      }
      res = tilt/one_degree;

      DBG("tilt: %d\n", tilt);
      break;

    case IN_CMD_SATURATION_PLUS:
      DBG("saturation + (%d)\n", v4l2GetControl (videoIn, V4L2_CID_SATURATION));
      res = v4l2UpControl(videoIn, V4L2_CID_SATURATION);
      break;

    case IN_CMD_SATURATION_MINUS:
      DBG("saturation - (%d)\n", v4l2GetControl (videoIn, V4L2_CID_SATURATION));
      res = v4l2DownControl(videoIn, V4L2_CID_SATURATION);
      break;

    case IN_CMD_CONTRAST_PLUS:
      DBG("contrast + (%d)\n", v4l2GetControl (videoIn, V4L2_CID_CONTRAST));
      res = v4l2UpControl(videoIn, V4L2_CID_CONTRAST);
      break;

    case IN_CMD_CONTRAST_MINUS:
      DBG("contrast - (%d)\n", v4l2GetControl (videoIn, V4L2_CID_CONTRAST));
      res = v4l2DownControl(videoIn, V4L2_CID_CONTRAST);
      break;

    case IN_CMD_BRIGHTNESS_PLUS:
      DBG("brightness + (%d)\n", v4l2GetControl (videoIn, V4L2_CID_BRIGHTNESS));
      res = v4l2UpControl(videoIn, V4L2_CID_BRIGHTNESS);
      break;

    case IN_CMD_BRIGHTNESS_MINUS:
      DBG("brightness - (%d)\n", v4l2GetControl (videoIn, V4L2_CID_BRIGHTNESS));
      res = v4l2DownControl(videoIn, V4L2_CID_BRIGHTNESS);
      break;

    case IN_CMD_GAIN_PLUS:
      DBG("gain + (%d)\n", v4l2GetControl (videoIn, V4L2_CID_GAIN));
      res = v4l2UpControl(videoIn, V4L2_CID_GAIN);
      break;

    case IN_CMD_GAIN_MINUS:
      DBG("gain - (%d)\n", v4l2GetControl (videoIn, V4L2_CID_GAIN));
      res = v4l2DownControl(videoIn, V4L2_CID_GAIN);
      break;

    case IN_CMD_FOCUS_PLUS:
      DBG("focus + (%d)\n", focus);

      value=MIN(MAX(focus+10,0),255);

      if ( (res = v4l2SetControl(videoIn, V4L2_CID_FOCUS_LOGITECH, value)) == 0) {
        focus = value;
      }
      res = focus;
      break;

    case IN_CMD_FOCUS_MINUS:
      DBG("focus - (%d)\n", focus);

      value=MIN(MAX(focus-10,0),255);

      if ( (res = v4l2SetControl(videoIn, V4L2_CID_FOCUS_LOGITECH, value)) == 0) {
        focus = value;
      }
      res = focus;
      break;

    case IN_CMD_FOCUS_SET:
      value=MIN(MAX(value,0),255);
      DBG("set focus to %d\n", value);

      if ( (res = v4l2SetControl(videoIn, V4L2_CID_FOCUS_LOGITECH, value)) == 0) {
        focus = value;
      }
      res = focus;
      break;

    /* switch the webcam LED permanently on */
    case IN_CMD_LED_ON:
      res = v4l2SetControl(videoIn, V4L2_CID_LED1_MODE_LOGITECH, 1);
    break;

    /* switch the webcam LED permanently off */
    case IN_CMD_LED_OFF:
      res = v4l2SetControl(videoIn, V4L2_CID_LED1_MODE_LOGITECH, 0);
    break;

    /* switch the webcam LED on if streaming, off if not streaming */
    case IN_CMD_LED_AUTO:
      res = v4l2SetControl(videoIn, V4L2_CID_LED1_MODE_LOGITECH, 3);
    break;

    /* let the webcam LED blink at a given hardcoded intervall */
    case IN_CMD_LED_BLINK:
      res = v4l2SetControl(videoIn, V4L2_CID_LED1_MODE_LOGITECH, 2);
      res = v4l2SetControl(videoIn, V4L2_CID_LED1_FREQUENCY_LOGITECH, 255);
    break;

    default:
      DBG("nothing matched\n");
      res = -1;
  }

  if ( cmd != IN_CMD_RESET_PAN_TILT_NO_MUTEX )
    pthread_mutex_unlock( &controls_mutex );

  return res;
}

/*** private functions for this plugin below ***/
/******************************************************************************
Description.: print a help message to stderr
Input Value.: -
Return Value: -
******************************************************************************/
void help(void) {
  int i;

  fprintf(stderr, " ---------------------------------------------------------------\n" \
                  " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
                  " ---------------------------------------------------------------\n" \
                  " The following parameters can be passed to this plugin:\n\n" \
                  " [-d | --device ].......: video device to open (your camera)\n" \
                  " [-r | --resolution ]...: the resolution of the video device,\n" \
                  "                          can be one of the following strings:\n" \
                  "                          ");

  for ( i=0; i < LENGTH_OF(resolutions); i++ ) {
    fprintf(stderr, "%s ", resolutions[i].string);
    if ( (i+1)%6 == 0)
      fprintf(stderr, "\n                          ");
  }
  fprintf(stderr, "\n                          or a custom value like the following" \
                  "\n                          example: 640x480\n");

  fprintf(stderr, " [-f | --fps ]..........: frames per second\n" \
                  " [-y | --yuv ]..........: enable YUYV format and disable MJPEG mode\n" \
                  " [-q | --quality ]......: JPEG compression quality in percent \n" \
                  "                          (activates YUYV format, disables MJPEG)\n" \
                  " [-m | --minimum_size ].: drop frames smaller then this limit, useful\n" \
                  "                          if the webcam produces small-sized garbage frames\n" \
                  "                          may happen under low light conditions\n" \
                  " [-n | --no_dynctrl ]...: do not initalize dynctrls of Linux-UVC driver\n" \
                  " [-l | --led ]..........: switch the LED \"on\", \"off\", let it \"blink\" or leave\n" \
                  "                          it up to the driver using the value \"auto\"\n" \
                  " ---------------------------------------------------------------\n\n");
}

/******************************************************************************
Description.: this thread worker grabs a frame and copies it to the global buffer
Input Value.: unused
Return Value: unused, always NULL
******************************************************************************/
void *cam_thread( void *arg ) {
  /* set cleanup handler to cleanup allocated ressources */
  pthread_cleanup_push(cam_cleanup, NULL);

  while( !pglobal->stop ) {

    /* grab a frame */
    if( uvcGrab(videoIn) < 0 ) {
      IPRINT("Error grabbing frames\n");
      exit(EXIT_FAILURE);
    }
  
    DBG("received frame of size: %d\n", videoIn->buf.bytesused);

    /*
     * Workaround for broken, corrupted frames:
     * Under low light conditions corrupted frames may get captured.
     * The good thing is such frames are quite small compared to the regular pictures.
     * For example a VGA (640x480) webcam picture is normally >= 8kByte large,
     * corrupted frames are smaller.
     */
    if ( videoIn->buf.bytesused < minimum_size ) {
      DBG("dropping too small frame, assuming it as broken\n");
      continue;
    }

    /* copy JPG picture to global buffer */
    pthread_mutex_lock( &pglobal->db );

    /*
     * If capturing in YUV mode convert to JPEG now.
     * This compression requires many CPU cycles, so try to avoid YUV format.
     * Getting JPEGs straight from the webcam, is one of the major advantages of
     * Linux-UVC compatible devices.
     */
    if (videoIn->formatIn == V4L2_PIX_FMT_YUYV) {
      DBG("compressing frame\n");
      pglobal->size = compress_yuyv_to_jpeg(videoIn, pglobal->buf, videoIn->framesizeIn, gquality);
    }
    else {
      DBG("copying frame\n");
      pglobal->size = memcpy_picture(pglobal->buf, videoIn->tmpbuffer, videoIn->buf.bytesused);
    }

#if 0
    /* motion detection can be done just by comparing the picture size, but it is not very accurate!! */
    if ( (prev_size - global->size)*(prev_size - global->size) > 4*1024*1024 ) {
        DBG("motion detected (delta: %d kB)\n", (prev_size - global->size) / 1024);
    }
    prev_size = global->size;
#endif

    /* signal fresh_frame */
    pthread_cond_broadcast(&pglobal->db_update);
    pthread_mutex_unlock( &pglobal->db );

    DBG("waiting for next frame\n");

    /* only use usleep if the fps is below 5, otherwise the overhead is too long */
    if ( videoIn->fps < 5 ) {
      usleep(1000*1000/videoIn->fps);
    }
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
void cam_cleanup(void *arg) {
  static unsigned char first_run=1;

  if ( !first_run ) {
    DBG("already cleaned up ressources\n");
    return;
  }

  first_run = 0;
  IPRINT("cleaning up ressources allocated by input thread\n");

  /* restore behaviour of the LED to auto */
  input_cmd(IN_CMD_LED_AUTO, 0);

  close_v4l2(videoIn);
  if (videoIn->tmpbuffer != NULL) free(videoIn->tmpbuffer);
  if (videoIn != NULL) free(videoIn);
  if (pglobal->buf != NULL) free(pglobal->buf);
}




