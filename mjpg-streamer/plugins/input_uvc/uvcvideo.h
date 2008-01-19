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
#ifndef _USB_VIDEO_H_
#define _USB_VIDEO_H_

#include <linux/kernel.h>
#include <linux/videodev.h>

/* Compatibility */
#include "uvc_compat.h"

/*
 * Private V4L2 control identifiers.
 */

#define V4L2_CID_BACKLIGHT_COMPENSATION		(V4L2_CID_PRIVATE_BASE+0)
#define V4L2_CID_POWER_LINE_FREQUENCY		(V4L2_CID_PRIVATE_BASE+1)
#define V4L2_CID_SHARPNESS			(V4L2_CID_PRIVATE_BASE+2)
#define V4L2_CID_HUE_AUTO			(V4L2_CID_PRIVATE_BASE+3)

#define V4L2_CID_FOCUS_AUTO			(V4L2_CID_PRIVATE_BASE+4)
#define V4L2_CID_FOCUS_ABSOLUTE			(V4L2_CID_PRIVATE_BASE+5)
#define V4L2_CID_FOCUS_RELATIVE			(V4L2_CID_PRIVATE_BASE+6)

#define V4L2_CID_PAN_RELATIVE			(V4L2_CID_PRIVATE_BASE+7)
#define V4L2_CID_TILT_RELATIVE			(V4L2_CID_PRIVATE_BASE+8)
#define V4L2_CID_PANTILT_RESET			(V4L2_CID_PRIVATE_BASE+9)

#define V4L2_CID_EXPOSURE_AUTO			(V4L2_CID_PRIVATE_BASE+10)
#define V4L2_CID_EXPOSURE_ABSOLUTE		(V4L2_CID_PRIVATE_BASE+11)
#define V4L2_CID_EXPOSURE_AUTO_PRIORITY		(V4L2_CID_PRIVATE_BASE+14)

#define V4L2_CID_WHITE_BALANCE_TEMPERATURE_AUTO	(V4L2_CID_PRIVATE_BASE+12)
#define V4L2_CID_WHITE_BALANCE_TEMPERATURE	(V4L2_CID_PRIVATE_BASE+13)

#define V4L2_CID_PRIVATE_LAST			V4L2_CID_EXPOSURE_AUTO_PRIORITY

/*
 * Dynamic controls
 */
/* Data types for UVC control data */
enum uvc_control_data_type {
        UVC_CTRL_DATA_TYPE_RAW = 0,
        UVC_CTRL_DATA_TYPE_SIGNED,
        UVC_CTRL_DATA_TYPE_UNSIGNED,
        UVC_CTRL_DATA_TYPE_BOOLEAN,
        UVC_CTRL_DATA_TYPE_ENUM,
	UVC_CTRL_DATA_TYPE_BITMASK,
};

#define UVC_CONTROL_SET_CUR	(1 << 0)
#define UVC_CONTROL_GET_CUR	(1 << 1)
#define UVC_CONTROL_GET_MIN	(1 << 2)
#define UVC_CONTROL_GET_MAX	(1 << 3)
#define UVC_CONTROL_GET_RES	(1 << 4)
#define UVC_CONTROL_GET_DEF	(1 << 5)
/* Control should be saved at suspend and restored at resume. */
#define UVC_CONTROL_RESTORE	(1 << 6)

#define UVC_CONTROL_GET_RANGE	(UVC_CONTROL_GET_CUR | UVC_CONTROL_GET_MIN | \
				 UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | \
				 UVC_CONTROL_GET_DEF)

struct uvc_xu_control_info {
	__u8 entity[16];
	__u8 index;
	__u8 selector;
	__u16 size;
	__u32 flags;
};

struct uvc_xu_control_mapping {
	__u32 id;
	__u8 name[32];
	__u8 entity[16];
	__u8 selector;

	__u8 size;
	__u8 offset;
	enum v4l2_ctrl_type v4l2_type;
	enum uvc_control_data_type data_type;
};

struct uvc_xu_control {
	__u8 unit;
	__u8 selector;
	__u16 size;
	__u8 __user *data;
};

#define UVCIOC_CTRL_ADD		_IOW  ('U', 1, struct uvc_xu_control_info)
#define UVCIOC_CTRL_MAP		_IOWR ('U', 2, struct uvc_xu_control_mapping)
#define UVCIOC_CTRL_GET		_IOWR ('U', 3, struct uvc_xu_control)
#define UVCIOC_CTRL_SET		_IOW  ('U', 4, struct uvc_xu_control)



#endif

