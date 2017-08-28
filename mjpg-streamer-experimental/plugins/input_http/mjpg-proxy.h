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

#include "misc.h"


#ifndef DBG
#ifdef DEBUG
#define DBG(...) fprintf(stderr, " DBG(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__)
#else
#define DBG(...)
#endif
#endif

#define BUFFER_SIZE 1024 * 256

struct extractor_state {
    
    char * port;
    char * hostname;

    // this is current result
    char buffer [BUFFER_SIZE];
    int length;

    // this is inner state of a parser

    int sockfd;
    int part;
    int last_four_bytes;
    struct search_pattern contentlength;
    struct search_pattern boundary;

    int * should_stop;
    void (*on_image_received)(char * data, int length);
        
};

void init_mjpg_proxy(struct extractor_state  * state);

int parse_cmd_line(struct extractor_state * out_state, int argc, char * argv []);

void connect_and_stream(struct extractor_state * state);

void close_mjpg_proxy(struct extractor_state * state);

#endif
