/*******************************************************************************
#                                                                              #
#      Copyright (C) 2011 Eugene Katsevman                                     #
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

#ifndef MJPG_PROXY_H
#define MJPG_PROXY_H

#include <stdbool.h>

#include "misc.h"


#ifndef DBG
#ifdef DEBUG
#define DBG(...) fprintf(stderr, " DBG(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__)
#else
#define DBG(...)
#endif
#endif

#define BUFFER_SIZE 1024 * 256
#define PATH_SIZE 256
#define BOUNDARY_SIZE 128
/* Added path to state because servers that host more than 1 stream have to have a path
    to identify each of the streams. 
    Added because MotionPlus has a stream for each camera.*/
struct extractor_state {
    char * port;
    char * hostname;
    char path[PATH_SIZE]; // the path part of the url for the sream we'll open

    // this is current result
    char buffer [BUFFER_SIZE];
    int length; // how many chars we've stuffed in the frame buffer

    // this is inner state of a parser

    int sockfd; // what we listen on
 //   int part;
   // int last_four_bytes;
    //struct search_pattern contentlength;
    //struct search_pattern boundary;

    int * should_stop;
    void (*on_image_received)(char * data, int length);

    int frameLength; // set when we get a new frame 
                        //- how we know when we;re done with a frame, length >= frameLength
    char boundary[BOUNDARY_SIZE]; // the name of the boundary tag that seperates frames
    bool collecting;  // set if we;re collecting messages for a frame
};

void init_mjpg_proxy(struct extractor_state  * state);

int parse_cmd_line(struct extractor_state * out_state, int argc, char * argv []);

void connect_and_stream(struct extractor_state * state);

void close_mjpg_proxy(struct extractor_state * state);

#endif
