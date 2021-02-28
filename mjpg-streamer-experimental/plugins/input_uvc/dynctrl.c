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


#include <sys/ioctl.h>
#include <sys/time.h>
#include <getopt.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>

#include <linux/videodev2.h>

#include "../../utils.h"
#include "dynctrl.h"

/* some Logitech webcams have pan/tilt/focus controls */
#define LENGTH_OF_XU_CTR (6)
#define LENGTH_OF_XU_MAP (10)

/* mapping for Pan/Tilt/Focus */
static struct uvc_xu_control_mapping xu_mappings[] = {
    {
        .id        = V4L2_CID_PAN_RELATIVE,
        .name      = "Pan (relative)",
        .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
        .selector  = XU_MOTORCONTROL_PANTILT_RELATIVE,
        .size      = 16,
        .offset    = 0,
        .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
        .data_type = UVC_CTRL_DATA_TYPE_SIGNED
    },
    {
        .id        = V4L2_CID_TILT_RELATIVE,
        .name      = "Tilt (relative)",
        .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
        .selector  = XU_MOTORCONTROL_PANTILT_RELATIVE,
        .size      = 16,
        .offset    = 16,
        .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
        .data_type = UVC_CTRL_DATA_TYPE_SIGNED
    },
    {
        .id        = V4L2_CID_PAN_RESET,
        .name      = "Pan Reset",
        .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
        .selector  = XU_MOTORCONTROL_PANTILT_RESET,
        .size      = 1,
        .offset    = 0,
        .v4l2_type = V4L2_CTRL_TYPE_BUTTON,
        .data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
    },
    {
        .id        = V4L2_CID_TILT_RESET,
        .name      = "Tilt Reset",
        .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
        .selector  = XU_MOTORCONTROL_PANTILT_RESET,
        .size      = 1,
        .offset    = 1,
        .v4l2_type = V4L2_CTRL_TYPE_BUTTON,
        .data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
    },
    {
        .id        = V4L2_CID_PANTILT_RESET_LOGITECH,
        .name      = "Pan/tilt Reset",
        .entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
        .selector  = XU_MOTORCONTROL_PANTILT_RESET,
        .size      = 8,
        .offset    = 0,
        .v4l2_type = V4L2_CTRL_TYPE_BUTTON,
        .data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
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
    },
    {
        .id        = V4L2_CID_DISABLE_PROCESSING_LOGITECH,
        .name      = "Disable video processing",
        .entity    = UVC_GUID_LOGITECH_VIDEO_PIPE,
        .selector  = XU_COLOR_PROCESSING_DISABLE,
        .size      = 8,
        .offset    = 0,
        .v4l2_type = V4L2_CTRL_TYPE_BOOLEAN,
        .data_type = UVC_CTRL_DATA_TYPE_BOOLEAN
    },
    {
        .id        = V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH,
        .name      = "Raw bits per pixel",
        .entity    = UVC_GUID_LOGITECH_VIDEO_PIPE,
        .selector  = XU_RAW_DATA_BITS_PER_PIXEL,
        .size      = 8,
        .offset    = 0,
        .v4l2_type = V4L2_CTRL_TYPE_INTEGER,
        .data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
    },

};

int initDynCtrls(int fd)
{
    int i = 0;
    int err = 0;
    /* after adding the controls, add the mapping now */
    for(i = 0; i < LENGTH_OF_XU_MAP; i++) {
        if((err = xioctl(fd, UVCIOC_CTRL_MAP, &xu_mappings[i])) < 0) {
            if(errno == EEXIST)
                fprintf(stderr,"Mapping exists\n");
            else if (errno != 0)
                fprintf(stderr, "UVCIOC_CTRL_MAP - Error at %s: %s (%d)\n", xu_mappings[i].name, strerror(errno), errno);
        }
    }
    return 0;
}
