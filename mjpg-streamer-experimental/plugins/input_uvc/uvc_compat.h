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

#ifndef _UVC_COMPAT_H
#define _UVC_COMPAT_H

#include <linux/version.h>
#ifndef __KERNEL__
#ifndef __user
#define __user
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
/*
 * Extended control API
 */
struct v4l2_ext_control {
    __u32 id;
    __u32 reserved2[2];
    union {
        __s32 value;
        __s64 value64;
        void *reserved;
    };
} __attribute__((packed));

struct v4l2_ext_controls {
    __u32 ctrl_class;
    __u32 count;
    __u32 error_idx;
    __u32 reserved[2];
    struct v4l2_ext_control *controls;
};

/* Values for ctrl_class field */
#define V4L2_CTRL_CLASS_USER        0x00980000  /* Old-style 'user' controls */
#define V4L2_CTRL_CLASS_MPEG        0x00990000  /* MPEG-compression controls */

#define V4L2_CTRL_ID_MASK       (0x0fffffff)
#define V4L2_CTRL_ID2CLASS(id)      ((id) & 0x0fff0000UL)
#define V4L2_CTRL_DRIVER_PRIV(id)   (((id) & 0xffff) >= 0x1000)
/* Control flags */
#define V4L2_CTRL_FLAG_READ_ONLY    0x0004
#define V4L2_CTRL_FLAG_UPDATE       0x0008
#define V4L2_CTRL_FLAG_INACTIVE     0x0010
#define V4L2_CTRL_FLAG_SLIDER       0x0020

/* Query flags, to be ORed with the control ID */
#define V4L2_CTRL_FLAG_NEXT_CTRL    0x80000000

/* User-class control IDs defined by V4L2 */
#undef  V4L2_CID_BASE
#define V4L2_CID_BASE           (V4L2_CTRL_CLASS_USER | 0x900)
#define V4L2_CID_USER_BASE      V4L2_CID_BASE
#define V4L2_CID_USER_CLASS     (V4L2_CTRL_CLASS_USER | 1)

#define VIDIOC_G_EXT_CTRLS      _IOWR ('V', 71, struct v4l2_ext_controls)
#define VIDIOC_S_EXT_CTRLS      _IOWR ('V', 72, struct v4l2_ext_controls)
#define VIDIOC_TRY_EXT_CTRLS        _IOWR ('V', 73, struct v4l2_ext_controls)

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
/*
 * Frame size and frame rate enumeration
 *
 * Included in Linux 2.6.19
 */
enum v4l2_frmsizetypes {
    V4L2_FRMSIZE_TYPE_DISCRETE  = 1,
    V4L2_FRMSIZE_TYPE_CONTINUOUS    = 2,
    V4L2_FRMSIZE_TYPE_STEPWISE  = 3
};

struct v4l2_frmsize_discrete {
    __u32           width;      /* Frame width [pixel] */
    __u32           height;     /* Frame height [pixel] */
};

struct v4l2_frmsize_stepwise {
    __u32           min_width;  /* Minimum frame width [pixel] */
    __u32           max_width;  /* Maximum frame width [pixel] */
    __u32           step_width; /* Frame width step size [pixel] */
    __u32           min_height; /* Minimum frame height [pixel] */
    __u32           max_height; /* Maximum frame height [pixel] */
    __u32           step_height;    /* Frame height step size [pixel] */
};

struct v4l2_frmsizeenum {
    __u32           index;      /* Frame size number */
    __u32           pixel_format;   /* Pixel format */
    __u32           type;       /* Frame size type the device supports. */

    union {                 /* Frame size */
        struct v4l2_frmsize_discrete    discrete;
        struct v4l2_frmsize_stepwise    stepwise;
    };

    __u32   reserved[2];            /* Reserved space for future use */
};

enum v4l2_frmivaltypes {
    V4L2_FRMIVAL_TYPE_DISCRETE  = 1,
    V4L2_FRMIVAL_TYPE_CONTINUOUS    = 2,
    V4L2_FRMIVAL_TYPE_STEPWISE  = 3
};

struct v4l2_frmival_stepwise {
    struct v4l2_fract   min;        /* Minimum frame interval [s] */
    struct v4l2_fract   max;        /* Maximum frame interval [s] */
    struct v4l2_fract   step;       /* Frame interval step size [s] */
};

struct v4l2_frmivalenum {
    __u32           index;      /* Frame format index */
    __u32           pixel_format;   /* Pixel format */
    __u32           width;      /* Frame width */
    __u32           height;     /* Frame height */
    __u32           type;       /* Frame interval type the device supports. */

    union {                 /* Frame interval */
        struct v4l2_fract       discrete;
        struct v4l2_frmival_stepwise    stepwise;
    };

    __u32   reserved[2];            /* Reserved space for future use */
};

#define VIDIOC_ENUM_FRAMESIZES      _IOWR ('V', 74, struct v4l2_frmsizeenum)
#define VIDIOC_ENUM_FRAMEINTERVALS  _IOWR ('V', 75, struct v4l2_frmivalenum)
#endif


#endif /* _UVC_COMPAT_H */

