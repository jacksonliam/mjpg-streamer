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

#include "jpeglib.h"
#include "jerror.h"

#include <linux/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>

#include "v4l2uvc.h"

#define OUTPUT_BUF_SIZE  4096

typedef struct {
    struct jpeg_destination_mgr pub; /* public fields */

    JOCTET * buffer;    /* start of buffer */

    unsigned char *outbuffer;
    int outbuffer_size;
    unsigned char *outbuffer_cursor;
    int *written;

} mjpg_destination_mgr;

typedef mjpg_destination_mgr * mjpg_dest_ptr;

/******************************************************************************
Description.:
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(void) init_destination(j_compress_ptr cinfo)
{
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;

    /* Allocate the output buffer --- it will be released when done with image */
    dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_IMAGE, OUTPUT_BUF_SIZE * sizeof(JOCTET));

    *(dest->written) = 0;

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

/******************************************************************************
Description.: called whenever local jpeg buffer fills up
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(boolean) empty_output_buffer(j_compress_ptr cinfo)
{
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;

    memcpy(dest->outbuffer_cursor, dest->buffer, OUTPUT_BUF_SIZE);
    dest->outbuffer_cursor += OUTPUT_BUF_SIZE;
    *(dest->written) += OUTPUT_BUF_SIZE;

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

    return TRUE;
}

/******************************************************************************
Description.: called by jpeg_finish_compress after all data has been written.
              Usually needs to flush buffer.
Input Value.:
Return Value:
******************************************************************************/
METHODDEF(void) term_destination(j_compress_ptr cinfo)
{
    mjpg_dest_ptr dest = (mjpg_dest_ptr) cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

    /* Write any data remaining in the buffer */
    memcpy(dest->outbuffer_cursor, dest->buffer, datacount);
    dest->outbuffer_cursor += datacount;
    *(dest->written) += datacount;
}

/******************************************************************************
Description.: Prepare for output to a stdio stream.
Input Value.: buffer is the already allocated buffer memory that will hold
              the compressed picture. "size" is the size in bytes.
Return Value: -
******************************************************************************/
GLOBAL(void) dest_buffer(j_compress_ptr cinfo, unsigned char *buffer, int size, int *written)
{
    mjpg_dest_ptr dest;

    if(cinfo->dest == NULL) {
        cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(mjpg_destination_mgr));
    }

    dest = (mjpg_dest_ptr) cinfo->dest;
    dest->pub.init_destination = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination = term_destination;
    dest->outbuffer = buffer;
    dest->outbuffer_size = size;
    dest->outbuffer_cursor = buffer;
    dest->written = written;
}

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
    static int written;

    line_buffer = calloc(vd->width * 3, 1);
    yuyv = vd->framebuffer;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    /* jpeg_stdio_dest (&cinfo, file); */
    dest_buffer(&cinfo, buffer, size, &written);

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
    } else if (vd->formatIn == V4L2_PIX_FMT_RGB24) {
        while(cinfo.next_scanline < vd->height) {
            int x;
            unsigned char *ptr = line_buffer;

            for(x = 0; x < vd->width; x++) {
                *(ptr++) = yuyv[0];
                *(ptr++) = yuyv[1];
                *(ptr++) = yuyv[2];
                yuyv += 3;
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
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    free(line_buffer);

    return (written);
}


void *memcpy_rgb2y(unsigned char *dest, const unsigned char *src, size_t count){
	// char *d = (char*)dest;
	// const char *s = (char*)src;
	while(count--){
		// *d++ = *s++; //*s++ = *(s++)
		// *d++ = ((*s++)*1224+(*s++)*2404+(*s++)*467)>>12;
		*dest++ = ((*src++)*1224+(*src++)*2404+(*src++)*467)>>12;
	}
}
unsigned long jpeg_imageFile_dec_rgb2y_enc(unsigned char *outbuff, unsigned char *inbuff, size_t inbuff_len, int width, int height, int quality){
	struct jpeg_decompress_struct cinfo;
	JSAMPARRAY raw_buffer;
	char *src_buff;
	/**分配数据空间*/
	src_buff = (char *)malloc(width*height);// 需要释放
	/**定义一个标准的错误结构体，一旦程序出现错误就会调用exit()函数，退出进程**/
	struct jpeg_error_mgr jpeg_err;
	cinfo.err = jpeg_std_error(&jpeg_err);
	jpeg_create_decompress(&cinfo);
	jpeg_mem_src(&cinfo, inbuff, inbuff_len);
	jpeg_read_header(&cinfo, TRUE);
	/**开始解压缩**/
	jpeg_start_decompress(&cinfo);
	raw_buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo,JPOOL_IMAGE, width*3, 1);
	/***图像宽高，深度**/
	// width = cinfo.output_width;
	// height = cinfo.output_height;
	// depth = cinfo.output_components;
	char *ptr_src_buff = src_buff;
	while(cinfo.output_scanline < height){
		/***读取一行jpeg数据到raw_buffer***/
		jpeg_read_scanlines(&cinfo, raw_buffer, 1);
		// Y = 0.299R+0.587G+0.114B
		/**将raw_buffer中的数据给src_buff***/
		// memcpy(ptr_src_buff, *raw_buffer, width); //直传
		memcpy_rgb2y(ptr_src_buff, *raw_buffer, width); // rgb2y
		/***指针偏移一行***/
		ptr_src_buff += width;
	}

	/***解压缩完毕***/
	jpeg_finish_decompress(&cinfo);
	/***释放资源***/
	jpeg_destroy_decompress(&cinfo);

	// ywz 第二阶段 src_buff再编码：
	/**定义一个标准的错误结构体，一旦程序出现错误就会调用exit()函数，退出进程**/
	unsigned long jpeg_size;
	struct jpeg_error_mgr jpeg_err_enc;
	struct jpeg_compress_struct cinfo_enc;
	/***绑定错误处理结构对象**/
	cinfo_enc.err = jpeg_std_error(&jpeg_err_enc);
	jpeg_create_compress(&cinfo_enc);

	/****1. 指定目标源***/
	jpeg_mem_dest(&cinfo_enc, &outbuff, &jpeg_size);
	/**2. 指定图像宽高，深度，色彩空间***/
	cinfo_enc.image_width = width;
	cinfo_enc.image_height = height;
	cinfo_enc.input_components = 1;
	cinfo_enc.in_color_space = JCS_GRAYSCALE;//JCS_GRAYSCALE; ;//JCS_RGB;
	// cinfo_enc.num_components = depth;
	// cinfo_enc.data_precision = 8;
	/**设置jpeg默认值**/
	jpeg_set_defaults(&cinfo_enc);
	/**设置图片质量***/
	jpeg_set_quality(&cinfo_enc, quality, TRUE); // quality
	jpeg_start_compress(&cinfo_enc, TRUE);
	while(cinfo_enc.next_scanline < cinfo_enc.image_height){
		raw_buffer = &src_buff[cinfo_enc.next_scanline * width];
		jpeg_write_scanlines(&cinfo_enc, &raw_buffer, 1);
	}

	/**图像压缩结束**/
	jpeg_finish_compress(&cinfo_enc);
	/***销毁图像压缩***/
	jpeg_destroy_compress(&cinfo_enc);
	free(src_buff);
	// ywz 第二阶段done
	return jpeg_size;
}
