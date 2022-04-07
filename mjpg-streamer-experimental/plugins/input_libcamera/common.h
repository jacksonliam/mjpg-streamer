#ifndef COMMON_H_
#define COMMON_H_

struct vdIn {
    unsigned char *framebuffer;
    int width;
    int height;
    int formatIn;
    int stride;
};

#endif