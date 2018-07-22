#ifndef _EPEG_PRIVATE_H
#define _EPEG_PRIVATE_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <setjmp.h>
#include <jpeglib.h>
#include <libexif/exif-data.h>

//#include "config.h"

typedef struct _epeg_error_mgr *emptr;

struct _epeg_error_mgr
{
      struct     jpeg_error_mgr pub;
      jmp_buf    setjmp_buffer;
};

struct _Epeg_Image
{
   struct _epeg_error_mgr          jerr;
   struct stat                     stat_info;
   unsigned char                  *pixels;
   unsigned char                 **lines;
   
   char                            scaled : 1;
   
   int                             error;
   
   Epeg_Colorspace                 color_space;
   
   struct {
      char                          *file;
      struct {
	 unsigned char           **data;
	 int                       size;
      } mem;
      int                            w, h;
      char                          *comment;
      FILE                          *f;
      J_COLOR_SPACE                  color_space;
      int                            orientation;  /* Exif orientation values 0-8 */
      struct jpeg_decompress_struct  jinfo;
      struct {
	 char                       *uri;
	 unsigned long long int      mtime;
	 int                         w, h;
	 char                       *mime;
      } thumb_info;
   } in;
   struct {
      char                        *file;
      struct {
	 unsigned char           **data;
	 int                      *size;
      } mem;
      int                          x, y;
      int                          w, h;
      char                        *comment;
      FILE                        *f;
      struct jpeg_compress_struct  jinfo;
      int                          quality;
      char                         thumbnail_info : 1;
   } out;
};

METHODDEF(void) _jpeg_decompress_error_exit(j_common_ptr cinfo);
METHODDEF(void) _jpeg_init_source(j_decompress_ptr cinfo);
METHODDEF(boolean) _jpeg_fill_input_buffer(j_decompress_ptr cinfo);
METHODDEF(void) _jpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes);
METHODDEF(void) _jpeg_term_source(j_decompress_ptr cinfo);

METHODDEF(void) _jpeg_init_destination(j_compress_ptr cinfo);
METHODDEF(boolean) _jpeg_empty_output_buffer (j_compress_ptr cinfo);
METHODDEF(void) _jpeg_term_destination (j_compress_ptr cinfo);
    
METHODDEF(void) _emit_message (j_common_ptr cinfo, int msg_level);
METHODDEF(void) _output_message (j_common_ptr cinfo);
METHODDEF(void) _format_message (j_common_ptr cinfo, char * buffer);
#endif
