/*******************************************************************************
# UVC streamer: Linuc-UVC streaming application                                #
#This package work with the Logitech UVC based webcams with the mjpeg feature. #
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

#include "utils.h"
#include "color.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/types.h>
#include <string.h>
#include <fcntl.h>
#include <wait.h>
#include <time.h>
#include <limits.h>
#include "huffman.h"

int is_huffman(unsigned char *buf)
{
  unsigned char *ptbuf;
  int i = 0;
  ptbuf = buf;
  while (((ptbuf[0] << 8) | ptbuf[1]) != 0xffda) {
    if (i++ > 2048)
      return 0;
    if (((ptbuf[0] << 8) | ptbuf[1]) == 0xffc4)
      return 1;
    ptbuf++;
  }
  return 0;
}
/*
int print_picture(FILE* file, unsigned char *buf, int size)
{
  unsigned char *ptdeb, *ptcur = buf;
  int sizein;

  if (!is_huffman(buf)) {
    ptdeb = ptcur = buf;
    while (((ptcur[0] << 8) | ptcur[1]) != 0xffc0)
      ptcur++;
    sizein = ptcur - ptdeb;
    if( fwrite(buf, sizein, 1, file) <= 0) return -1;
    //printf();
    if( fwrite(dht_data, DHT_SIZE, 1, file) <= 0) return -1;
    if( fwrite(ptcur, size - sizein, 1, file) <= 0) return -1;
  } else {
    if( fwrite(ptcur, size, 1, file) <= 0) return -1;
  }

  return 0;
}*/

int print_picture(int fd, unsigned char *buf, int size)
{
    unsigned char *ptdeb, *ptcur = buf;
    int sizein;

    if (!is_huffman(buf)) {
        ptdeb = ptcur = buf;
        while (((ptcur[0] << 8) | ptcur[1]) != 0xffc0)
            ptcur++;
        sizein = ptcur - ptdeb;
        if( write(fd, buf, sizein) <= 0) return -1;
        if( write(fd, dht_data, DHT_SIZE) <= 0) return -1;
        if( write(fd, ptcur, size - sizein) <= 0) return -1;
    } else {
        if( write(fd, ptcur, size) <= 0) return -1;
    }

    return 0;
}
