/****************************************************************************
#       spcav4l: v4l library.                           #
#This package work with the spca5xx based webcam with the raw jpeg feature. #
#All the decoding is in user space with the help of libjpeg.                #
#.                                                                          #
#       Copyright (C) 2003 2004 2005 Michel Xhaard                  #
#                                                                           #
# This program is free software; you can redistribute it and/or modify      #
# it under the terms of the GNU General Public License as published by      #
# the Free Software Foundation; either version 2 of the License, or         #
# (at your option) any later version.                                       #
#                                                                           #
# This program is distributed in the hope that it will be useful,           #
# but WITHOUT ANY WARRANTY; without even the implied warranty of            #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             #
# GNU General Public License for more details.                              #
#                                                                           #
# You should have received a copy of the GNU General Public License         #
# along with this program; if not, write to the Free Software               #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA #
#                                                                           #
****************************************************************************/

#include "spcaframe.h"
#include "spcav4l.h"
#include "utils.h"
#include "jdatatype.h"
#include "encoder.h"

static int init_v4l(struct vdIn *vd);
static int checkpalette(struct vdIn *vd);
static int check_palettesize(struct vdIn *vd);

static struct bridge_list Blist[] = {

    {BRIDGE_SPCA505, "SPCA505"},
    {BRIDGE_SPCA506, "SPCA506"},
    {BRIDGE_SPCA501, "SPCA501"},
    {BRIDGE_SPCA508, "SPCA508"},
    {BRIDGE_SPCA504, "SPCA504"},
    {BRIDGE_SPCA500, "SPCA500"},
    {BRIDGE_SPCA504B, "SPCA504B"},
    {BRIDGE_SPCA533, "SPCA533"},
    {BRIDGE_SPCA504C, "SPCA504C"},
    {BRIDGE_SPCA561, "SPCA561"},
    {BRIDGE_SPCA536, "SPCA536"},
    {BRIDGE_SONIX, "SN9C102"},
    {BRIDGE_ZC3XX, "ZC301-2"},
    {BRIDGE_CX11646, "CX11646"},
    {BRIDGE_TV8532, "TV8532"},
    {BRIDGE_ETOMS, "ET61XX51"},
    {BRIDGE_SN9CXXX, "SN9CXXX"},
    {BRIDGE_MR97311, "MR97311"},
    {BRIDGE_PAC207, "PAC207BCA"},
    {BRIDGE_VC0321, "VC0321"},
    {BRIDGE_VC0323, "VC0323"},
    {BRIDGE_PAC7311, "PAC7311"},
    {BRIDGE_UNKNOW, "UNKNOW"},
    { -1, NULL}
};


/****************************************************************************
*           Public
****************************************************************************/
int
init_videoIn(struct vdIn *vd, char *device, int width, int height,
             int format, int grabmethod)
{
    int err = -1;
    int i;
    if(vd == NULL || device == NULL)
        return -1;
    if(width == 0 || height == 0)
        return -1;
    if(grabmethod < 0 || grabmethod > 1)
        grabmethod = 1; //read by default;
    // check format
    vd->videodevice = NULL;
    vd->cameraname = NULL;
    vd->videodevice = NULL;
    vd->videodevice = (char *) realloc(vd->videodevice, 16);
    vd->cameraname = (char *) realloc(vd->cameraname, 32);
    snprintf(vd->videodevice, 12, "%s", device);
    printf("video %s \n", vd->videodevice);
    memset(vd->cameraname, 0, sizeof(vd->cameraname));
    memset(vd->bridge, 0, sizeof(vd->bridge));
    vd->signalquit = 1;
    vd->hdrwidth = width;
    vd->hdrheight = height;
    /*          compute the max frame size   */
    vd->formatIn = format;
    vd->bppIn = GetDepth(vd->formatIn);
    vd->grabMethod = grabmethod;      //mmap or read
    vd->pFramebuffer = NULL;
    /* init and check all setting */
    err = init_v4l(vd);
    /* allocate the 4 frames output buffer */
    for(i = 0; i < OUTFRMNUMB; i++) {
        vd->ptframe[i] = NULL;
        vd->ptframe[i] =
            (unsigned char *) realloc(vd->ptframe[i], sizeof(struct frame_t) + (size_t) vd->framesizeIn);
        vd->framelock[i] = 0;
    }
    vd->frame_cour = 0;


    pthread_mutex_init(&vd->grabmutex, NULL);
    return err;
}

int
close_v4l(struct vdIn *vd)
{
    int i;
    if(vd->grabMethod) {
        printf("unmapping frame buffer\n");
        munmap(vd->pFramebuffer, vd->mmapsize);
    } else {
        free(vd->pFramebuffer);
        vd->pFramebuffer = NULL;
    }
    printf("close video_device\n");
    close(vd->fd);
    /* dealloc the whole buffers */
    if(vd->videodevice) {
        free(vd->videodevice);
        vd->videodevice = NULL;
    }
    if(vd->cameraname) {
        free(vd->cameraname);
        vd->cameraname = NULL;
    }
    for(i = 0; i < OUTFRMNUMB; i++) {
        if(vd->ptframe[i]) {
            free(vd->ptframe[i]);
            vd->ptframe[i] = NULL;
            vd->framelock[i] = 0;
            printf("freeing output buffer %d\n", i);
        }
    }
    pthread_mutex_destroy(&vd->grabmutex);
    return 0;
}

int
convertframe(unsigned char *dst, unsigned char *src, int width, int height, int formatIn, int qualite)
{
    int jpegsize = 0;
    switch(formatIn) {
    case VIDEO_PALETTE_JPEG:
        jpegsize = get_jpegsize(src, width * height);
        if(jpegsize > 0)
            memcpy(dst, src, jpegsize);
        break;
    case VIDEO_PALETTE_YUV420P:
        jpegsize = encode_image(src, dst, qualite, YUVto420, width, height);
        break;
    case VIDEO_PALETTE_RGB24:
        jpegsize = encode_image(src, dst, qualite, RGBto420, width, height);
        break;
    case VIDEO_PALETTE_RGB565:
        jpegsize = encode_image(src, dst, qualite, RGB565to420, width, height);
        break;
    case VIDEO_PALETTE_RGB32:
        jpegsize = encode_image(src, dst, qualite, RGB32to420, width, height);
        break;
    default:
        break;
    }
    return jpegsize;
}

int
v4lGrab(struct vdIn *vd)
{
    static  int frame = 0;

    int len;
    int size;
    int erreur = 0;
    int jpegsize = 0;
    int qualite = 1024;
    struct frame_t *headerframe;
    double timecourant = 0;
    double temps = 0;
    timecourant = ms_time();
    if(vd->grabMethod) {

        vd->vmmap.height = vd->hdrheight;
        vd->vmmap.width = vd->hdrwidth;
        vd->vmmap.format = vd->formatIn;
        if(ioctl(vd->fd, VIDIOCSYNC, &vd->vmmap.frame) < 0) {
            perror("cvsync err\n");
            erreur = -1;
        }

        /* Is there someone using the frame */
        while((vd->framelock[vd->frame_cour] != 0) && vd->signalquit)
            usleep(1000);
        pthread_mutex_lock(&vd->grabmutex);

        /*
        memcpy (vd->ptframe[vd->frame_cour]+ sizeof(struct frame_t), vd->pFramebuffer +
              vd->videombuf.offsets[vd->vmmap.frame] , vd->framesizeIn);
         jpegsize =jpeg_compress(vd->ptframe[vd->frame_cour]+ sizeof(struct frame_t),vd->framesizeIn,
         vd->pFramebuffer + vd->videombuf.offsets[vd->vmmap.frame] ,vd->hdrwidth, vd->hdrheight, qualite);
         */
        temps = ms_time();
        jpegsize = convertframe(vd->ptframe[vd->frame_cour] + sizeof(struct frame_t),
                                vd->pFramebuffer + vd->videombuf.offsets[vd->vmmap.frame],
                                vd->hdrwidth, vd->hdrheight, vd->formatIn, qualite);

        headerframe = (struct frame_t*)vd->ptframe[vd->frame_cour];
        snprintf(headerframe->header, 5, "%s", "SPCA");
        headerframe->seqtimes = ms_time();
        headerframe->deltatimes = (int)(headerframe->seqtimes - timecourant);
        headerframe->w = vd->hdrwidth;
        headerframe->h = vd->hdrheight;
        headerframe->size = ((jpegsize < 0) ? 0 : jpegsize);
        headerframe->format = vd->formatIn;
        headerframe->nbframe = frame++;

        // printf("compress frame %d times %f\n",frame, headerframe->seqtimes-temps);
        pthread_mutex_unlock(&vd->grabmutex);
        /************************************/

        if((ioctl(vd->fd, VIDIOCMCAPTURE, &(vd->vmmap))) < 0) {
            perror("cmcapture");
            erreur = -1;
        }
        vd->vmmap.frame = (vd->vmmap.frame + 1) % vd->videombuf.frames;
        vd->frame_cour = (vd->frame_cour + 1) % OUTFRMNUMB;
        //printf("frame nb %d\n",vd->vmmap.frame);

    } else {
        /* read method */
        size = vd->framesizeIn;
        len = read(vd->fd, vd->pFramebuffer, size);
        if(len <= 0) {
            printf("v4l read error\n");
            printf("len %d asked %d \n", len, size);
            return 0;
        }
        /* Is there someone using the frame */
        while((vd->framelock[vd->frame_cour] != 0) && vd->signalquit)
            usleep(1000);
        pthread_mutex_lock(&vd->grabmutex);
        /*
         memcpy (vd->ptframe[vd->frame_cour]+ sizeof(struct frame_t), vd->pFramebuffer, vd->framesizeIn);
         jpegsize =jpeg_compress(vd->ptframe[vd->frame_cour]+ sizeof(struct frame_t),len,
         vd->pFramebuffer, vd->hdrwidth, vd->hdrheight, qualite);
         */
        temps = ms_time();
        jpegsize = convertframe(vd->ptframe[vd->frame_cour] + sizeof(struct frame_t),
                                vd->pFramebuffer ,
                                vd->hdrwidth, vd->hdrheight, vd->formatIn, qualite);
        headerframe = (struct frame_t*)vd->ptframe[vd->frame_cour];
        snprintf(headerframe->header, 5, "%s", "SPCA");
        headerframe->seqtimes = ms_time();
        headerframe->deltatimes = (int)(headerframe->seqtimes - timecourant);
        headerframe->w = vd->hdrwidth;
        headerframe->h = vd->hdrheight;
        headerframe->size = ((jpegsize < 0) ? 0 : jpegsize);;
        headerframe->format = vd->formatIn;
        headerframe->nbframe = frame++;
        //  printf("compress frame %d times %f\n",frame, headerframe->seqtimes-temps);
        vd->frame_cour = (vd->frame_cour + 1) % OUTFRMNUMB;
        pthread_mutex_unlock(&vd->grabmutex);
        /************************************/

    }
    return erreur;
}

/*****************************************************************************
*               Private
******************************************************************************/
static int
GetVideoPict(struct vdIn *vd)
{
    if(ioctl(vd->fd, VIDIOCGPICT, &vd->videopict) < 0)
        exit_fatal("Couldnt get videopict params with VIDIOCGPICT");


    printf("VIDIOCGPICT brightnes=%d hue=%d color=%d contrast=%d whiteness=%d"
           "depth=%d palette=%d\n", vd->videopict.brightness,
           vd->videopict.hue, vd->videopict.colour, vd->videopict.contrast,
           vd->videopict.whiteness, vd->videopict.depth,
           vd->videopict.palette);

    return 0;
}

static int
SetVideoPict(struct vdIn *vd)
{
    if(ioctl(vd->fd, VIDIOCSPICT, &vd->videopict) < 0)
        exit_fatal("Couldnt set videopict params with VIDIOCSPICT");

    printf("VIDIOCSPICT brightnes=%d hue=%d color=%d contrast=%d whiteness=%d"
           "depth=%d palette=%d\n", vd->videopict.brightness,
           vd->videopict.hue, vd->videopict.colour, vd->videopict.contrast,
           vd->videopict.whiteness, vd->videopict.depth,
           vd->videopict.palette);

    return 0;
}
static void spcaPrintParam(int fd, struct video_param *videoparam);
static void spcaSetTimeInterval(int fd, struct video_param *videoparam, unsigned short time);
static void spcaSetQuality(int fd, struct video_param *videoparam, unsigned char index);


static int
init_v4l(struct vdIn *vd)
{
    int f;
    int erreur = 0;
    int err;
    if((vd->fd = open(vd->videodevice, O_RDWR)) == -1)
        exit_fatal("ERROR opening V4L interface");

    if(ioctl(vd->fd, VIDIOCGCAP, &(vd->videocap)) == -1)
        exit_fatal("Couldn't get videodevice capability");

    printf("Camera found: %s \n", vd->videocap.name);
    snprintf(vd->cameraname, 32, "%s", vd->videocap.name);

    erreur = GetVideoPict(vd);
    if(ioctl(vd->fd, VIDIOCGCHAN, &vd->videochan) == -1) {
        printf("Hmm did not support Video_channel\n");
        vd->cameratype = UNOW;
    } else {
        if(vd->videochan.name) {
            printf("Bridge found: %s \n", vd->videochan.name);
            snprintf(vd->bridge, 9, "%s", vd->videochan.name);
            vd->cameratype = GetStreamId(vd->videochan.name);
            spcaPrintParam(vd->fd, &vd->videoparam);
        } else {
            printf("Bridge not found not a spca5xx Webcam Probing the hardware !!\n");
            vd->cameratype = UNOW;
        }
    }
    printf("StreamId: %d  Camera\n", vd->cameratype);
    /* probe all available palette and size */
    if(probePalette(vd) < 0) {
        exit_fatal("could't probe video palette Abort !");
    }
    if(probeSize(vd) < 0) {
        exit_fatal("could't probe video size Abort !");
    }

    /* now check if the needed setting match the available
        if not find a new set and populate the change */
    err = check_palettesize(vd);
    printf(" Format asked %d check %d\n", vd->formatIn, err);
    vd->videopict.palette = vd->formatIn;
    vd->videopict.depth = GetDepth(vd->formatIn);
    vd->bppIn = GetDepth(vd->formatIn);

    vd->framesizeIn = (vd->hdrwidth * vd->hdrheight * vd->bppIn) >> 3;

    erreur = SetVideoPict(vd);
    erreur = GetVideoPict(vd);
    if(vd->formatIn != vd->videopict.palette ||
            vd->bppIn != vd->videopict.depth)
        exit_fatal("could't set video palette Abort !");
    if(erreur < 0)
        exit_fatal("could't set video palette Abort !");

    if(vd->grabMethod) {
        printf(" grabbing method default MMAP asked \n");
        // MMAP VIDEO acquisition
        memset(&(vd->videombuf), 0, sizeof(vd->videombuf));
        if(ioctl(vd->fd, VIDIOCGMBUF, &(vd->videombuf)) < 0) {
            perror(" init VIDIOCGMBUF FAILED\n");
        }
        printf("VIDIOCGMBUF size %d  frames %d  offets[0]=%d offsets[1]=%d\n",
               vd->videombuf.size, vd->videombuf.frames,
               vd->videombuf.offsets[0], vd->videombuf.offsets[1]);
        vd->pFramebuffer =
            (unsigned char *) mmap(0, vd->videombuf.size, PROT_READ | PROT_WRITE,
                                   MAP_SHARED, vd->fd, 0);
        vd->mmapsize = vd->videombuf.size;
        vd->vmmap.height = vd->hdrheight;
        vd->vmmap.width = vd->hdrwidth;
        vd->vmmap.format = vd->formatIn;
        for(f = 0; f < vd->videombuf.frames; f++) {
            vd->vmmap.frame = f;
            if(ioctl(vd->fd, VIDIOCMCAPTURE, &(vd->vmmap))) {
                perror("cmcapture");
            }
        }
        vd->vmmap.frame = 0;
    } else {
        /* read method */
        /* allocate the read buffer */
        vd->pFramebuffer =
            (unsigned char *) realloc(vd->pFramebuffer, (size_t) vd->framesizeIn);
        printf(" grabbing method READ asked \n");
        if(ioctl(vd->fd, VIDIOCGWIN, &(vd->videowin)) < 0)
            perror("VIDIOCGWIN failed \n");
        vd->videowin.height = vd->hdrheight;
        vd->videowin.width = vd->hdrwidth;
        if(ioctl(vd->fd, VIDIOCSWIN, &(vd->videowin)) < 0)
            perror("VIDIOCSWIN failed \n");
        printf("VIDIOCSWIN height %d  width %d \n",
               vd->videowin.height, vd->videowin.width);
    }
    vd->frame_cour = 0;
    return erreur;
}

int probePalette(struct vdIn *vd)
{
    /* probe palette and set a default one for unknow cams*/
    int pal[] = {VIDEO_PALETTE_JPEG, VIDEO_PALETTE_YUV420P, VIDEO_PALETTE_RGB24, VIDEO_PALETTE_RGB565, VIDEO_PALETTE_RGB32};
    struct video_picture pict;
    int masq = 0x1;
    int i;
    int availpal = 0;
    int defaut = 1;
    /* initialize the internal struct */
    if(ioctl(vd->fd, VIDIOCGPICT, &pict) < 0) {
        perror("Couldnt get videopict params with VIDIOCGPICT\n");
        return -1;
    }
    /* try each palette we have we skip raw_jpeg */
    for(i = 0; i < 5 ; i++) {
        pict.palette = pal[i];
        /* maybe correct the bug on qca driver depth always 24 ? */
        pict.depth = GetDepth(pal[i]);
        printf("try palette %d depth %d\n", pict.palette, pict.depth);
        if(ioctl(vd->fd, VIDIOCSPICT, &pict) < 0) {
            printf("Couldnt set palette first try %d \n", pal[i]);

        }
        if(ioctl(vd->fd, VIDIOCGPICT, &pict) < 0) {
            printf("Couldnt get palette %d \n", pal[i]);

        }
        if(pict.palette != pal[i]) {
            printf("Damned second try fail \n");
        } else {
            availpal = availpal | masq ;
            printf("Available  palette %d \n", pal[i]);
            if(defaut) {
                defaut = 0;
                //vd->formatIn = pal[i];
                // vd->bppIn = GetDepth (pal[i]);
            }
        }

        masq = masq << 1;
    }
    vd->palette = availpal;
    //should set default palette here ?
    return 1;
}

int probeSize(struct vdIn *vd)
{
    /* probe size and set a default one for unknow cams */
    int size[] = { 640, 480, 384, 288, 352, 288, 320, 240, 192, 144, 176, 144, 160, 120 };
    struct video_window win;
    int maxw, minw, maxh, minh;
    int masq = 0x1;
    int i = 0;
    int defaut = 1 ;
    /* initialize de parameters */
    maxw = vd->videocap.maxwidth;
    minw = vd->videocap.minwidth;
    maxh = vd->videocap.maxheight;
    minh = vd->videocap.minheight;
    printf("probe size in \n");
    while(size[i] > maxw) {
        printf("skip size %d x %d\n", size[i], size[i+1]);
        i += 2;
        masq = masq << 1;
        if(i > 13) break;
    }
    /* initialize the internal struct */
    if(ioctl(vd->fd, VIDIOCGWIN, &win) < 0) {
        perror("VIDIOCGWIN failed \n");
        return -1;
    }
    /* now i is on the first possible width */
    while((size[i] >= minw) && i < 13) {
        win.width = size[i];
        win.height = size[i+1];
        if(ioctl(vd->fd, VIDIOCSWIN, &win) < 0) {
            printf("VIDIOCSWIN reject width %d  height %d \n",
                   win.width, win.height);
        } else {
            vd->sizeothers = vd->sizeothers | masq;
            printf("Available Resolutions width %d  heigth %d \n",
                   win.width, win.height);
            if(defaut) {

                // vd->hdrwidth = win.width;
                // vd->hdrheight = win.height;
                defaut = 0;
            }
        }
        masq = masq << 1 ;
        i += 2;
    }

    return 1;
}

int
changeSize(struct vdIn *vd)
{
    int erreur;
    erreur = GetVideoPict(vd);
    vd->formatIn = vd->videopict.palette;
    vd->bppIn = vd->videopict.depth;
    /* To Compute the estimate frame size perhaps not need !!! */
    if((vd->bppIn = GetDepth(vd->formatIn)) < 0) {
        perror("getdepth  failed \n");
        exit(1);
    }
    if(vd->grabMethod) {
        vd->vmmap.height = vd->hdrheight;
        vd->vmmap.width = vd->hdrwidth;
        vd->vmmap.format = vd->formatIn;

    } else {

        if(ioctl(vd->fd, VIDIOCGWIN, &vd->videowin) < 0)
            perror("VIDIOCGWIN failed \n");
        vd->videowin.height = vd->hdrheight;
        vd->videowin.width = vd->hdrwidth;
        if(ioctl(vd->fd, VIDIOCSWIN, &vd->videowin) < 0)
            perror("VIDIOCSWIN failed \n");
        printf("VIDIOCGWIN height %d  width %d \n",
               vd->videowin.height, vd->videowin.width);
    }
    vd->framesizeIn = ((vd->hdrwidth * vd->hdrheight * vd->bppIn) >> 3);
//  vd->pixTmp =
//    (unsigned char *) realloc (vd->pixTmp, (size_t) vd->framesizeIn);
    return 0;
}
/* return masq byte for the needed size */
static int convertsize(int width, int height)
{
    switch(width) {
    case 640:
        if(height == 480)
            return VGA;
        break;
    case 384:
        if(height == 288)
            return PAL;
        break;
    case 352:
        if(height == 288)
            return SIF;
        break;
    case 320:
        if(height == 240)
            return CIF;
        break;
    case 192:
        if(height == 144)
            return QPAL;
        break;
    case 176:
        if(height == 144)
            return QSIF;
        break;
    case 160:
        if(height == 120)
            return QCIF;
        break;
    default:
        break;
    }
    return -1;
}
static int sizeconvert(int *width, int *height, int size)
{
    switch(size) {
    case VGA:
        *height = 480;
        *width = 640;
        break;
    case PAL:
        *height = 288;
        *width = 384;
        break;
    case SIF:
        *height = 288;
        *width = 352;
        break;
    case CIF:
        *height = 240;
        *width = 320;
        break;
    case QPAL:
        *height = 144;
        *width = 192;
        break;
    case QSIF:
        *height = 144;
        *width = 176;
        break;
    case QCIF:
        *height = 120;
        *width = 160;
        break;
    default:
        return -1;
        break;
    }
    return 0;
}
static int convertpalette(int palette)
{
    switch(palette) {
    case VIDEO_PALETTE_JPEG:
        return JPG;
        break;
    case VIDEO_PALETTE_YUV420P:
        return YUV420P;
        break;
    case VIDEO_PALETTE_RGB24:
        return RGB24;
        break;
    case VIDEO_PALETTE_RGB565:
        return RGB565;
        break;
    case VIDEO_PALETTE_RGB32:
        return RGB32;
        break;
    default:
        break;
    }
    return -1;
}
static int paletteconvert(int code)
{
    switch(code) {
    case JPG:
        return VIDEO_PALETTE_JPEG;
        break;
    case YUV420P:
        return VIDEO_PALETTE_YUV420P;
        break;
    case RGB24:
        return VIDEO_PALETTE_RGB24;
        break;
    case RGB565:
        return VIDEO_PALETTE_RGB565;
        break;
    case RGB32:
        return VIDEO_PALETTE_RGB32;
        break;
    default:
        return -1;
        break;
    }
    return 0;
}
/*check palette not so easy try VIDIOCMCAPTURE IOCTL the only one with palette and size */
/* some drivers may fail here  */
static int checkpalette(struct vdIn *vd)
{
    unsigned char masq = 0x00;
    int needpalette = 0;
    int palette = 0;

    needpalette = convertpalette(vd->formatIn);
    /* is the palette available */
    if(!(vd->palette & needpalette)) {
        /* find an other one */
        if(needpalette > 1) {
            masq = needpalette - 1;
        }
        if((masq & vd->palette) > 1) {
            /* check lower masq upper size */
            while(!((needpalette = needpalette >> 1) & vd->palette) && needpalette);
        } else if((masq & vd->palette) == 0) {
            masq = 0xff - (needpalette << 1) + 1;
            if((masq & vd->palette) == 0) {
                /* no more size available */
                needpalette = 0;
            } else {
                /* check upper masq */
                while(!((needpalette = needpalette << 1) & vd->palette) && needpalette);
            }
        } // maybe == 1
    }

    if((palette = paletteconvert(needpalette)) < 0) {
        printf("Invalid palette in check palette fatal !! \n");
        return -1;
    }

    if(palette) {
        vd->vmmap.height = vd->hdrheight;
        vd->vmmap.width = vd->hdrwidth;
        vd->vmmap.format = palette;

        vd->vmmap.frame = 0;
        if(ioctl(vd->fd, VIDIOCMCAPTURE, &(vd->vmmap))) {
            perror("cmcapture");
            return -1;
        }
    } else {
        printf("palette not find check palette fatal !! \n");
        return -1;
    }
    /*populate the change */
    vd->formatIn = palette;
    return palette;

}
/* test is palette and size are available otherwhise return the next available palette and size
palette is set by preference order jpeg yuv420p rbg24 rgb565 and rgb32 */
static int check_palettesize(struct vdIn *vd)
{
    int needsize = 0;
    int needpalette = 0;
    unsigned char masq = 0x00;
    /* initialize needed size */
    if((needsize = convertsize(vd->hdrwidth, vd->hdrheight)) < 0) {
        printf("size seem unavailable fatal errors !!\n");
        return -1;
    }
    /* is there a match with available palette */
    /* check */
    if(!(vd->sizeothers & needsize)) {
        if(needsize > 1) {
            masq = needsize - 1;
        }
        if((masq & vd->sizeothers) > 1) {
            /* check lower masq upper size */
            while(!((needsize = needsize >> 1) & vd->sizeothers) && needsize);
        } else if((masq & vd->sizeothers) == 0) {
            masq = 0xff - (needsize << 1) + 1;
            if((masq & vd->sizeothers) == 0) {
                /* no more size available */
                needsize = 0;
            } else {
                /* check upper masq */
                while(!((needsize = needsize << 1) & vd->sizeothers) && needsize);
            }
        } // maybe == 1

    }
    if(needsize) {
        /* set the size now check for a palette */
        if(sizeconvert(&vd->hdrwidth, &vd->hdrheight, needsize) > 0) {
            printf("size not set fatal errors !!\n");
            return -1;
        }
        if((needpalette = checkpalette(vd) < 0)) {
            return -1;
        }
    } else {
        printf("Damned no match found Fatal errors !!\n");
        return -1;
    }
    return needsize;
}

int
isSpcaChip(const char *BridgeName)
{
    int i = -1;
    int find = -1;
    int size = 0;

    /* Spca506 return more with channel video, cut it */

    /* return Bridge otherwhise -1 */
    for(i = 0; i < MAX_BRIDGE - 1; i++) {
        size = strlen(Blist[i].name) ;
        // printf ("is_spca %s \n",Blist[i].name);
        if(strncmp(BridgeName, Blist[i].name, size) == 0) {
            find = i;
            break;
        }
    }

    return find;
}

int
GetStreamId(const char *BridgeName)
{
    int i = -1;
    int match = -1;
    /* return Stream_id otherwhise -1 */
    if((match = isSpcaChip(BridgeName)) < 0) {
        printf("Not an Spca5xx Camera !!\n");
        return match;
    }
    switch(match) {
    case BRIDGE_SPCA505:
    case BRIDGE_SPCA506:
        i = YYUV;
        break;
    case BRIDGE_SPCA501:
    case BRIDGE_VC0321:
        i = YUYV;
        break;
    case BRIDGE_SPCA508:
        i = YUVY;
        break;
    case BRIDGE_SPCA536:
    case BRIDGE_SPCA504:
    case BRIDGE_SPCA500:
    case BRIDGE_SPCA504B:
    case BRIDGE_SPCA533:
    case BRIDGE_SPCA504C:
    case BRIDGE_ZC3XX:
    case BRIDGE_CX11646:
    case BRIDGE_SN9CXXX:
    case BRIDGE_MR97311:
    case BRIDGE_VC0323:
    case BRIDGE_PAC7311:
        i = JPEG;
        break;
    case BRIDGE_ETOMS:
    case BRIDGE_SONIX:
    case BRIDGE_SPCA561:
    case BRIDGE_TV8532:
    case BRIDGE_PAC207:
        i = GBRG;
        break;
    default:
        i = UNOW; // -1;
        printf("Unable to find a StreamId !!\n");
        break;

    }
    return i;
}

int
GetDepth(int format)
{
    int depth;
    switch(format) {

    case VIDEO_PALETTE_JPEG: {
        depth = 8;
    }
    break;
    case VIDEO_PALETTE_RAW: {
        depth = 8;
    }
    break;
    case VIDEO_PALETTE_YUV420P: {
        depth = (8 * 3) >> 1;
    }
    break;
    case VIDEO_PALETTE_RGB565:
        depth = 16;
        break;
    case VIDEO_PALETTE_RGB24:
        depth = 24;
        break;
    case VIDEO_PALETTE_RGB32: {
        depth = 32;
    }
    break;
    default:
        depth = -1;
        break;
    }
    return depth;
}

__u8
SpcaGetBrightness(struct vdIn * vdin)
{
    if(GetVideoPict(vdin) < 0) {
        printf(" Error getBrightness \n");
        return 0;
    }
    return ((vdin->videopict.brightness) >> 8);
}

void
SpcaSetBrightness(struct vdIn *vdin, __u8 bright)
{
    vdin->videopict.brightness = bright << 8;
    if(SetVideoPict(vdin) < 0) {
        printf(" Error setBrightness \n");
    }

}
__u8
SpcaGetContrast(struct vdIn *vdin)
{
    if(GetVideoPict(vdin) < 0) {
        printf(" Error getContrast \n");
        return 0;
    }
    return ((vdin->videopict.contrast) >> 8);
}

void
SpcaSetContrast(struct vdIn *vdin, __u8 contrast)
{
    vdin->videopict.contrast = contrast << 8;
    if(SetVideoPict(vdin) < 0) {
        printf(" Error setContrast \n");
    }
}
__u8
SpcaGetColors(struct vdIn *vdin)
{
    if(GetVideoPict(vdin) < 0) {
        printf(" Error getColors \n");
        return 0;
    }
    return ((vdin->videopict.colour) >> 8);
}

void
SpcaSetColors(struct vdIn *vdin, __u8 colors)
{
    vdin->videopict.colour = colors << 8;
    if(SetVideoPict(vdin) < 0) {
        printf(" Error setColors \n");
    }
}
/* we assume that struct videopict is initialized */
unsigned short upbright(struct vdIn *vdin)
{
    unsigned short bright = 0;

    bright = vdin->videopict.brightness;
    if((bright + 0x200) < 0xffff) {
        bright += 0x200;
        vdin->videopict.brightness = bright;
        if(SetVideoPict(vdin) < 0) {
            printf(" Error setVideopict \n");
            return 0;
        }
    }
    return bright;
}
unsigned short downbright(struct vdIn *vdin)
{
    unsigned short bright = 0;

    bright = vdin->videopict.brightness;
    if((bright - 0x200) > 0) {
        bright -= 0x200;
        vdin->videopict.brightness = bright;
        if(SetVideoPict(vdin) < 0) {
            printf(" Error setVideopict \n");
            return 0;
        }
    }
    return bright;
}
unsigned short upcontrast(struct vdIn *vdin)
{
    unsigned short contrast = 0;

    contrast = vdin->videopict.contrast;
    if((contrast + 0x200) < 0xffff) {
        contrast += 0x200;
        vdin->videopict.contrast = contrast;
        if(SetVideoPict(vdin) < 0) {
            printf(" Error setVideopict \n");
            return 0;
        }
    }
    return contrast;
}
unsigned short downcontrast(struct vdIn *vdin)
{
    unsigned short contrast = 0;

    contrast = vdin->videopict.contrast;
    if((contrast - 0x200) > 0) {
        contrast -= 0x200;
        vdin->videopict.contrast = contrast;
        if(SetVideoPict(vdin) < 0) {
            printf(" Error setVideopict \n");
            return 0;
        }
    }
    return contrast;
}
void
qualityUp(struct vdIn *vdin)
{
    struct video_param *videoparam = &vdin->videoparam;
    int fd = vdin->fd;
    unsigned char index = videoparam->quality;
    index += 1;
    spcaSetQuality(fd, videoparam, index);
}
void
qualityDown(struct vdIn *vdin)
{
    struct video_param *videoparam = &vdin->videoparam;
    int fd = vdin->fd;
    unsigned char index = videoparam->quality;
    if(index > 0) index--;
    spcaSetQuality(fd, videoparam, index);
}
void
timeUp(struct vdIn *vdin)
{
    struct video_param *videoparam = &vdin->videoparam;
    int fd = vdin->fd;
    unsigned short index = videoparam->time_interval;
    index += 10;
    spcaSetTimeInterval(fd, videoparam, index);
}
void
timeDown(struct vdIn *vdin)
{
    struct video_param *videoparam = &vdin->videoparam;
    int fd = vdin->fd;
    unsigned short index = videoparam->time_interval;
    if(index > 0) index -= 10;
    spcaSetTimeInterval(fd, videoparam, index);
}
void
spcaSetAutoExpo(struct vdIn *vdin)
{
    struct video_param *videoparam = &vdin->videoparam;
    int fd = vdin->fd;
    videoparam->chg_para = CHGABRIGHT;
    videoparam->autobright = !videoparam->autobright;
    if(ioctl(fd, SPCASVIDIOPARAM, videoparam) == -1) {
        printf("autobright error !!\n");
    } else
        spcaPrintParam(fd, videoparam);

}
static void spcaPrintParam(int fd, struct video_param *videoparam)
{
    if(ioctl(fd, SPCAGVIDIOPARAM, videoparam) == -1) {
        printf("wrong spca5xx device\n");
    } else
        printf("quality %d autoexpo %d Timeframe %d \n",
               videoparam->quality, videoparam->autobright, videoparam->time_interval);
}

static void spcaSetTimeInterval(int fd, struct video_param *videoparam, unsigned short time)
{
    if(time < 1000) {
        videoparam->chg_para = CHGTINTER;
        videoparam->time_interval = time;
        if(ioctl(fd, SPCASVIDIOPARAM, videoparam) == -1) {
            printf("frame_times error !!\n");
        } else
            spcaPrintParam(fd, videoparam);
    }

}
static void spcaSetQuality(int fd, struct video_param *videoparam, unsigned char index)
{
    if(index < 6) {
        videoparam->chg_para = CHGQUALITY;
        videoparam->quality = index;
        if(ioctl(fd, SPCASVIDIOPARAM, videoparam) == -1) {
            printf("quality error !!\n");
        } else
            spcaPrintParam(fd, videoparam);
    }
}
