#include "Epeg.h"
#include "epeg_private.h"
#include <jerror.h>

static Epeg_Image   *_epeg_open_header         (Epeg_Image *im);
static int           _epeg_decode              (Epeg_Image *im);
static int           _epeg_scale               (Epeg_Image *im);
static int           _epeg_decode_for_trim     (Epeg_Image *im);
static int           _epeg_trim                (Epeg_Image *im);
static int           _epeg_encode              (Epeg_Image *im);

static void          _epeg_fatal_error_handler (j_common_ptr cinfo);

static const JOCTET fake_EOI[2] = { 0xFF, JPEG_EOI };

static ExifByteOrder exif_byte_order = EXIF_BYTE_ORDER_INTEL;

/**
 * Open a JPEG image by filename.
 * @param file The file path to open.
 * @return A handle to the opened JPEG file, with the header decoded.
 * 
 * This function opens the file indicated by the @p file parameter, and
 * attempts to decode it as a jpeg file. If this failes, NULL is returned.
 * Otherwise a valid handle to an open JPEG file is returned that can be used
 * by other Epeg calls.
 * 
 * The @p file must be a pointer to a valid C string, NUL (0 byte) terminated
 * thats is a relative or absolute file path. If not results are not
 * determined.
 * 
 * See also: epeg_memory_open(), epeg_close()
 */
EAPI Epeg_Image *
epeg_file_open(const char *file)
{
   Epeg_Image *im;
   
   im = calloc(1, sizeof(Epeg_Image));
   if (!im) return NULL;
   
   im->in.file = strdup(file);
   if (!im->in.file)
     {
	free(im);
	return NULL;
     }
   
   im->in.f = fopen(im->in.file, "rb");
   if (!im->in.f)
     {
	epeg_close(im);
	return NULL;
     }
   fstat(fileno(im->in.f), &(im->stat_info));
   im->out.quality = 75;
   return _epeg_open_header(im);
}

/**
 * Open a JPEG image stored in memory.
 * @param data A pointer to the memory containing the JPEG data.
 * @param size The size of the memory segment containing the JPEG.
 * @return  A handle to the opened JPEG, with the header decoded.
 * 
 * This function opens a JPEG file that is stored in memory pointed to by
 * @p data, and that is @p size bytes in size. If successful a valid handle
 * is returned, or on failure NULL is returned.
 * 
 * See also: epeg_file_open(), epeg_close()
 */
EAPI EAPI Epeg_Image *
epeg_memory_open(unsigned char *data, int size)
{
   Epeg_Image *im;
   
   im = calloc(1, sizeof(Epeg_Image));
   if (!im) return NULL;
   
   im->out.quality = 75;
   im->in.mem.data = (unsigned char **)data;
   im->in.mem.size = size;
   im->in.f = NULL;
   im->in.w = 0;
   im->in.h = 0;
   return _epeg_open_header(im);
}

/**
 * Return the original JPEG pixel size.
 * @param im A handle to an opened Epeg image.
 * @param w A pointer to the width value in pixels to be filled in.
 * @param h A pointer to the height value in pixels to be filled in.
 * 
 * Returns the image size in pixels.
 * 
 */
EAPI void
epeg_size_get(Epeg_Image *im, int *w, int *h)
{
   if (w) *w = im->in.w;
   if (h) *h = im->in.h;
}

/**
 * Return the original JPEG pixel color space.
 * @param im A handle to an opened Epeg image.
 * @param space A pointer to the color space value to be filled in.
 * 
 * Returns the image color space.
 * 
 */
EAPI void
epeg_colorspace_get(Epeg_Image *im, int *space)
{
   if (space) *space = im->color_space;
}

/**
 * Set the size of the image to decode in pixels.
 * @param im A handle to an opened Epeg image.
 * @param w The width of the image to decode at, in pixels.
 * @param h The height of the image to decode at, in pixels.
 * 
 * Sets the size at which to deocode the JPEG image, giving an optimised load
 * that only decodes the pixels needed.
 * 
 */
EAPI void
epeg_decode_size_set(Epeg_Image *im, int w, int h)
{
   if      (im->pixels) return;
   if      (w < 1)        w = 1;
   else if (w > im->in.w) w = im->in.w;
   if      (h < 1)        h = 1;
   else if (h > im->in.h) h = im->in.h;
   im->out.w = w;
   im->out.h = h;
   im->out.x = 0;
   im->out.y = 0;
}

EAPI void
epeg_decode_bounds_set(Epeg_Image *im, int x, int y, int w, int h)
{
   if      (im->pixels) return;
   if      (w < 1)        w = 1;
   else if (w > im->in.w) w = im->in.w;
   if      (h < 1)        h = 1;
   else if (h > im->in.h) h = im->in.h;
   im->out.w = w;
   im->out.h = h;
   if      (x < 0)        x = 0;
   if      (y < 0)        y = 0;
   im->out.x = x;
   im->out.y = y;
}

/**
 * Set the colorspace in which to decode the image.
 * @param im A handle to an opened Epeg image.
 * @param colorspace The colorspace to decode the image in.
 * 
 * This sets the colorspace to decode the image in. The default is EPEG_YUV8,
 * as this is normally the native colorspace of a JPEG file, avoiding any
 * colorspace conversions for a faster load and/or save.
 */
EAPI void
epeg_decode_colorspace_set(Epeg_Image *im, Epeg_Colorspace colorspace)
{
   if (im->pixels) return;
   if ((colorspace < EPEG_GRAY8) || (colorspace > EPEG_CMYK)) return;
   im->color_space = colorspace;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 * 
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organsied from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 * 
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed or the image data cannot be decoded.
 * 
 */
EAPI const void *
epeg_pixels_get(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;
   
   if (!im->pixels)
     {
	if (_epeg_decode(im) != 0) return NULL;
     }
   
   if (!im->pixels) return NULL;
   if ((im->out.w < 1) || (im->out.h < 1)) return NULL;
   
   if (_epeg_scale(im) != 0) return NULL;
   
   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) ow = iw - x;
   if ((y + oh) > ih) oh = ih - y;
   if (ow < 1) return NULL;
   if (oh < 1) return NULL;
   if (x < 0)
     {
	ow += x;
	ox = -x;
     }
   if (y < 0)
     {
	oh += y;
	oy = -y;
     }
   if (ow < 1) return NULL;
   if (oh < 1) return NULL;

   ww = x + ox + ow;
   hh = y + oy + oh;

   if (im->color_space == EPEG_GRAY8)
     {
	unsigned char *pix, *p;
	
	pix = malloc(w * h * 1);
	if (!pix) return NULL;
	for (yy = y + oy; yy < hh; yy++)
	  {
	     unsigned char *s;
	     
	     s = im->lines[yy] + ((x + ox) * bpp);
	     p = pix + ((((yy - y) * w) + ox));
	     for (xx = x + ox; xx < ww; xx++)
	       {
		  p[0] = s[0];
		  p++;
		  s += bpp;
	       }
	  }
	return pix;
     }
   else if (im->color_space == EPEG_YUV8)
     {
	unsigned char *pix, *p;
	
	pix = malloc(w * h * 3);
	if (!pix) return NULL;
	for (yy = y + oy; yy < hh; yy++)
	  {
	     unsigned char *s;
	     
	     s = im->lines[yy] + ((x + ox) * bpp);
	     p = pix + ((((yy - y) * w) + ox) * 3);
	     for (xx = x + ox; xx < ww; xx++)
	       {
		  p[0] = s[0];
		  p[1] = s[1];
		  p[2] = s[2];
		  p += 3;
		  s += bpp;
	       }
	  }
	return pix;
     }
   else if (im->color_space == EPEG_RGB8)
     {
	unsigned char *pix, *p;
	
	pix = malloc(w * h * 3);
	if (!pix) return NULL;
	for (yy = y + oy; yy < hh; yy++)
	  {
	     unsigned char *s;
	     
	     s = im->lines[yy] + ((x + ox) * bpp);
	     p = pix + ((((yy - y) * w) + ox) * 3);
	     for (xx = x + ox; xx < ww; xx++)
	       {
		  p[0] = s[0];
		  p[1] = s[1];
		  p[2] = s[2];
		  p += 3;
		  s += bpp;
	       }
	  }
	return pix;
     }
   else if (im->color_space == EPEG_BGR8)
     {
	unsigned char *pix, *p;
	
	pix = malloc(w * h * 3);
	if (!pix) return NULL;
	for (yy = y + oy; yy < hh; yy++)
	  {
	     unsigned char *s;
	     
	     s = im->lines[yy] + ((x + ox) * bpp);
	     p = pix + ((((yy - y) * w) + ox) * 3);
	     for (xx = x + ox; xx < ww; xx++)
	       {
		  p[0] = s[2];
		  p[1] = s[1];
		  p[2] = s[0];
		  p += 3;
		  s += bpp;
	       }
	  }
	return pix;
     }
   else if (im->color_space == EPEG_RGBA8)
     {
	unsigned char *pix, *p;
	
	pix = malloc(w * h * 4);
	if (!pix) return NULL;
	for (yy = y + oy; yy < hh; yy++)
	  {
	     unsigned char *s;
	     
	     s = im->lines[yy] + ((x + ox) * bpp);
	     p = pix + ((((yy - y) * w) + ox) * 4);
	     for (xx = x + ox; xx < ww; xx++)
	       {
		  p[0] = s[0];
		  p[1] = s[1];
		  p[2] = s[2];
		  p[3] = 0xff;
		  p += 4;
		  s += bpp;
	       }
	  }
	return pix;
     }
   else if (im->color_space == EPEG_BGRA8)
     {
	unsigned char *pix, *p;
	
	pix = malloc(w * h * 4);
	if (!pix) return NULL;
	for (yy = y + oy; yy < hh; yy++)
	  {
	     unsigned char *s;
	     
	     s = im->lines[yy] + ((x + ox) * bpp);
	     p = pix + ((((yy - y) * w) + ox) * 4);
	     for (xx = x + ox; xx < ww; xx++)
	       {
		  p[0] = 0xff;
		  p[1] = s[2];
		  p[2] = s[1];
		  p[3] = s[0];
		  p += 4;
		  s += bpp;
	       }
	  }
	return pix;
     }
   else if (im->color_space == EPEG_ARGB32)
     {
	unsigned int *pix, *p;
	
	pix = malloc(w * h * 4);
	if (!pix) return NULL;
	for (yy = y + oy; yy < hh; yy++)
	  {
	     unsigned char *s;
	     
	     s = im->lines[yy] + ((x + ox) * bpp);
	     p = pix + ((((yy - y) * w) + ox));
	     for (xx = x + ox; xx < ww; xx++)
	       {
		  p[0] = 0xff000000 | (s[0] << 16) | (s[1] << 8) | (s[2]);
		  p++;
		  s += bpp;
	       }
	  }
	return pix;
     }
   else if (im->color_space == EPEG_CMYK)
     {
	unsigned char *pix, *p;
	
	pix = malloc(w * h * 4);
	if (!pix) return NULL;
	for (yy = y + oy; yy < hh; yy++)
	  {
	     unsigned char *s;
	     
	     s = im->lines[yy] + ((x + ox) * bpp);
	     p = pix + ((((yy - y) * w) + ox) * 4);
	     for (xx = x + ox; xx < ww; xx++)
	       {
		  p[0] = s[0];
		  p[1] = s[1];
		  p[2] = s[2];
		  p[3] = 0xff;
		  p += 4;
		  s += bpp;
	       }
	  }
	return pix;
     }
   return NULL;
}

/**
 * Get a segment of decoded pixels from an image.
 * @param im A handle to an opened Epeg image.
 * @param x Rectangle X.
 * @param y Rectangle Y.
 * @param w Rectangle width.
 * @param h Rectangle height.
 * @return Pointer to the top left of the requested pixel block.
 * 
 * Return image pixels in the decoded format from the specified location
 * rectangle bounded with the box @p x, @p y @p w X @p y. The pixel block is
 * packed with no row padding, and it organsied from top-left to bottom right,
 * row by row. You must free the pixel block using epeg_pixels_free() before
 * you close the image handle, and assume the pixels to be read-only memory.
 * 
 * On success the pointer is returned, on failure, NULL is returned. Failure
 * may be because the rectangle is out of the bounds of the image, memory
 * allocations failed or the image data cannot be decoded.
 * 
 */
EAPI const void *
epeg_pixels_get_as_RGB8(Epeg_Image *im, int x, int y,  int w, int h)
{
   int xx, yy, ww, hh, bpp, ox, oy, ow, oh, iw, ih;
   
   if (!im->pixels)
     {
	if (_epeg_decode(im) != 0) return NULL;
     }
	
   if (!im->pixels) return NULL;
   if ((im->out.w < 1) || (im->out.h < 1)) return NULL;
   
   bpp = im->in.jinfo.output_components;
   iw = im->out.w;
   ih = im->out.h;
   ow = w;
   oh = h;
   ox = 0;
   oy = 0;
   if ((x + ow) > iw) ow = iw - x;
   if ((y + oh) > ih) oh = ih - y;
   if (ow < 1) return NULL;
   if (oh < 1) return NULL;
   if (x < 0)
     {
	ow += x;
	ox = -x;
     }
   if (y < 0)
     {
	oh += y;
	oy = -y;
     }
   if (ow < 1) return NULL;
   if (oh < 1) return NULL;
   
   ww = x + ox + ow;
   hh = y + oy + oh;
   
   if (im->color_space == EPEG_GRAY8)
     {
	unsigned char *pix, *p;
	
	pix = malloc(w * h * 3);
	if (!pix) return NULL;
	for (yy = y + oy; yy < hh; yy++)
	  {
	     unsigned char *s;
	     
	     s = im->lines[yy] + ((x + ox) * bpp);
	     p = pix + ((((yy - y) * w) + ox) * 3);
	     for (xx = x + ox; xx < ww; xx++)
	       {
		  p[0] = s[0];
		  p[1] = s[0];
		  p[2] = s[0];
		  p += 3;
		  s += bpp;
	       }
	  }
	return pix;
     }
   if (im->color_space == EPEG_RGB8)
     {
	unsigned char *pix, *p;
	
	pix = malloc(w * h * 3);
	if (!pix) return NULL;
	for (yy = y + oy; yy < hh; yy++)
	  {
	     unsigned char *s;
	     
	     s = im->lines[yy] + ((x + ox) * bpp);
	     p = pix + ((((yy - y) * w) + ox) * 3);
	     for (xx = x + ox; xx < ww; xx++)
	       {
		  p[0] = s[0];
		  p[1] = s[1];
		  p[2] = s[2];
		  p += 3;
		  s += bpp;
	       }
	  }
	return pix;
     }
   if (im->color_space == EPEG_CMYK)
     {
	unsigned char *pix, *p;
	
	pix = malloc(w * h * 3);
	if (!pix) return NULL;
	for (yy = y + oy; yy < hh; yy++)
	  {
	     unsigned char *s;
	     
	     s = im->lines[yy] + ((x + ox) * bpp);
	     p = pix + ((((yy - y) * w) + ox) * 3);
	     for (xx = x + ox; xx < ww; xx++)
	       {
		  p[0] = (unsigned char)(MIN(255, (s[0] * s[3]) / 255));
		  p[1] = (unsigned char)(MIN(255, (s[1] * s[3]) / 255));
		  p[2] = (unsigned char)(MIN(255, (s[2] * s[3]) / 255));
		  p += 3;
		  s += bpp;
	       }
	  }
	return pix;
     }
   return NULL;
}

/**
 * Free requested pixel block from an image.
 * @param im A handle to an opened Epeg image.
 * @param data The pointer to the image pixels.
 * 
 * This frees the data for a block of pixels requested from image @p im.
 * @p data must be a valid (non NULL) pointer to a pixel block taken from the
 * image @p im by epeg_pixels_get() and mustbe called before the image is
 * closed by epeg_close().
 */
EAPI void
epeg_pixels_free(Epeg_Image *im, const void *data)
{
   free((void *)data);
}

/**
 * Get the image comment field as a string.
 * @param im A handle to an opened Epeg image.
 * @return A pointer to the loaded image comments.
 * 
 * This function returns the comment field as a string (NUL byte terminated)
 * of the loaded image @p im, if there is a comment, or NULL if no comment is
 * saved with the image. Consider the string returned to be read-only.
 * 
 */
EAPI const char *
epeg_comment_get(Epeg_Image *im)
{
   return im->in.comment;
}

/**
 * Get thumbnail comments of loaded image.
 * @param im A handle to an opened Epeg image.
 * @param info Pointer to a thumbnail info struct to be filled in.
 * 
 * This function retrieves thumbnail comments written by Epeg to any saved
 * JPEG files. If no thumbnail comments were saved, the fields will be 0 in
 * the @p info struct on return.
 * 
 */
EAPI void
epeg_thumbnail_comments_get(Epeg_Image *im, Epeg_Thumbnail_Info *info)
{
   if (!info) return;
   info->uri      = im->in.thumb_info.uri;
   info->mtime    = im->in.thumb_info.mtime;
   info->w        = im->in.thumb_info.w;
   info->h        = im->in.thumb_info.h;
   info->mimetype = im->in.thumb_info.mime;
}

/**
 * Set the comment field of the image for saving.
 * @param im A handle to an opened Epeg image.
 * @param comment The comment to set.
 * 
 * Set the comment for the image file for when it gets saved. This is a NUL
 * byte terminated C string. If @p comment is NULL the output file will have
 * no comment field.
 * 
 * The default comment will be any comment loaded from the input file.
 * 
 */
EAPI void
epeg_comment_set(Epeg_Image *im, const char *comment)
{
   if (im->out.comment) free(im->out.comment);
   if (!comment) im->out.comment = NULL;
   else im->out.comment = strdup(comment);
}

/**
 * Set the encoding quality of the saved image.
 * @param im A handle to an opened Epeg image.
 * @param quality The quality of encoding from 0 to 100.
 * 
 * Set the quality of the output encoded image. Values from 0 to 100
 * inclusive are valid, with 100 being the maximum quality, and 0 being the
 * minimum. If the quality is set equal to or above 90%, the output U and V
 * color planes are encoded at 1:1 with the Y plane.
 * 
 * The default quality is 75.
 * 
 */
EAPI void
epeg_quality_set(Epeg_Image *im, int quality)
{
   if      (quality < 0)   quality = 0;
   else if (quality > 100) quality = 100;
   im->out.quality = quality;
}

/**
 * Enable thumbnail comments in saved image.
 * @param im A handle to an opened Epeg image.
 * @param onoff A boolean on and off enabling flag.
 * 
 * if @p onoff is 1, the output file will have thumbnail comments added to
 * it, and if it is 0, it will not. The default is 0.
 * 
 */
EAPI void
epeg_thumbnail_comments_enable(Epeg_Image *im, int onoff)
{
   im->out.thumbnail_info = onoff;
}

/**
 * Set the output file path for the image when saved.
 * @param im A handle to an opened Epeg image.
 * @param file The path to the output file.
 * 
 * This sets the output file path name (either a full or relative path name)
 * to where the file will be written when saved. @p file must be a NUL
 * terminated C string conatining the path to the file to be saved to. If it is
 * NULL, the image will not be saved to a file when calling epeg_encode().
 */
EAPI void
epeg_file_output_set(Epeg_Image *im, const char *file)
{
   if (im->out.file) free(im->out.file);
   if (!file) im->out.file = NULL;
   else im->out.file = strdup(file);
}

/**
 * Set the output file to be a block of allocated memory.
 * @param im A handle to an opened Epeg image.
 * @param data A pointer to a pointer to a memory block.
 * @param size A pointer to a counter of the size of the memory block.
 * 
 * This sets the output encoding of the image when saved to be allocated
 * memory. After epeg_close() is called the pointer pointed to by @p data
 * and the integer pointed to by @p size will contain the pointer to the
 * memory block and its size in bytes, respecitvely. The memory block can be
 * freed with the free() function call. If the save fails the pointer to the
 * memory block will be unaffected, as will the size.
 * 
 */
EAPI void
epeg_memory_output_set(Epeg_Image *im, unsigned char **data, int *size)
{
   im->out.mem.data = data;
   im->out.mem.size = size;
   im->out.file = NULL;
}

/**
 * This saves the image to its specified destination.
 * @param im A handle to an opened Epeg image.
 * 
 * This saves the image @p im to its destination specified by
 * epeg_file_output_set() or epeg_memory_output_set(). The image will be
 * encoded at the deoded pixel size, using the quality, comment and thumbnail
 * comment settings set on the image.
 *
 * retval 1 - error scale
 *        2 - error encode
 *        3 - error decode
 *        4 - error decode ( setjmp )
 */
EAPI int
epeg_encode(Epeg_Image *im)
{
   int ret;
   if ((ret = _epeg_decode(im)) != 0)
     return (ret == 2 ? 4 : 3);
   if (_epeg_scale(im) != 0)
     return 1;
   if (_epeg_encode(im) != 0)
     return 2;
   return 0;
}

/**
 * FIXME: Document this
 * @param im A handle to an opened Epeg image.
 * 
 * FIXME: Document this.
 */
EAPI int
epeg_trim(Epeg_Image *im)
{
   if (_epeg_decode_for_trim(im) != 0)
     return 1;
   if (_epeg_trim(im) != 0)
     return 1;
   if (_epeg_encode(im) != 0)
     return 1;
   return 0;
}

/**
 * Close an image handle.
 * @param im A handle to an opened Epeg image.
 * 
 * This closes an opened image handle and frees all memory associated with it.
 * It does not free encoded data generated by epeg_memory_output_set() followed
 * by epeg_encode() nor does it guarantee to free any data recieved by
 * epeg_pixels_get(). Once an image handle is closed consider it invalid.
 */
EAPI void
epeg_close(Epeg_Image *im)
{
   if (!im) return;
   if (im->pixels)                   free(im->pixels);
   if (im->lines)                    free(im->lines);
   if (im->in.file)                  free(im->in.file);
   if (!im->in.file)                 free(im->in.jinfo.src);
   if (im->in.f || im->in.mem.data)  jpeg_destroy_decompress(&(im->in.jinfo));
   if (im->in.f)                     fclose(im->in.f);
   if (im->in.comment)               free(im->in.comment);
   if (im->in.thumb_info.uri)        free(im->in.thumb_info.uri);
   if (im->in.thumb_info.mime)       free(im->in.thumb_info.mime);
   if (im->out.file)                 free(im->out.file);
   if (!im->out.file)                free(im->out.jinfo.dest);
   if (im->out.f || im->in.mem.data) jpeg_destroy_compress(&(im->out.jinfo));
   if (im->out.f)                    fclose(im->out.f);
   if (im->out.comment)              free(im->out.comment);
   free(im);
}

static Epeg_Image *
_epeg_open_header(Epeg_Image *im)
{
   struct jpeg_marker_struct *m;
   struct jpeg_source_mgr *src_mgr = NULL;

   im->in.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;
#ifdef NOWARNINGS
   im->jerr.pub.emit_message = _emit_message;
   im->jerr.pub.output_message = _output_message;
   im->jerr.pub.format_message = _format_message;
#endif
   
   if (setjmp(im->jerr.setjmp_buffer))
     {
	error:
	epeg_close(im);
	im = NULL;
	return NULL;
     }
   
   jpeg_create_decompress(&(im->in.jinfo));
   jpeg_save_markers(&(im->in.jinfo), JPEG_APP0 + 7, 1024);
   /* Save Exif markers */
   jpeg_save_markers(&(im->in.jinfo), JPEG_APP0 + 1, 65535);
   jpeg_save_markers(&(im->in.jinfo), JPEG_COM,      65535);
   if (im->in.f != NULL)
     {
	jpeg_stdio_src(&(im->in.jinfo), im->in.f);
     }
   else
     {
	/* Setup RAM source manager. */
	src_mgr = calloc(1, sizeof(struct jpeg_source_mgr));
	if (!src_mgr) goto error;
	src_mgr->init_source = _jpeg_init_source;
	src_mgr->fill_input_buffer = _jpeg_fill_input_buffer;
	src_mgr->skip_input_data = _jpeg_skip_input_data;
	src_mgr->resync_to_restart = jpeg_resync_to_restart;
	src_mgr->term_source = _jpeg_term_source;
	src_mgr->bytes_in_buffer = im->in.mem.size;
	src_mgr->next_input_byte = (JOCTET *) im->in.mem.data;
   	im->in.jinfo.src = (struct jpeg_source_mgr *) src_mgr;
     }

   jpeg_read_header(&(im->in.jinfo), TRUE);
   im->in.w = im->in.jinfo.image_width;
   im->in.h = im->in.jinfo.image_height;
   if (im->in.w < 1) goto error;
   if (im->in.h < 1) goto error;
   
   im->out.w = im->in.w;
   im->out.h = im->in.h;

   im->color_space = ((im->in.color_space = im->in.jinfo.out_color_space) == JCS_GRAYSCALE) ? EPEG_GRAY8 : EPEG_RGB8;
   if (im->in.color_space == JCS_CMYK) im->color_space = EPEG_CMYK;

   /* Will be set if orientation is present in EXIF data */
   im->in.orientation = 0;

   for (m = im->in.jinfo.marker_list; m; m = m->next)
     {
	if (m->marker == JPEG_COM)
	  {
	     if (im->in.comment) free(im->in.comment);
	     im->in.comment = malloc(m->data_length + 1);
	     if (im->in.comment)
	       {
		  memcpy(im->in.comment, m->data, m->data_length);
		  im->in.comment[m->data_length] = 0;
	       }
	  }
	else if (m->marker == (JPEG_APP0 + 7))
	  {
	     if ((m->data_length > 7) &&
		 (!strncmp((char *)m->data, "Thumb::", 7)))
	       {
		  char *p, *p2;
		  
		  p = malloc(m->data_length + 1);
		  if (p)
		    {
		       memcpy(p, m->data, m->data_length);
		       p[m->data_length] = 0;
		       p2 = strchr(p, '\n');
		       if (p2)
			 {
			    p2[0] = 0;
			    if (!strcmp(p, "Thumb::URI"))

			      im->in.thumb_info.uri = strdup(p2 + 1);
			    else if (!strcmp(p, "Thumb::MTime"))
			      sscanf(p2 + 1, "%llu", &(im->in.thumb_info.mtime));
			    else if (!strcmp(p, "Thumb::Image::Width"))
			      im->in.thumb_info.w = atoi(p2 + 1);
			    else if (!strcmp(p, "Thumb::Image::Height"))
			      im->in.thumb_info.h = atoi(p2 + 1);
			    else if (!strcmp(p, "Thumb::Mimetype"))
			      im->in.thumb_info.mime = strdup(p2 + 1);
			 }
		       free(p);
		    }
	       }
	  }
	else if (m->marker == (JPEG_APP0 + 1))
	{
	    /*
	    *	Look for an Exif Orientation tag. If found,
	    *	store it in im->in.orientation. Later, this will
	    *	be written to the output jpeg Exif data.
	    */
            ExifData *ed = exif_data_new_from_data(m->data, m->data_length);
            if (ed) {
             	exif_byte_order = exif_data_get_byte_order(ed);
             	ExifEntry *entry = exif_content_get_entry(ed->ifd[EXIF_IFD_0],EXIF_TAG_ORIENTATION);
                if (entry) {
                    im->in.orientation = exif_get_short(entry->data, exif_byte_order);
                    // do not unref the entry since exif_content_get_entry does not copy the memory, 
                    // it returns the entry pointer from ed, this is the reason way exif_data_unref throws segfault
                    // exif_entry_unref(entry);
	     	}
	    }            
            exif_data_unref(ed);
     	}
    }
    return im;
}

/**
  retval 1 - malloc or other
         2 - setjmp error
*/
static int
_epeg_decode(Epeg_Image *im)
{
   int scale, scalew, scaleh, y;
   JDIMENSION old_output_scanline = 1;
   
   if (im->pixels) return 1;
   if ((im->out.w < 1) || (im->out.h < 1)) return 1;
   
   scalew = im->in.w / im->out.w;
   scaleh = im->in.h / im->out.h;
   
   scale = scalew;   
   if (scaleh < scalew) scale = scaleh;

   if      (scale > 8) scale = 8;
   else if (scale < 1) scale = 1;
   
   im->in.jinfo.scale_num           = 1;
   im->in.jinfo.scale_denom         = scale;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing  = FALSE;
   im->in.jinfo.dct_method          = JDCT_IFAST;

   switch (im->color_space)
     {
      case EPEG_GRAY8:
	im->in.jinfo.out_color_space = JCS_GRAYSCALE;
	im->in.jinfo.output_components = 1;
	break;
		
      case EPEG_YUV8:
	im->in.jinfo.out_color_space = JCS_YCbCr;
	break;
		
      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
	im->in.jinfo.out_color_space = JCS_RGB;
	break;
		
      case EPEG_CMYK:
	im->in.jinfo.out_color_space = JCS_CMYK;
	im->in.jinfo.output_components = 4;
	break;
	
      default:
	break;
     }
   
   im->out.jinfo.err			= jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit		= _epeg_fatal_error_handler;
#ifdef NOWARNINGS
   im->jerr.pub.emit_message = _emit_message;
   im->jerr.pub.output_message = _output_message;
   im->jerr.pub.format_message = _format_message;
#endif

   if (setjmp(im->jerr.setjmp_buffer))
     return 2;

   jpeg_calc_output_dimensions(&(im->in.jinfo));
   
   im->pixels = malloc(im->in.jinfo.output_width * im->in.jinfo.output_height * im->in.jinfo.output_components);
   if (!im->pixels) return 1;
	
   im->lines = malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines)
     {
	free(im->pixels);
	im->pixels = NULL;
	return 1;
     }
	
   jpeg_start_decompress(&(im->in.jinfo));
   
   for (y = 0; y < im->in.jinfo.output_height; y++)
     im->lines[y] = im->pixels + (y * im->in.jinfo.output_components * im->in.jinfo.output_width);
   
   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height)
     {
	if (old_output_scanline == im->in.jinfo.output_scanline)
	  {
	     jpeg_abort_decompress(&(im->in.jinfo));
	     return 1;
	  }
	old_output_scanline = im->in.jinfo.output_scanline;
	jpeg_read_scanlines(&(im->in.jinfo), 
			    &(im->lines[im->in.jinfo.output_scanline]), 
			    im->in.jinfo.rec_outbuf_height);
     }
   
   jpeg_finish_decompress(&(im->in.jinfo));
   
   return 0;
}

static int
_epeg_scale(Epeg_Image *im)
{
   unsigned char *dst, *row, *src;
   int            x, y, w, h, i;
   
   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) return 0;
   if (im->scaled) return 0;
   
   if ((im->out.w < 1) || (im->out.h < 1)) return 0;
   
   im->scaled = 1;
   w = im->out.w;
   h = im->out.h;
   for (y = 0; y < h; y++)
     {
	row = im->pixels + (((y * im->in.jinfo.output_height) / h) * im->in.jinfo.output_components * im->in.jinfo.output_width);
	dst = im->pixels + (y * im->in.jinfo.output_components * im->in.jinfo.output_width);
	
	for (x = 0; x < im->out.w; x++)
	  {
	     src = row + (((x * im->in.jinfo.output_width) / w) * im->in.jinfo.output_components);
	     for (i = 0; i < im->in.jinfo.output_components; i++)
	       dst[i] = src[i];
	     dst += im->in.jinfo.output_components;
	  }
     }
   return 0;
}

static int
_epeg_decode_for_trim(Epeg_Image *im)
{
   int		y;
   
   if (im->pixels) return 1;

   im->in.jinfo.scale_num           = 1;
   im->in.jinfo.scale_denom         = 1;
   im->in.jinfo.do_fancy_upsampling = FALSE;
   im->in.jinfo.do_block_smoothing  = FALSE;
   im->in.jinfo.dct_method          = JDCT_ISLOW;
   
   switch (im->color_space)
     {
      case EPEG_GRAY8:
	im->in.jinfo.out_color_space = JCS_GRAYSCALE;
	im->in.jinfo.output_components = 1;
	break;
	
      case EPEG_YUV8:
	im->in.jinfo.out_color_space = JCS_YCbCr;
	break;
	
      case EPEG_RGB8:
      case EPEG_BGR8:
      case EPEG_RGBA8:
      case EPEG_BGRA8:
      case EPEG_ARGB32:
	im->in.jinfo.out_color_space = JCS_RGB;
	break;
	
      case EPEG_CMYK:
	im->in.jinfo.out_color_space = JCS_CMYK;
	im->in.jinfo.output_components = 4;
	break;
	
      default:
	break;
     }
   
   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;
#ifdef NOWARNINGS
   im->jerr.pub.emit_message = _emit_message;
   im->jerr.pub.output_message = _output_message;
   im->jerr.pub.format_message = _format_message;
#endif
   
   if (setjmp(im->jerr.setjmp_buffer))
     return 1;

   jpeg_calc_output_dimensions(&(im->in.jinfo));

   im->pixels = malloc(im->in.jinfo.output_width * im->in.jinfo.output_height * im->in.jinfo.output_components);
   if (!im->pixels) return 1;
   
   im->lines = malloc(im->in.jinfo.output_height * sizeof(char *));
   if (!im->lines)
     {
	free(im->pixels);
	im->pixels = NULL;
	return 1;
     }
   
   jpeg_start_decompress(&(im->in.jinfo));
   
   for (y = 0; y < im->in.jinfo.output_height; y++)
     im->lines[y] = im->pixels + (y * im->in.jinfo.output_components * im->in.jinfo.output_width);
   
   while (im->in.jinfo.output_scanline < im->in.jinfo.output_height)
     jpeg_read_scanlines(&(im->in.jinfo), 
			 &(im->lines[im->in.jinfo.output_scanline]), 
			 im->in.jinfo.rec_outbuf_height);
   
   jpeg_finish_decompress(&(im->in.jinfo));
   
   return 0;
}

static int
_epeg_trim(Epeg_Image *im)
{
   int            y, a, b, h;
   
   if ((im->in.w == im->out.w) && (im->in.h == im->out.h)) return 1;
   if (im->scaled) return 1;
   
   im->scaled = 1;
   h = im->out.h;
   a = im->out.x;
   b = im->out.y;
   
   for (y = 0; y < h; y++)
     im->lines[y] = im->pixels + ((y+b) * im->in.jinfo.output_components * im->in.jinfo.output_width) + (a * im->in.jinfo.output_components);
   
   return 0;
}

struct epeg_destination_mgr
{
   struct jpeg_destination_mgr dst_mgr;
   Epeg_Image *im;
   unsigned char *buf;
};

/* Get an existing tag, or create one if it doesn't exist */
static ExifEntry *init_tag(ExifData *exif, ExifIfd ifd, ExifTag tag)
{
    ExifEntry *entry = exif_content_get_entry (exif->ifd[ifd], tag);
    if (entry)
        return entry;
    entry = exif_entry_new ();
    if (entry) {
        exif_content_add_entry (exif->ifd[ifd], entry);
        exif_entry_initialize (entry, tag);
    }
    return entry;
}

static int
_epeg_encode(Epeg_Image *im)
{
   struct epeg_destination_mgr *dst_mgr = NULL;
   int ok = 0;

   if ((im->out.w < 1) || (im->out.h < 1)) return 1;
   if (im->out.f) return 1;
   
   if (im->out.file)
     {
	im->out.f = fopen(im->out.file, "wb");
	if (!im->out.f)
   	  {
	     im->error = 1;
	     return 1;
	  }
     }
   else
     im->out.f = NULL;
   
   im->out.jinfo.err = jpeg_std_error(&(im->jerr.pub));
   im->jerr.pub.error_exit = _epeg_fatal_error_handler;
#ifdef NOWARNINGS
   im->jerr.pub.emit_message = _emit_message;
   im->jerr.pub.output_message = _output_message;
   im->jerr.pub.format_message = _format_message;
#endif
   
   if (setjmp(im->jerr.setjmp_buffer))
     {
	ok = 1;
	im->error = 1;
	goto done;
     }

   jpeg_create_compress(&(im->out.jinfo));
   if (im->out.f)
     jpeg_stdio_dest(&(im->out.jinfo), im->out.f);
   else
     {
	*(im->out.mem.data) = NULL;
	*(im->out.mem.size) = 0;
	/* Setup RAM destination manager */
	dst_mgr = calloc(1, sizeof(struct epeg_destination_mgr));
	if (!dst_mgr) return 1;
	dst_mgr->dst_mgr.init_destination = _jpeg_init_destination;
	dst_mgr->dst_mgr.empty_output_buffer = _jpeg_empty_output_buffer;
	dst_mgr->dst_mgr.term_destination = _jpeg_term_destination;
	dst_mgr->im = im;
	dst_mgr->buf = malloc(65536);
	if (!dst_mgr->buf)
	  {
	     ok = 1;
	     im->error = 1;
	     goto done;
	  }
   	im->out.jinfo.dest = (struct jpeg_destination_mgr *)dst_mgr;
     }
   im->out.jinfo.image_width      = im->out.w;
   im->out.jinfo.image_height     = im->out.h;
   im->out.jinfo.input_components = im->in.jinfo.output_components;
   im->out.jinfo.in_color_space   = im->in.jinfo.out_color_space;
   im->out.jinfo.dct_method	  = im->in.jinfo.dct_method;
   jpeg_set_defaults(&(im->out.jinfo));
   jpeg_set_quality(&(im->out.jinfo), im->out.quality, TRUE);   
   
   if (im->out.quality >= 90)
     {
	im->out.jinfo.comp_info[0].h_samp_factor = 1;
	im->out.jinfo.comp_info[0].v_samp_factor = 1;
	im->out.jinfo.comp_info[1].h_samp_factor = 1;
	im->out.jinfo.comp_info[1].v_samp_factor = 1;
	im->out.jinfo.comp_info[2].h_samp_factor = 1;
	im->out.jinfo.comp_info[2].v_samp_factor = 1;
     }
   jpeg_start_compress(&(im->out.jinfo), TRUE);

   /* Set the image options for Exif */
   ExifData *exif = exif_data_new();
   exif_data_set_option(exif, EXIF_DATA_OPTION_FOLLOW_SPECIFICATION);
   exif_data_set_byte_order(exif, exif_byte_order);
   exif_data_fix(exif);
   /* Add Exif Orientation tag */
   if (im->in.orientation != 0) {
        ExifEntry *entry = init_tag(exif, EXIF_IFD_0, EXIF_TAG_ORIENTATION);
        exif_set_short(entry->data, exif_byte_order, im->in.orientation);
        exif_entry_unref(entry);
   }
   /* Write Exif data to output jpeg file */
   unsigned char *exif_data = NULL;
   unsigned int exif_data_len;
   exif_data_save_data(exif, &exif_data, &exif_data_len);
   jpeg_write_marker(&(im->out.jinfo), JPEG_APP0 + 1, exif_data, exif_data_len);
   exif_data_unref(exif);
   free(exif_data);

   /* Output comment if there is one */
   if (im->out.comment && *im->out.comment)
     jpeg_write_marker(&(im->out.jinfo), JPEG_COM, im->out.comment, strlen(im->out.comment));
   
   /* Output thumbnail info in APP7 */
   if (im->out.thumbnail_info)
     {
	char buf[8192];
	
	if (im->in.file)
	  {
	     snprintf(buf, sizeof(buf), "Thumb::URI\nfile://%s", im->in.file);
	     jpeg_write_marker(&(im->out.jinfo), JPEG_APP0 + 7, buf, strlen(buf));
	     snprintf(buf, sizeof(buf), "Thumb::MTime\n%llu", (unsigned long long int)im->stat_info.st_mtime);
	  }
	jpeg_write_marker(&(im->out.jinfo), JPEG_APP0 + 7, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Thumb::Image::Width\n%i", im->in.w);
	jpeg_write_marker(&(im->out.jinfo), JPEG_APP0 + 7, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Thumb::Image::Height\n%i", im->in.h);
	jpeg_write_marker(&(im->out.jinfo), JPEG_APP0 + 7, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "Thumb::Mimetype\nimage/jpeg");
	jpeg_write_marker(&(im->out.jinfo), JPEG_APP0 + 7, buf, strlen(buf));
     }
   
   while (im->out.jinfo.next_scanline < im->out.h)
     jpeg_write_scanlines(&(im->out.jinfo), &(im->lines[im->out.jinfo.next_scanline]), 1);
   jpeg_finish_compress(&(im->out.jinfo));

   done:
   if ((im->in.f) || (im->in.mem.data != NULL)) jpeg_destroy_decompress(&(im->in.jinfo));
   if ((im->in.f) && (im->in.file)) fclose(im->in.f);
   if (dst_mgr)
     {
	if (dst_mgr->buf) free(dst_mgr->buf);
	free(dst_mgr);
	im->out.jinfo.dest = NULL;
     }
   jpeg_destroy_compress(&(im->out.jinfo));
   if ((im->out.f) && (im->out.file)) fclose(im->out.f); 
   im->in.f = NULL;
   im->out.f = NULL;
   
   return ok;
}

static void 
_epeg_fatal_error_handler(j_common_ptr cinfo)
{
   emptr errmgr;
   
   errmgr = (emptr)cinfo->err;
   longjmp(errmgr->setjmp_buffer, 1);
   return;
}

/* Source manager methods */
METHODDEF(void)
_jpeg_decompress_error_exit(j_common_ptr cinfo)
{
}


METHODDEF(void)
_jpeg_init_source(j_decompress_ptr cinfo)
{
}

METHODDEF(boolean)
_jpeg_fill_input_buffer(j_decompress_ptr cinfo)
{
   WARNMS(cinfo, JWRN_JPEG_EOF);
   
   /* Insert a fake EOI marker */
   cinfo->src->next_input_byte = fake_EOI;
   cinfo->src->bytes_in_buffer = sizeof(fake_EOI);
   return TRUE;
}


METHODDEF(void)
_jpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
   if (num_bytes > (long)(cinfo)->src->bytes_in_buffer)
     ERREXIT(cinfo, 0);
   
   (cinfo)->src->next_input_byte += num_bytes;
   (cinfo)->src->bytes_in_buffer -= num_bytes;
}

METHODDEF(void)
_jpeg_term_source(j_decompress_ptr cinfo)
{
}


/* destination manager methods */
METHODDEF(void)
_jpeg_init_destination(j_compress_ptr cinfo)
{
   struct epeg_destination_mgr *dst_mgr;
   
   dst_mgr = (struct epeg_destination_mgr *)cinfo->dest;
   dst_mgr->dst_mgr.free_in_buffer = 65536;
   dst_mgr->dst_mgr.next_output_byte = (JOCTET *)dst_mgr->buf;
}

METHODDEF(boolean)
_jpeg_empty_output_buffer(j_compress_ptr cinfo)
{
   struct epeg_destination_mgr *dst_mgr;
   unsigned char *p;
   int psize;
   
   dst_mgr = (struct epeg_destination_mgr *)cinfo->dest;
   psize = *(dst_mgr->im->out.mem.size);
   *(dst_mgr->im->out.mem.size) += 65536;
   p = realloc(*(dst_mgr->im->out.mem.data), *(dst_mgr->im->out.mem.size));
   if (p)
     {
	*(dst_mgr->im->out.mem.data) = p;
	memcpy(p + psize, dst_mgr->buf, 65536);
	dst_mgr->dst_mgr.free_in_buffer = 65536;
	dst_mgr->dst_mgr.next_output_byte = (JOCTET *)dst_mgr->buf;
     }
   else
     return FALSE;
   return TRUE;
}

METHODDEF(void)
_jpeg_term_destination(j_compress_ptr cinfo)
{
   struct epeg_destination_mgr *dst_mgr;
   unsigned char *p;
   int psize;
   
   dst_mgr = (struct epeg_destination_mgr *)cinfo->dest;
   psize = *(dst_mgr->im->out.mem.size);
   *(dst_mgr->im->out.mem.size) += 65536 - dst_mgr->dst_mgr.free_in_buffer;
   p = realloc(*(dst_mgr->im->out.mem.data), *(dst_mgr->im->out.mem.size));
   if (p)
     {
	*(dst_mgr->im->out.mem.data) = p;
	memcpy(p + psize, dst_mgr->buf, 65536 - dst_mgr->dst_mgr.free_in_buffer);
     }
}

/* be noisy - not */
METHODDEF(void)
_emit_message(j_common_ptr cinfo, int msg_level)
{
}

METHODDEF(void)
_output_message(j_common_ptr cinfo)
{
}

METHODDEF(void)
_format_message(j_common_ptr cinfo, char * buffer)
{
}

