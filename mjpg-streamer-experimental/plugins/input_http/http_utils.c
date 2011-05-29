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
#include "http_utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int min(int a, int b) {
    if (a<b)
        return a;
    else
        return b;
}


void search_pattern_reset(struct search_pattern * pattern) {
    pattern->current_matched_char = pattern->string;
}

int search_pattern_compare(struct search_pattern * pattern, char c) {
    if (* (pattern->current_matched_char) == c)
    {   pattern->current_matched_char ++;
        return 1;
    }
    else {
        search_pattern_reset(pattern);
        return 0;
    }
}

int search_pattern_matches(struct search_pattern * pattern) {
    return  *(pattern->current_matched_char)==0;
}

