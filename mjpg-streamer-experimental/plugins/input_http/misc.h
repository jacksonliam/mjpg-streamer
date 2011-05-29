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
#ifndef HTTP_UTILS
#define HTTP_UTILS

int min(int a, int b);
void write_image(char * image, int length);

// dumb 4 byte storing to detect double CRLF
int is_crlf(int bytes);

int is_crlfcrlf(int bytes) ;
void push_byte(int * bytes, char byte);

struct search_pattern {
    const char * string;
    const char * current_matched_char;
};

void search_pattern_reset(struct search_pattern * pattern);


int search_pattern_compare(struct search_pattern * pattern, char c) ;

int search_pattern_matches(struct search_pattern * pattern);



#endif
