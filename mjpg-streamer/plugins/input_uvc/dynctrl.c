/*******************************************************************************
# Linux-UVC streaming input-plugin for MJPG-streamer                           #
#                                                                              #
# This package work with the Logitech UVC based webcams with the mjpeg feature #
#                                                                              #
# Copyright (C)      2007 Tom Stöveken                                         #
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

#include <sys/ioctl.h>
#include <sys/time.h>
#include <getopt.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>

#include <linux/videodev2.h>

#include "../../utils.h"
#include "../../mjpg_streamer.h"
#include "uvcvideo.h"
#include "dynctrl.h"

/* some Logitech webcams have pan/tilt/focus controls */
static struct uvc_xu_control_info xu_ctrls[] = {
  {
    .entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector = XU_MOTORCONTROL_PANTILT_RELATIVE,
    .index    = 0,
    .size     = 4,
    .flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_DEF
  },
  {
    .entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector = XU_MOTORCONTROL_PANTILT_RESET,
    .index    = 1,
    .size     = 1,
    .flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF
  },
  {
    .entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector = XU_MOTORCONTROL_FOCUS,
    .index    = 2,
    .size     = 6,
    .flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_DEF
  },
  {
    .entity   = UVC_GUID_LOGITECH_USER_HW_CONTROL,
    .selector = XU_HW_CONTROL_LED1,
    .index    = 0,
    .size     = 3,
    .flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF
  }
};

/* mapping for Pan/Tilt/Focus */
static struct uvc_xu_control_mapping xu_mappings[] = {
  {
    .id        = V4L2_CID_PAN_RELATIVE_LOGITECH,
    .name      = "Pan (relative)",
    .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector  = XU_MOTORCONTROL_PANTILT_RELATIVE,
    .size      = 16,
    .offset    = 0,
    .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
    .data_type = UVC_CTRL_DATA_TYPE_SIGNED
  },
  {
    .id        = V4L2_CID_TILT_RELATIVE_LOGITECH,
    .name      = "Tilt (relative)",
    .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector  = XU_MOTORCONTROL_PANTILT_RELATIVE,
    .size      = 16,
    .offset    = 16,
    .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
    .data_type = UVC_CTRL_DATA_TYPE_SIGNED
  },
  {
    .id        = V4L2_CID_PANTILT_RESET_LOGITECH,
    .name      = "Pan/Tilt (reset)",
    .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector  = XU_MOTORCONTROL_PANTILT_RESET,
    .size      = 2,
    .offset    = 0,
    .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
    .data_type = UVC_CTRL_DATA_TYPE_ENUM
  },
  {
    .id        = V4L2_CID_FOCUS_LOGITECH,
    .name      = "Focus (absolute)",
    .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .selector  = XU_MOTORCONTROL_FOCUS,
    .size      = 8,
    .offset    = 0,
    .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
    .data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
  },
  {
    .id        = V4L2_CID_LED1_MODE_LOGITECH,
    .name      = "LED1 Mode",
    .entity    = UVC_GUID_LOGITECH_USER_HW_CONTROL,
    .selector  = XU_HW_CONTROL_LED1,
    .size      = 8,
    .offset    = 0,
    .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
    .data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
  },
  {
    .id        = V4L2_CID_LED1_FREQUENCY_LOGITECH,
    .name      = "LED1 Frequency",
    .entity    = UVC_GUID_LOGITECH_USER_HW_CONTROL,
    .selector  = XU_HW_CONTROL_LED1,
    .size      = 8,
    .offset    = 16,
    .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
    .data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
  }
};

void initDynCtrls(int dev) {
  int i, err;

  /* try to add all controls listed above */
  for ( i=0; i<LENGTH_OF(xu_ctrls); i++ ) {
    DBG("adding control for %d\n", i);
    errno=0;
    if ( (err=ioctl(dev, UVCIOC_CTRL_ADD, &xu_ctrls[i])) < 0 ) {
      if ( errno != EEXIST ) {
        DBG("uvcioc ctrl add error: errno=%d (retval=%d)\n", errno, err);
      } else {
        DBG("control %d already exists\n", i);
      }
    }
  }

  /* after adding the controls, add the mapping now */
  for ( i=0; i<LENGTH_OF(xu_mappings); i++ ) {
    DBG("mapping controls for %s\n", xu_mappings[i].name);
    errno=0;
    if ((err=ioctl(dev, UVCIOC_CTRL_MAP, &xu_mappings[i])) < 0) {
      if (errno != EEXIST) {
        DBG("uvcioc ctrl map error: errno=%d (retval=%d)\n", errno, err);
      } else {
        DBG("mapping %d already exists\n", i);
      }
    }
  }
}

/*
SRC: https://lists.berlios.de/pipermail/linux-uvc-devel/2007-July/001888.html

- dev: the device file descriptor
- pan: pan angle in 1/64th of degree
- tilt: tilt angle in 1/64th of degree
- reset: set to 1 to reset pan/tilt to the device origin, set to 0 otherwise
*/
int uvcPanTilt(int dev, int pan, int tilt, int reset) {
  struct v4l2_ext_control xctrls[2];
  struct v4l2_ext_controls ctrls;

  if (reset) {
    xctrls[0].id = V4L2_CID_PANTILT_RESET_LOGITECH;
    xctrls[0].value = 3;

    ctrls.count = 1;
    ctrls.controls = xctrls;
  } else {
    xctrls[0].id = V4L2_CID_PAN_RELATIVE_LOGITECH;
    xctrls[0].value = pan;
    xctrls[1].id = V4L2_CID_TILT_RELATIVE_LOGITECH;
    xctrls[1].value = tilt;

    ctrls.count = 2;
    ctrls.controls = xctrls;
  }

  if ( ioctl(dev, VIDIOC_S_EXT_CTRLS, &ctrls) < 0 ) {
    return -1;
  }

  return 0;
}
