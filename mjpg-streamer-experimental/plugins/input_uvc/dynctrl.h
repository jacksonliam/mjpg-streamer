/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#                                                                               #
# This program is free software; you can redistribute it and/or modify          #
# it under the terms of the GNU General Public License as published by          #
# the Free Software Foundation; either version 2 of the License, or             #
# (at your option) any later version.                                           #
#                                                                               #
# This program is distributed in the hope that it will be useful,               #
# but WITHOUT ANY WARRANTY; without even the implied warranty of                #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 #
# GNU General Public License for more details.                                  #
#                                                                               #
# You should have received a copy of the GNU General Public License             #
# along with this program; if not, write to the Free Software                   #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA     #
#                                                                               #
********************************************************************************/

#ifndef V4L2_DYNA_CTRLS_H
#define V4L2_DYNA_CTRLS_H

#include <linux/videodev2.h>
#include <linux/uvcvideo.h>
#include "v4l2uvc.h"
/*
 * Dynamic controls
 */

#define V4L2_CID_BASE_EXTCTR                0x0A046D01
#define V4L2_CID_BASE_LOGITECH              V4L2_CID_BASE_EXTCTR
//#define V4L2_CID_PAN_RELATIVE_LOGITECH        V4L2_CID_BASE_LOGITECH
//#define V4L2_CID_TILT_RELATIVE_LOGITECH       V4L2_CID_BASE_LOGITECH+1
#define V4L2_CID_PANTILT_RESET_LOGITECH         V4L2_CID_BASE_LOGITECH+2

/*this should realy be replaced by V4L2_CID_FOCUS_ABSOLUTE in libwebcam*/
#define V4L2_CID_FOCUS_LOGITECH             V4L2_CID_BASE_LOGITECH+3
#define V4L2_CID_LED1_MODE_LOGITECH         V4L2_CID_BASE_LOGITECH+4
#define V4L2_CID_LED1_FREQUENCY_LOGITECH        V4L2_CID_BASE_LOGITECH+5
#define V4L2_CID_DISABLE_PROCESSING_LOGITECH        V4L2_CID_BASE_LOGITECH+0x70
#define V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH        V4L2_CID_BASE_LOGITECH+0x71
#define V4L2_CID_LAST_EXTCTR                V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH

#define UVC_GUID_LOGITECH_VIDEO_PIPE        {0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x50}
#define UVC_GUID_LOGITECH_MOTOR_CONTROL     {0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x56}
#define UVC_GUID_LOGITECH_USER_HW_CONTROL   {0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x1f}

#define XU_HW_CONTROL_LED1          1
#define XU_MOTORCONTROL_PANTILT_RELATIVE    1
#define XU_MOTORCONTROL_PANTILT_RESET       2
#define XU_MOTORCONTROL_FOCUS           3
#define XU_COLOR_PROCESSING_DISABLE     5
#define XU_RAW_DATA_BITS_PER_PIXEL      8

int initDynCtrls(int fd);

#endif
