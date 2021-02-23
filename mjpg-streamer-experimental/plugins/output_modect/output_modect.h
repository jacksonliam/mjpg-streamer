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

#ifndef OUTPUT_MODECT_H
#define OUTPUT_MODECT_H

#include "../../mjpg_streamer.h"

#define TRUE 1
#define FALSE 0
#define OUTPUT_PLUGIN_NAME "output_modect"
#define MAX_EVENT_TIME 300000
#define MODECT_DEBUG_DFLT FALSE
#define MODECT_JPG_SCALE_DFLT 3
#define MODECT_DETECT_DFLT TRUE
#define MODECT_PIXDIFF_DFLT 30
#define MODECT_ALARMPIX_DFLT 60
#define MODECT_LINGER_DFLT 3000
#define MODECT_EXEC_DFLT NULL
#define MODECT_SAVE_EVENTS_DFLT 0
#define MODECT_STORAGE_DFLT 0
#define MODECT_RECYCLE_DFLT 0
#define MODECT_DTSTAMP_DFLT 0

enum {   MODECT_FLG_NON_DYNAMIC,
         MODECT_FLG_DYNAMIC
};
enum {   MODECT_ID_DEBUG,
         MODECT_ID_DETECT,
         MODECT_ID_JPG_SCALE,
         MODECT_ID_PIXDIFF,
         MODECT_ID_ALARMPIX,
         MODECT_ID_LINGER,
         MODECT_ID_MOTION,
         MODECT_ID_SAVE_EVENTS,
         MODECT_ID_STORAGE,
         MODECT_ID_RECYCLE,
         MODECT_ID_DTSTAMP,
         MODECT_ID_STAT
};
#endif

