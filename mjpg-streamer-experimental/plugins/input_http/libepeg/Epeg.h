#ifndef _EPEG_H
#define _EPEG_H

#ifdef EAPI
#undef EAPI
#endif
#ifdef WIN32
# ifdef BUILDING_DLL
#  define EAPI __declspec(dllexport)
# else
#  define EAPI __declspec(dllimport)
# endif
#else
# ifdef __GNUC__
#  if __GNUC__ >= 4
#   define EAPI __attribute__ ((visibility("default")))
#  else
#   define EAPI
#  endif
# else
#  define EAPI
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

   typedef enum _Epeg_Colorspace
     {
	EPEG_GRAY8,
	  EPEG_YUV8,
	  EPEG_RGB8,
	  EPEG_BGR8,
	  EPEG_RGBA8,
	  EPEG_BGRA8,
	  EPEG_ARGB32,
	  EPEG_CMYK
     }
   Epeg_Colorspace;
   
   typedef struct _Epeg_Image          Epeg_Image;
   typedef struct _Epeg_Thumbnail_Info Epeg_Thumbnail_Info;

   struct _Epeg_Thumbnail_Info
     {
	char                   *uri;
	unsigned long long int  mtime;
	int                     w, h;
	char                   *mimetype;
     };
   
   EAPI Epeg_Image   *epeg_file_open                 (const char *file);
   EAPI Epeg_Image   *epeg_memory_open               (unsigned char *data, int size);
   EAPI void          epeg_size_get                  (Epeg_Image *im, int *w, int *h);
   EAPI void          epeg_decode_size_set           (Epeg_Image *im, int w, int h);
   EAPI void          epeg_colorspace_get            (Epeg_Image *im, int *space);
   EAPI void          epeg_decode_colorspace_set     (Epeg_Image *im, Epeg_Colorspace colorspace);
   EAPI const void   *epeg_pixels_get                (Epeg_Image *im, int x, int y, int w, int h);
   EAPI void          epeg_pixels_free               (Epeg_Image *im, const void *data);
   EAPI const char   *epeg_comment_get               (Epeg_Image *im);
   EAPI void          epeg_thumbnail_comments_get    (Epeg_Image *im, Epeg_Thumbnail_Info *info);
   EAPI void          epeg_comment_set               (Epeg_Image *im, const char *comment);
   EAPI void          epeg_quality_set               (Epeg_Image *im, int quality);
   EAPI void          epeg_thumbnail_comments_enable (Epeg_Image *im, int onoff);
   EAPI void          epeg_file_output_set           (Epeg_Image *im, const char *file);
   EAPI void          epeg_memory_output_set         (Epeg_Image *im, unsigned char **data, int *size);
   EAPI int           epeg_encode                    (Epeg_Image *im);
   EAPI int           epeg_trim                      (Epeg_Image *im);
   EAPI void          epeg_close                     (Epeg_Image *im);
   
#ifdef __cplusplus
}
#endif

#endif
