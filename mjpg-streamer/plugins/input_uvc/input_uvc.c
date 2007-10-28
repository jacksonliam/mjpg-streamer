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

#define INPUT_PLUGIN_NAME "UVC webcam M-JPEG grabber"
#define MAX_ARGUMENTS 32

/* private functions and variables to this plugin */
pthread_t   cam;
pthread_mutex_t controls_mutex;
struct vdIn *videoIn;
static globals *pglobal;

void *cam_thread( void *);
void cam_cleanup(void *);
void help(void);

/*** plugin interface functions ***/
/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
int input_init(input_parameter *param) {
  char *argv[MAX_ARGUMENTS]={NULL}, *dev = "/dev/video0";
  int argc=1, width=640, height=480, fps=5, i;

  if( pthread_mutex_init(&controls_mutex, NULL) != 0 ) {
    fprintf(stderr, "could not initialize mutex variable\n");
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
        /*
         * interesting resolutions, others are mentioned at:
         * http://www.quickcamteam.net/hcl/frame-format-matrix/
         */
        if ( strcmp("960x720", optarg) == 0 ) { width=960; height=720; }
        else if ( strcmp("640x480", optarg) == 0 ) { width=640; height=480; }
        else if ( strcmp("320x240", optarg) == 0 ) { width=320; height=240; }
        else if ( strcmp("160x120", optarg) == 0 ) { width=160; height=120; }
        else fprintf(stderr, "ignoring unsupported resolution\n");
        break;

      /* f, fps */
      case 6:
      case 7:
        DBG("case 6,7\n");
        fps=atoi(optarg);
        break;

      default:
        DBG("default case\n");
        help();
        return 1;
    }
  }

  pglobal = param->global;

  /* allocate webcam datastructure */
  videoIn = malloc(sizeof(struct vdIn));

  IPRINT("Using V4L2 device.: %s\n", dev);
  IPRINT("Resolution........: %i x %i\n", width, height);
  IPRINT("frames per second.: %i\n", fps);

  /* open video device and prepare data structure */
  if (init_videoIn(videoIn, dev, width, height, fps, V4L2_PIX_FMT_MJPEG, 1) < 0) {
    IPRINT("init_VideoIn failed\n");
    closelog();
    exit(EXIT_FAILURE);
  }

  return 0;
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
int input_stop(void) {
  DBG("will cancel input thread\n");
  pthread_cancel(cam);

  return 0;
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
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
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
int input_cmd(in_cmd_type cmd) {
  int res=0;
  static int pan=0;
  static int tilt=0;
  static int pan_tilt_valid=-1;
  const int one_degree = ONE_DEGREE;

  pthread_mutex_lock( &controls_mutex );

  DBG("pan: %d, tilt: %d, valid: %d\n", pan, tilt, pan_tilt_valid);

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
      DBG("about to set all pan/tilt to default position\n");
      if ( uvcPanTilt(videoIn, 0, 0, 3) != 0 ) {
        res = -1;
        break;
      }
      pan_tilt_valid = 1;
      break;

    case IN_CMD_PAN_PLUS:
      DBG("pan +\n");
      if ( pan_tilt_valid != 1 ) {
        if ( uvcPanTilt(videoIn, 0, 0, 3) != 0 ) {
          res = -1;
          break;
        }
        pan_tilt_valid = 1;
      }
      pan = ( (MAX_PAN) < (pan+one_degree) ) ? (MAX_PAN) : (pan+one_degree);
      res = uvcPanTilt(videoIn, pan, tilt, 0);
      break;

    case IN_CMD_PAN_MINUS:
      DBG("pan -\n");
      if ( pan_tilt_valid != 1 ) {
        if ( uvcPanTilt(videoIn, 0, 0, 3) != 0 ) {
          res = -1;
          break;
        }
        pan_tilt_valid = 1;
      }
      pan = ( (MIN_PAN) > (pan+one_degree) )? (MIN_PAN) : (pan+one_degree);
      res = uvcPanTilt(videoIn, pan, tilt, 0);
      break;

    case IN_CMD_TILT_PLUS:
      DBG("tilt +\n");
      if ( pan_tilt_valid != 1 ) {
        if ( uvcPanTilt(videoIn, 0, 0, 3) != 0 ) {
          res = -1;
          break;
        }
        pan_tilt_valid = 1;
      }
      tilt = ( (MAX_TILT) < (tilt+one_degree) )? (MAX_TILT) : (tilt+one_degree);
      res = uvcPanTilt(videoIn, pan, tilt, 0);
      break;

    case IN_CMD_TILT_MINUS:
      DBG("tilt -\n");
      if ( pan_tilt_valid != 1 ) {
        if ( uvcPanTilt(videoIn, 0, 0, 3) != 0 ) {
          res = -1;
          break;
        }
        pan_tilt_valid = 1;
      }
      tilt = ( (MIN_TILT) > (tilt+one_degree) )? (MIN_TILT) : (tilt+one_degree);
      res = uvcPanTilt(videoIn, pan, tilt, 0);
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

    default:
      DBG("nothing matched\n");
      res = -1;
  }

  pthread_mutex_unlock( &controls_mutex );
  return res;
}

/*** private functions for this plugin below ***/
/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
void help(void) {
    fprintf(stderr, " ---------------------------------------------------------------\n" \
                    " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
                    " ---------------------------------------------------------------\n" \
                    " The following parameters can be passed to this plugin:\n\n" \
                    " [-d | --device ].......: video device to open (your camera)\n" \
                    " [-r | --resolution ]...: 960x720, 640x480, 320x240, 160x120\n" \
                    " [-f | --fps ]..........: frames per second\n" \
                    " ---------------------------------------------------------------\n");
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
int is_huffman(unsigned char *buf)
{
    unsigned char *ptbuf;
    int i = 0;
    ptbuf = buf;
    while (((ptbuf[0] << 8) | ptbuf[1]) != 0xffda) {
        if (i++ > 2048)
            return 0;
        if (((ptbuf[0] << 8) | ptbuf[1]) == 0xffc4)
            return 1;
        ptbuf++;
    }
    return 0;
}

#if 0
/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
int print_picture(int fd, unsigned char *buf, int size)
{
    unsigned char *ptdeb, *ptcur = buf;
    int sizein;

    if (!is_huffman(buf)) {
        ptdeb = ptcur = buf;
        while (((ptcur[0] << 8) | ptcur[1]) != 0xffc0)
            ptcur++;
        sizein = ptcur - ptdeb;
        if( write(fd, buf, sizein) <= 0) return -1;
        if( write(fd, dht_data, DHT_SIZE) <= 0) return -1;
        if( write(fd, ptcur, size - sizein) <= 0) return -1;
    } else {
        if( write(fd, ptcur, size) <= 0) return -1;
    }
    return 0;
}
#endif

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
int memcpy_picture(unsigned char *out, unsigned char *buf, int size)
{
    unsigned char *ptdeb, *ptcur = buf;
    int sizein, pos=0;

    if (!is_huffman(buf)) {
        ptdeb = ptcur = buf;
        while (((ptcur[0] << 8) | ptcur[1]) != 0xffc0)
            ptcur++;
        sizein = ptcur - ptdeb;

        memcpy(out+pos, buf, sizein); pos += sizein;
        memcpy(out+pos, dht_data, DHT_SIZE); pos += DHT_SIZE;
        memcpy(out+pos, ptcur, size - sizein); pos += size-sizein;
    } else {
        memcpy(out+pos, ptcur, size); pos += size;
    }
    return pos;
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
/* the single writer thread */
void *cam_thread( void *arg ) {
  /* set cleanup handler to cleanup allocated ressources */
  pthread_cleanup_push(cam_cleanup, NULL);

  while( !pglobal->stop ) {

    /* grab a frame */
    if( uvcGrab(videoIn) < 0 ) {
      fprintf(stderr, "Error grabbing frames\n");
      exit(1);
    }

    /* copy JPG picture to global buffer */
    pthread_mutex_lock( &pglobal->db );
    pglobal->size = memcpy_picture(pglobal->buf, videoIn->tmpbuffer, videoIn->buf.bytesused);

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
  DBG("cleaning up ressources allocated by input thread\n");

  close_v4l2(videoIn);
  if (videoIn->tmpbuffer != NULL) free(videoIn->tmpbuffer);
  if (videoIn != NULL) free(videoIn);
  if (pglobal->buf != NULL) free(pglobal->buf);
}




