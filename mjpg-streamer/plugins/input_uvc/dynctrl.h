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
#include <string.h>

#ifdef USE_LIBV4L2
#include <libv4l2.h>
#define IOCTL_VIDEO(fd, req, value) v4l2_ioctl(fd, req, value)
#define OPEN_VIDEO(fd, flags) v4l2_open(fd, flags)
#define CLOSE_VIDEO(fd) v4l2_close(fd)
#else
#define IOCTL_VIDEO(fd, req, value) ioctl(fd, req, value)
#define OPEN_VIDEO(fd, flags) open(fd, flags)
#define CLOSE_VIDEO(fd) close(fd)
#endif


/*
 * Private V4L2 control identifiers from UVC driver.  - this seems to change acording to driver version
 * all other User-class control IDs are defined by V4L2 (videodev.h)
 */

/*------------------------- new camera class controls ---------------------*/
#define V4L2_CTRL_CLASS_USER_NEW		0x00980000
#define V4L2_CID_BASE_NEW			(V4L2_CTRL_CLASS_USER_NEW | 0x900)
#define V4L2_CID_POWER_LINE_FREQUENCY_NEW	(V4L2_CID_BASE_NEW+24)
#define V4L2_CID_HUE_AUTO_NEW			(V4L2_CID_BASE_NEW+25)
#define V4L2_CID_WHITE_BALANCE_TEMPERATURE_NEW	(V4L2_CID_BASE_NEW+26)
#define V4L2_CID_SHARPNESS_NEW			(V4L2_CID_BASE_NEW+27)
#define V4L2_CID_BACKLIGHT_COMPENSATION_NEW 	(V4L2_CID_BASE_NEW+28)
#define V4L2_CID_LAST_NEW			(V4L2_CID_BASE_NEW+31)

#define V4L2_CTRL_CLASS_CAMERA_NEW 0x009A0000	/* Camera class controls */
#define V4L2_CID_CAMERA_CLASS_BASE_NEW 		(V4L2_CTRL_CLASS_CAMERA_NEW | 0x900)

#define V4L2_CID_EXPOSURE_AUTO_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+1)
#define V4L2_CID_EXPOSURE_ABSOLUTE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+2)
#define V4L2_CID_EXPOSURE_AUTO_PRIORITY_NEW	(V4L2_CID_CAMERA_CLASS_BASE_NEW+3)

#define V4L2_CID_PAN_RELATIVE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+4)
#define V4L2_CID_TILT_RELATIVE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+5)
#define V4L2_CID_PAN_RESET_NEW			(V4L2_CID_CAMERA_CLASS_BASE_NEW+6)
#define V4L2_CID_TILT_RESET_NEW			(V4L2_CID_CAMERA_CLASS_BASE_NEW+7)

#define V4L2_CID_PAN_ABSOLUTE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+8)
#define V4L2_CID_TILT_ABSOLUTE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+9)

#define V4L2_CID_FOCUS_ABSOLUTE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+10)
#define V4L2_CID_FOCUS_RELATIVE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+11)
#define V4L2_CID_FOCUS_AUTO_NEW			(V4L2_CID_CAMERA_CLASS_BASE_NEW+12)
#define V4L2_CID_CAMERA_CLASS_LAST		(V4L2_CID_CAMERA_CLASS_BASE_NEW+13)

/*--------------- old private class controls ------------------------------*/

#define V4L2_CID_PRIVATE_BASE_OLD		0x08000000
#define V4L2_CID_BACKLIGHT_COMPENSATION_OLD	(V4L2_CID_PRIVATE_BASE_OLD+0)
#define V4L2_CID_POWER_LINE_FREQUENCY_OLD	(V4L2_CID_PRIVATE_BASE_OLD+1)
#define V4L2_CID_SHARPNESS_OLD			(V4L2_CID_PRIVATE_BASE_OLD+2)
#define V4L2_CID_HUE_AUTO_OLD			(V4L2_CID_PRIVATE_BASE_OLD+3)

#define V4L2_CID_FOCUS_AUTO_OLD			(V4L2_CID_PRIVATE_BASE_OLD+4)
#define V4L2_CID_FOCUS_ABSOLUTE_OLD		(V4L2_CID_PRIVATE_BASE_OLD+5)
#define V4L2_CID_FOCUS_RELATIVE_OLD		(V4L2_CID_PRIVATE_BASE_OLD+6)

#define V4L2_CID_PAN_RELATIVE_OLD		(V4L2_CID_PRIVATE_BASE_OLD+7)
#define V4L2_CID_TILT_RELATIVE_OLD		(V4L2_CID_PRIVATE_BASE_OLD+8)
#define V4L2_CID_PANTILT_RESET_OLD		(V4L2_CID_PRIVATE_BASE_OLD+9)

#define V4L2_CID_EXPOSURE_AUTO_OLD		(V4L2_CID_PRIVATE_BASE_OLD+10)
#define V4L2_CID_EXPOSURE_ABSOLUTE_OLD		(V4L2_CID_PRIVATE_BASE_OLD+11)

#define V4L2_CID_WHITE_BALANCE_TEMPERATURE_AUTO_OLD	(V4L2_CID_PRIVATE_BASE_OLD+12)
#define V4L2_CID_WHITE_BALANCE_TEMPERATURE_OLD		(V4L2_CID_PRIVATE_BASE_OLD+13)

#define V4L2_CID_PRIVATE_LAST			(V4L2_CID_WHITE_BALANCE_TEMPERATURE_OLD+1)

/*
 * Dynamic controls
 */

#define UVC_CTRL_DATA_TYPE_RAW		0
#define UVC_CTRL_DATA_TYPE_SIGNED	1
#define UVC_CTRL_DATA_TYPE_UNSIGNED	2
#define UVC_CTRL_DATA_TYPE_BOOLEAN	3
#define UVC_CTRL_DATA_TYPE_ENUM		4
#define UVC_CTRL_DATA_TYPE_BITMASK	5

#define V4L2_CID_BASE_EXTCTR				0x0A046D01
#define V4L2_CID_BASE_LOGITECH				V4L2_CID_BASE_EXTCTR
//#define V4L2_CID_PAN_RELATIVE_LOGITECH  		V4L2_CID_BASE_LOGITECH
//#define V4L2_CID_TILT_RELATIVE_LOGITECH 		V4L2_CID_BASE_LOGITECH+1
#define V4L2_CID_PANTILT_RESET_LOGITECH 		V4L2_CID_BASE_LOGITECH+2
#define V4L2_CID_FOCUS_LOGITECH         		V4L2_CID_BASE_LOGITECH+3
#define V4L2_CID_LED1_MODE_LOGITECH     		V4L2_CID_BASE_LOGITECH+4
#define V4L2_CID_LED1_FREQUENCY_LOGITECH 		V4L2_CID_BASE_LOGITECH+5
#define V4L2_CID_DISABLE_PROCESSING_LOGITECH 		V4L2_CID_BASE_LOGITECH+0x70
#define V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH 		V4L2_CID_BASE_LOGITECH+0x71
#define V4L2_CID_LAST_EXTCTR				V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH

#define UVC_GUID_LOGITECH_VIDEO_PIPE		{0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x50}
#define UVC_GUID_LOGITECH_MOTOR_CONTROL 	{0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x56}
#define UVC_GUID_LOGITECH_USER_HW_CONTROL 	{0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x1f}

#define XU_HW_CONTROL_LED1			1
#define XU_MOTORCONTROL_PANTILT_RELATIVE	1
#define XU_MOTORCONTROL_PANTILT_RESET		2
#define XU_MOTORCONTROL_FOCUS			3
#define XU_COLOR_PROCESSING_DISABLE		5
#define XU_RAW_DATA_BITS_PER_PIXEL		8

#define UVC_CONTROL_SET_CUR	(1 << 0)
#define UVC_CONTROL_GET_CUR	(1 << 1)
#define UVC_CONTROL_GET_MIN	(1 << 2)
#define UVC_CONTROL_GET_MAX	(1 << 3)
#define UVC_CONTROL_GET_RES	(1 << 4)
#define UVC_CONTROL_GET_DEF	(1 << 5)
/* Control should be saved at suspend and restored at resume. */
#define UVC_CONTROL_RESTORE	(1 << 6)
/* Control can be updated by the camera. */
#define UVC_CONTROL_AUTO_UPDATE	(1 << 7)

#define UVC_CONTROL_GET_RANGE   (UVC_CONTROL_GET_CUR | UVC_CONTROL_GET_MIN | \
                                 UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | \
                                 UVC_CONTROL_GET_DEF)


struct uvc_xu_control_info
{
	__u8 entity[16];
	__u8 index;
	__u8 selector;
	__u16 size;
	__u32 flags;
};

struct uvc_xu_control_mapping
{
	__u32 id;
	__u8 name[32];
	__u8 entity[16];
	__u8 selector;

	__u8 size;
	__u8 offset;
	enum v4l2_ctrl_type v4l2_type;
	__u32 data_type;
};

struct uvc_xu_control
{
	__u8 unit;
	__u8 selector;
	__u16 size;
	//__u8 __user *data;
	__u8 *data;
};

#define UVCIOC_CTRL_ADD		_IOW  ('U', 1, struct uvc_xu_control_info)
#define UVCIOC_CTRL_MAP		_IOWR ('U', 2, struct uvc_xu_control_mapping)
#define UVCIOC_CTRL_GET		_IOWR ('U', 3, struct uvc_xu_control)
#define UVCIOC_CTRL_SET		_IOW  ('U', 4, struct uvc_xu_control)

int initDynCtrls(int fd);

#endif
