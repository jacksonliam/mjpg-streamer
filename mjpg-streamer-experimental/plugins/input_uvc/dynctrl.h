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
#include "v4l2uvc.h"
/*
 * Dynamic controls
 */

#define UVC_CTRL_DATA_TYPE_RAW      0
#define UVC_CTRL_DATA_TYPE_SIGNED   1
#define UVC_CTRL_DATA_TYPE_UNSIGNED 2
#define UVC_CTRL_DATA_TYPE_BOOLEAN  3
#define UVC_CTRL_DATA_TYPE_ENUM     4
#define UVC_CTRL_DATA_TYPE_BITMASK  5

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

#define UVC_CONTROL_SET_CUR (1 << 0)
#define UVC_CONTROL_GET_CUR (1 << 1)
#define UVC_CONTROL_GET_MIN (1 << 2)
#define UVC_CONTROL_GET_MAX (1 << 3)
#define UVC_CONTROL_GET_RES (1 << 4)
#define UVC_CONTROL_GET_DEF (1 << 5)
/* Control should be saved at suspend and restored at resume. */
#define UVC_CONTROL_RESTORE (1 << 6)
/* Control can be updated by the camera. */
#define UVC_CONTROL_AUTO_UPDATE (1 << 7)

#define UVC_CONTROL_GET_RANGE   (UVC_CONTROL_GET_CUR | UVC_CONTROL_GET_MIN | \
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
    __u32 data_type;
};

struct uvc_xu_control {
    __u8 unit;
    __u8 selector;
    __u16 size;
    __u8 *data;
};

#define UVCIOC_CTRL_ADD     _IOW('U', 1, struct uvc_xu_control_info)
#define UVCIOC_CTRL_MAP     _IOWR('U', 2, struct uvc_xu_control_mapping)
#define UVCIOC_CTRL_GET     _IOWR('U', 3, struct uvc_xu_control)
#define UVCIOC_CTRL_SET     _IOW('U', 4, struct uvc_xu_control)

int initDynCtrls(int fd);

#endif
