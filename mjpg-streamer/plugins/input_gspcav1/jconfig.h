#ifndef JCONFIG_H
#define JCONFIG_H

#define IMAGE_WIDTH 320 
#define IMAGE_HEIGHT 240

#define VIDEO_PALETTE_RAW_JPEG  20
#define VIDEO_PALETTE_JPEG  21
#define SWAP_RGB 1
int bpp;
/* our own ioctl */
struct video_param {
	int chg_para;
#define CHGABRIGHT 1
#define CHGQUALITY 2
#define CHGLIGHTFREQ 3
#define CHGTINTER  4
	__u8 autobright;
	__u8 quality;
	__u16 time_interval;
	__u8 light_freq;
};

/* Our private ioctl */
#define SPCAGVIDIOPARAM _IOR('v',BASE_VIDIOCPRIVATE + 1,struct video_param)
#define SPCASVIDIOPARAM _IOW('v',BASE_VIDIOCPRIVATE + 2,struct video_param)

/* Camera type jpeg yuvy yyuv yuyv grey gbrg*/
enum {  
	JPEG = 0,
	YUVY,
	YYUV,
	YUYV,
	GREY,
	GBRG,
	UNOW,
};

enum {
	BRIDGE_SPCA505 = 0,
        BRIDGE_SPCA506,
	BRIDGE_SPCA501,
	BRIDGE_SPCA508,
	BRIDGE_SPCA504,
	BRIDGE_SPCA500,
	BRIDGE_SPCA504B,
	BRIDGE_SPCA533,
	BRIDGE_SPCA504C,
	BRIDGE_SPCA561,
	BRIDGE_SPCA536,
	BRIDGE_SONIX,
	BRIDGE_ZC3XX,
	BRIDGE_CX11646,
	BRIDGE_TV8532,
	BRIDGE_ETOMS,
	BRIDGE_SN9CXXX,
	BRIDGE_MR97311,
	BRIDGE_PAC207,
	BRIDGE_VC0321,
	BRIDGE_VC0323,
	BRIDGE_PAC7311,
	BRIDGE_UNKNOW,
	MAX_BRIDGE,
};
struct palette_list {
	int num;
	const char *name;
};

struct bridge_list {
	int num;
	const char *name;
};


#endif /* JCONFIG_H */
