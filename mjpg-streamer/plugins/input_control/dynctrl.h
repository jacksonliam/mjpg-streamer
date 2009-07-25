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

#include "uvcvideo.h"

#define V4L2_CID_PAN_RELATIVE_LOGITECH   0x0A046D01
#define V4L2_CID_TILT_RELATIVE_LOGITECH  0x0A046D02
#define V4L2_CID_PANTILT_RESET_LOGITECH  0x0A046D03
#define V4L2_CID_FOCUS_LOGITECH          0x0A046D04
#define V4L2_CID_LED1_MODE_LOGITECH      0x0A046D05
#define V4L2_CID_LED1_FREQUENCY_LOGITECH 0x0A046D06

#define UVC_GUID_LOGITECH_MOTOR_CONTROL {0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x56}
#define UVC_GUID_LOGITECH_USER_HW_CONTROL {0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x1f}

#define XU_HW_CONTROL_LED1               1
#define XU_MOTORCONTROL_PANTILT_RELATIVE 1
#define XU_MOTORCONTROL_PANTILT_RESET    2
#define XU_MOTORCONTROL_FOCUS            3

#define ONE_DEGREE (64);
#define MAX_PAN  (70*64)
#define MIN_PAN  (-70*64)
#define MAX_TILT (30*64)
#define MIN_TILT (-30*64)
#define MIN_RES  (64*5)

void initDynCtrls(int dev);
int uvcPanTilt(int dev, int pan, int tilt, int reset);
