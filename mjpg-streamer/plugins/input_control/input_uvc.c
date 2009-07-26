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
#include "dynctrl.h"

#define INPUT_PLUGIN_NAME "UVC webcam control"
#define MAX_ARGUMENTS 8

typedef struct input_uvc {
    int fd;
    char *videodevice;
} input_uvc;

/* private functions and variables to this plugin */
pthread_t cam;
pthread_mutex_t controls_mutex;
struct input_uvc *videoIn;
static globals *pglobal;

void *cam_thread( void *);
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
int input_init(input_parameter *param) 
{
  char *argv[MAX_ARGUMENTS]={NULL}, *dev = "/dev/video0";
  int argc=1;

  /* initialize the mutes variable */
  if( pthread_mutex_init(&controls_mutex, NULL) != 0 ) {
    IPRINT("could not initialize mutex variable\n");
    exit(EXIT_FAILURE);
  }

  /* keep a pointer to the global variables */
  pglobal = param->global;

  /* allocate webcam datastructure */
  videoIn = malloc(sizeof(struct input_uvc));
  if ( videoIn == NULL ) {
    IPRINT("not enough memory for videoIn\n");
    exit(EXIT_FAILURE);
  }
  memset(videoIn, 0, sizeof(struct input_uvc));

  /* display the parsed values */
  IPRINT("Using V4L2 device.: %s\n", dev);

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
  int i;
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

      default:
        DBG("default case\n");
        help();
        return 1;
    }
  }

    videoIn->videodevice = (char *) calloc (1, 16 * sizeof (char));
    snprintf (videoIn->videodevice, 12, "%s", dev);

    if ((videoIn->fd = open(videoIn->videodevice, O_RDWR)) == -1) {
      perror("ERROR opening V4L interface");
      return -1;
    }

    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(struct v4l2_capability));
    int ret = ioctl(videoIn->fd, VIDIOC_QUERYCAP, &cap);
    if (ret < 0) {
      fprintf(stderr, "Error opening device %s: unable to query device.\n", videoIn->videodevice);
      return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
      fprintf(stderr, "%s does not support streaming, cap=%x\n", videoIn->videodevice, cap.capabilities);
      return -1;
    }

    initDynCtrls(videoIn->fd);

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
#if 1

  pthread_create(&cam, 0, cam_thread, NULL);
  pthread_detach(cam);
#endif

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
  const int one_degree = ONE_DEGREE;

  /* certain commands do not need the mutex */
  if ( cmd != IN_CMD_RESET_PAN_TILT_NO_MUTEX )
    pthread_mutex_lock( &controls_mutex );

  switch (cmd) {
    case IN_CMD_HELLO:
      fprintf(stderr, "Hello from input plugin\n");
      break;

#if 0
    case IN_CMD_RESET:
      DBG("about to reset all image controls to defaults\n");
      res = v4l2ResetControl(videoIn, V4L2_CID_BRIGHTNESS);
      res |= v4l2ResetControl(videoIn, V4L2_CID_CONTRAST);
      res |= v4l2ResetControl(videoIn, V4L2_CID_SATURATION);
      res |= v4l2ResetControl(videoIn, V4L2_CID_GAIN);
      if ( res != 0 ) res = -1;
      break;
#endif

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

#if 0
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
    
    case IN_CMD_EXPOSURE_MANUAL:
      res = v4l2SetControl(videoIn, V4L2_CID_EXPOSURE_AUTO, 0);
      /*{ struct v4l2_control control;
      control.id    =V4L2_CID_EXPOSURE_AUTO_PRIORITY;
					control.value =0;
					if ((value = ioctl(videoIn->fd, VIDIOC_S_CTRL, &control)) < 0)
						printf("Set Auto Exposure off error\n");
					else
						printf("11Auto Exposure set to %d\n", control.value);

  }*/
    printf("uga manual: %d\n", res);
    break;
    
    case IN_CMD_EXPOSURE_AUTO:
      res = v4l2SetControl(videoIn, V4L2_CID_EXPOSURE_AUTO, 3);
      printf("uga auto: %d\n", res);
    /*  { struct v4l2_control control;
      control.id    =V4L2_CID_EXPOSURE_AUTO;
					control.value =1;
					if ((value = ioctl(videoIn->fd, VIDIOC_S_CTRL, &control)) < 0)
						printf("Set Auto Exposure on error\n");
					else
						printf("22Auto Exposure set to %d\n", control.value);

  }*/
    break;
    
    case IN_CMD_EXPOSURE_SHUTTER_PRIO:
      res = v4l2SetControl(videoIn, V4L2_CID_EXPOSURE_AUTO, 4);
    break;
    
    case IN_CMD_EXPOSURE_APERTURE_PRIO:
      res = v4l2SetControl(videoIn, V4L2_CID_EXPOSURE_AUTO, 8);
    break;
#endif

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
  fprintf(stderr, " ---------------------------------------------------------------\n" \
                  " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
                  " ---------------------------------------------------------------\n" \
                  " The following parameters can be passed to this plugin:\n\n" \
                  " [-d | --device ].......: video device to open (your camera)\n" );
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

#if 0
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
#endif
    sleep(1);
    pthread_cond_broadcast(&pglobal->db_update);
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
void cam_cleanup(void *arg) {
  static unsigned char first_run=1;

  if ( !first_run ) {
    DBG("already cleaned up ressources\n");
    return;
  }

  first_run = 0;
}




