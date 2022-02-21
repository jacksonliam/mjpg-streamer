#ifndef JPEG_UTILS_H
#define JPEG_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

int compress_image_to_jpeg(struct vdIn *vd, unsigned char *buffer, int size, int quality);

#ifdef __cplusplus
}
#endif

#endif