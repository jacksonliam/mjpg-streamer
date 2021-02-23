/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#      output_modect Copyright (C) 2019 Jim Allingham                          #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
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
#include "../../mjpg_streamer.h"
#include "output_modect.h"

struct _control omparms[] =
{
   {
      .ctrl.id = MODECT_ID_DEBUG, 
      .ctrl.type = V4L2_CTRL_TYPE_BOOLEAN, 
      .ctrl.name = "debug",
      .ctrl.minimum = 0,
      .ctrl.maximum = 1,
      .ctrl.step = 0,
      .ctrl.default_value = MODECT_DEBUG_DFLT,
      .ctrl.flags = MODECT_FLG_DYNAMIC,
      .ctrl.reserved = {0,0},
      .value = MODECT_DEBUG_DFLT, 
      .menuitems = NULL,
      .class_id = 0,
      .group = 1   },
   {
      .ctrl.id = MODECT_ID_DETECT, 
      .ctrl.type = V4L2_CTRL_TYPE_BOOLEAN, 
      .ctrl.name = "modect",
      .ctrl.minimum = 0,
      .ctrl.maximum = 1,
      .ctrl.step = 0,
      .ctrl.default_value = MODECT_DETECT_DFLT,
      .ctrl.flags = MODECT_FLG_DYNAMIC,
      .ctrl.reserved = {0,0},
      .value = MODECT_DETECT_DFLT, 
      .menuitems = NULL,
      .class_id = 0,
      .group = 1
   },
   {
      .ctrl.id = MODECT_ID_JPG_SCALE, 
      .ctrl.type = V4L2_CTRL_TYPE_BOOLEAN, 
      .ctrl.name = "jpeg_scale",
      .ctrl.minimum = 0,
      .ctrl.maximum = 32,
      .ctrl.step = 0,
      .ctrl.default_value = MODECT_JPG_SCALE_DFLT,
      .ctrl.flags = MODECT_FLG_DYNAMIC,
      .ctrl.reserved = {0,0},
      .value = MODECT_JPG_SCALE_DFLT,
      .menuitems = NULL,
      .class_id = 0,
      .group = 1
   },
   {
      .ctrl.id = MODECT_ID_PIXDIFF, 
      .ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
      .ctrl.name = "pix_threshold",
      .ctrl.minimum = 0,
      .ctrl.maximum = 511,
      .ctrl.step = 0,
      .ctrl.default_value = MODECT_PIXDIFF_DFLT,
      .ctrl.flags = MODECT_FLG_DYNAMIC,
      .ctrl.reserved = {0,0},
      .value = MODECT_PIXDIFF_DFLT, 
      .menuitems = NULL,
      .class_id = 0,
      .group = 1
   },
   {
      .ctrl.id = MODECT_ID_ALARMPIX, 
      .ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
      .ctrl.name = "alarm_pixels",
      .ctrl.minimum = 0,
      .ctrl.maximum = 1000000,
      .ctrl.step = 0,
      .ctrl.default_value = MODECT_ALARMPIX_DFLT,
      .ctrl.flags = MODECT_FLG_DYNAMIC,
      .ctrl.reserved = {0,0},
      .value = MODECT_ALARMPIX_DFLT, 
      .menuitems = NULL,
      .class_id = 0,
      .group = 1
   },
   {
      .ctrl.id = MODECT_ID_LINGER, 
      .ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
      .ctrl.name = "linger",
      .ctrl.minimum = 0,
      .ctrl.maximum = 100000,
      .ctrl.step = 0,
      .ctrl.default_value = MODECT_LINGER_DFLT,
      .ctrl.flags = MODECT_FLG_DYNAMIC,
      .ctrl.reserved = {0,0},
      .value = MODECT_LINGER_DFLT, 
      .menuitems = NULL,
      .class_id = 0,
      .group = 1
   },
   {
      .ctrl.id = MODECT_ID_MOTION, 
      .ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
      .ctrl.name = "motion",
      .ctrl.minimum = 0,
      .ctrl.maximum = 1,
      .ctrl.step = 0,
      .ctrl.default_value = 0,
      .ctrl.flags = MODECT_FLG_DYNAMIC,
      .ctrl.reserved = {0,0},
      .value = 0, 
      .menuitems = NULL,
      .class_id = 0,
      .group = 1
   },
   {
      .ctrl.id = MODECT_ID_SAVE_EVENTS, 
      .ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
      .ctrl.name = "save",
      .ctrl.minimum = 0,
      .ctrl.maximum = 1,
      .ctrl.step = 0,
      .ctrl.default_value = MODECT_SAVE_EVENTS_DFLT,
      .ctrl.flags = MODECT_FLG_DYNAMIC,
      .ctrl.reserved = {0,0},
      .value = MODECT_SAVE_EVENTS_DFLT, 
      .menuitems = NULL,
      .class_id = 0,
      .group = 1
   },
   {
      .ctrl.id = MODECT_ID_STORAGE, 
      .ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
      .ctrl.name = "storage",
      .ctrl.minimum = 0,
      .ctrl.maximum = 0x7fffffff,
      .ctrl.step = 0,
      .ctrl.default_value = MODECT_STORAGE_DFLT,
      .ctrl.flags = MODECT_FLG_DYNAMIC,
      .ctrl.reserved = {0,0},
      .value = MODECT_STORAGE_DFLT, 
      .menuitems = NULL,
      .class_id = 0,
      .group = 1
   },
   {
      .ctrl.id = MODECT_ID_RECYCLE, 
      .ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
      .ctrl.name = "recycle",
      .ctrl.minimum = 0,
      .ctrl.maximum = 100,
      .ctrl.step = 0,
      .ctrl.default_value = MODECT_RECYCLE_DFLT,
      .ctrl.flags = MODECT_FLG_DYNAMIC,
      .ctrl.reserved = {0,0},
      .value = MODECT_RECYCLE_DFLT, 
      .menuitems = NULL,
      .class_id = 0,
      .group = 1
   },
   {
      .ctrl.id = MODECT_ID_DTSTAMP, 
      .ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
      .ctrl.name = "timestamp",
      .ctrl.minimum = 0,
      .ctrl.maximum = 4,
      .ctrl.step = 0,
      .ctrl.default_value = MODECT_DTSTAMP_DFLT,
      .ctrl.flags = MODECT_FLG_DYNAMIC,
      .ctrl.reserved = {0,0},
      .value = MODECT_DTSTAMP_DFLT, 
      .menuitems = NULL,
      .class_id = 0,
      .group = 1
   },
   {
      .ctrl.id = MODECT_ID_STAT, 
      .ctrl.type = V4L2_CTRL_TYPE_INTEGER, 
      .ctrl.name = "stat",
      .ctrl.minimum = 0,
      .ctrl.maximum = 1,
      .ctrl.step = 0,
      .ctrl.default_value = 0,
      .ctrl.flags = MODECT_FLG_DYNAMIC,
      .ctrl.reserved = {0,0},
      .value = 0, 
      .menuitems = NULL,
      .class_id = 0,
      .group = 1
   }
};
