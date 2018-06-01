/*******************************************************************************
# Linux-UVC streaming input-plugin for MJPG-streamer                           #
#                                                                              #
# This package work with the Logitech UVC based webcams with the mjpeg feature #
#                                                                              #
#   Orginally Copyright (C) 2005 2006 Laurent Pinchart &&  Michel Xhaard       #
#   Modifications Copyright (C) 2006  Gabriel A. Devenyi                       #
#   Modifications Copyright (C) 2007  Tom Stöveken                             #
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
#include <jpeglib.h>
#include <stdlib.h>

#include <linux/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>

#include "v4l2uvc.h"

/******************************************************************************
Description.: yuv2jpeg function is based on compress_yuyv_to_jpeg written by
              Gabriel A. Devenyi.
              modified to support other formats like RGB5:6:5 by Miklós Márton
              It uses the destination manager implemented above to compress
              YUYV data to JPEG. Most other implementations use the
              "jpeg_stdio_dest" from libjpeg, which can not store compressed
              pictures to memory instead of a file.
Input Value.: video structure from v4l2uvc.c/h, destination buffer and buffersize
              the buffer must be large enough, no error/size checking is done!
Return Value: the buffer will contain the compressed data
******************************************************************************/
int compress_image_to_jpeg(struct vdIn *vd, unsigned char *buffer, int size, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    unsigned char *line_buffer, *yuyv;
    int z;
    unsigned long written = size;

    line_buffer = calloc(vd->width * 3, 1);
    yuyv = vd->framebuffer;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &buffer, &written);

    cinfo.image_width = vd->width;
    cinfo.image_height = vd->height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    z = 0;
    if (vd->formatIn == V4L2_PIX_FMT_YUYV) {
        while(cinfo.next_scanline < vd->height) {
            int x;
            unsigned char *ptr = line_buffer;


            for(x = 0; x < vd->width; x++) {
                int r, g, b;
                int y, u, v;

                if(!z)
                    y = yuyv[0] << 8;
                else
                    y = yuyv[2] << 8;
                u = yuyv[1] - 128;
                v = yuyv[3] - 128;

                r = (y + (359 * v)) >> 8;
                g = (y - (88 * u) - (183 * v)) >> 8;
                b = (y + (454 * u)) >> 8;

                *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
                *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
                *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);

                if(z++) {
                    z = 0;
                    yuyv += 4;
                }
            }

            row_pointer[0] = line_buffer;
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }
    } else if (vd->formatIn == V4L2_PIX_FMT_RGB565) {
        while(cinfo.next_scanline < vd->height) {
            int x;
            unsigned char *ptr = line_buffer;

            for(x = 0; x < vd->width; x++) {
                /*
                unsigned int tb = ((unsigned char)raw[i+1] << 8) + (unsigned char)raw[i];
                r =  ((unsigned char)(raw[i+1]) & 248);
                g = (unsigned char)(( tb & 2016) >> 3);
                b =  ((unsigned char)raw[i] & 31) * 8;
                */
                unsigned int twoByte = (yuyv[1] << 8) + yuyv[0];
                *(ptr++) = (yuyv[1] & 248);
                *(ptr++) = (unsigned char)((twoByte & 2016) >> 3);
                *(ptr++) = ((yuyv[0] & 31) * 8);
                yuyv += 2;
            }

            row_pointer[0] = line_buffer;
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }
    }  else if (vd->formatIn == V4L2_PIX_FMT_UYVY) {
        while(cinfo.next_scanline < vd->height) {
            int x;
            unsigned char *ptr = line_buffer;


            for(x = 0; x < vd->width; x++) {
                int r, g, b;
                int y, u, v;

                if(!z)
                    y = yuyv[1] << 8;
                else
                    y = yuyv[3] << 8;
                u = yuyv[0] - 128;
                v = yuyv[2] - 128;

                r = (y + (359 * v)) >> 8;
                g = (y - (88 * u) - (183 * v)) >> 8;
                b = (y + (454 * u)) >> 8;

                *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
                *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
                *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);

                if(z++) {
                    z = 0;
                    yuyv += 4;
                }
            }

            row_pointer[0] = line_buffer;
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }
    } else if (vd->formatIn == V4L2_PIX_FMT_RGB24) {
        jpeg_write_scanlines(&cinfo, (JSAMPROW*)vd->framebuffer, vd->height);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    free(line_buffer);

    return (written);
}
