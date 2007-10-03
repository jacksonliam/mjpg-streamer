/*******************************************************************************
# UVC streamer: Linuc-UVC streaming application                                #
# This package work with the Logitech UVC based webcams with the mjpeg feature #
#                                                                              #
# Copyright (C) 2005 2006 Laurent Pinchart &&  Michel Xhaard                   #
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/types.h>
#include <string.h>
#include <fcntl.h>
#include <wait.h>
#include <time.h>
#include <limits.h>

#include "utils.h"

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
void daemon_mode(void) {
  int fr=0;

  fr = fork();
  if( fr < 0 ) {
    fprintf(stderr, "fork() failed\n");
    exit(1);
  }
  if ( fr > 0 ) {
    exit(0);
  }

  if( setsid() < 0 ) {
    fprintf(stderr, "setsid() failed\n");
    exit(1);
  }

  fr = fork();
  if( fr < 0 ) {
    fprintf(stderr, "fork() failed\n");
    exit(1);
  }
  if ( fr > 0 ) {
    fprintf(stderr, "forked to background (%d)\n", fr);
    exit(0);
  }

  umask(0);

  chdir("/");
  close(0);
  close(1);
  close(2);

  open("/dev/null", O_RDWR);
  dup(0);
  dup(0);
}

