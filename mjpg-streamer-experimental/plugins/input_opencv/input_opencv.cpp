/*******************************************************************************
#                                                                              #
# OpenCV input plugin                                                          #
# Copyright (C) 2016 Dustin Spicuzza                                           #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
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
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <dlfcn.h>
#include <pthread.h>

#include "input_opencv.h"

#include "opencv2/opencv.hpp"
#include "opencv2/highgui/highgui.hpp"

#if CV_MAJOR_VERSION<3
enum VideoCaptureProperties {
       CAP_PROP_POS_MSEC       =0, //!< Current position of the video file in milliseconds.
       CAP_PROP_POS_FRAMES     =1, //!< 0-based index of the frame to be decoded/captured next.
       CAP_PROP_POS_AVI_RATIO  =2, //!< Relative position of the video file: 0=start of the film, 1=end of the film.
       CAP_PROP_FRAME_WIDTH    =3, //!< Width of the frames in the video stream.
       CAP_PROP_FRAME_HEIGHT   =4, //!< Height of the frames in the video stream.
       CAP_PROP_FPS            =5, //!< Frame rate.
       CAP_PROP_FOURCC         =6, //!< 4-character code of codec. see VideoWriter::fourcc .
       CAP_PROP_FRAME_COUNT    =7, //!< Number of frames in the video file.
       CAP_PROP_FORMAT         =8, //!< Format of the %Mat objects returned by VideoCapture::retrieve().
       CAP_PROP_MODE           =9, //!< Backend-specific value indicating the current capture mode.
       CAP_PROP_BRIGHTNESS    =10, //!< Brightness of the image (only for those cameras that support).
       CAP_PROP_CONTRAST      =11, //!< Contrast of the image (only for cameras).
       CAP_PROP_SATURATION    =12, //!< Saturation of the image (only for cameras).
       CAP_PROP_HUE           =13, //!< Hue of the image (only for cameras).
       CAP_PROP_GAIN          =14, //!< Gain of the image (only for those cameras that support).
       CAP_PROP_EXPOSURE      =15, //!< Exposure (only for those cameras that support).
       CAP_PROP_CONVERT_RGB   =16, //!< Boolean flags indicating whether images should be converted to RGB.
       CAP_PROP_WHITE_BALANCE_BLUE_U =17, //!< Currently unsupported.
       CAP_PROP_RECTIFICATION =18, //!< Rectification flag for stereo cameras (note: only supported by DC1394 v 2.x backend currently).
       CAP_PROP_MONOCHROME    =19,
       CAP_PROP_SHARPNESS     =20,
       CAP_PROP_AUTO_EXPOSURE =21, //!< DC1394: exposure control done by camera, user can adjust reference level using this feature.
       CAP_PROP_GAMMA         =22,
       CAP_PROP_TEMPERATURE   =23,
       CAP_PROP_TRIGGER       =24,
       CAP_PROP_TRIGGER_DELAY =25,
       CAP_PROP_WHITE_BALANCE_RED_V =26,
       CAP_PROP_ZOOM          =27,
       CAP_PROP_FOCUS         =28,
       CAP_PROP_GUID          =29,
       CAP_PROP_ISO_SPEED     =30,
       CAP_PROP_BACKLIGHT     =32,
       CAP_PROP_PAN           =33,
       CAP_PROP_TILT          =34,
       CAP_PROP_ROLL          =35,
       CAP_PROP_IRIS          =36,
       CAP_PROP_SETTINGS      =37, //!< Pop up video/camera filter dialog (note: only supported by DSHOW backend currently. The property value is ignored)
       CAP_PROP_BUFFERSIZE    =38,
       CAP_PROP_AUTOFOCUS     =39,
       CAP_PROP_SAR_NUM       =40, //!< Sample aspect ratio: num/den (num)
       CAP_PROP_SAR_DEN       =41, //!< Sample aspect ratio: num/den (den)
#ifndef CV_DOXYGEN
       CV__CAP_PROP_LATEST
#endif
     };
#endif

using namespace cv;
using namespace std;

/* private functions and variables to this plugin */
static globals     *pglobal;

typedef struct {
    char *filter_args;
    int fps_set, fps,
        quality_set, quality,
        co_set, co,
        br_set, br,
        sa_set, sa,
        gain_set, gain,
        ex_set, ex;
} context_settings;

// filter functions
typedef bool (*filter_init_fn)(const char * args, void** filter_ctx);
typedef Mat (*filter_init_frame_fn)(void* filter_ctx);
typedef void (*filter_process_fn)(void* filter_ctx, Mat &src, Mat &dst);
typedef void (*filter_free_fn)(void* filter_ctx);


typedef struct {
    pthread_t   worker;
    VideoCapture capture;
    
    context_settings *init_settings;
    
    void* filter_handle;
    void* filter_ctx;
    
    filter_init_fn filter_init;
    filter_init_frame_fn filter_init_frame;
    filter_process_fn filter_process;
    filter_free_fn filter_free;
    
} context;


void *worker_thread(void *);
void worker_cleanup(void *);

#define INPUT_PLUGIN_NAME "OpenCV Input plugin"
static char plugin_name[] = INPUT_PLUGIN_NAME;

static void null_filter(void* filter_ctx, Mat &src, Mat &dst) {
    dst = src;
}

static void help() {
    
    fprintf(stderr,
    " ---------------------------------------------------------------\n" \
    " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
    " ---------------------------------------------------------------\n" \
    " The following parameters can be passed to this plugin:\n\n" \
    " [-d | --device ].......: video device to open (your camera)\n" \
    " [-r | --resolution ]...: the resolution of the video device,\n" \
    "                          can be one of the following strings:\n" \
    "                          ");
    
    resolutions_help("                          ");
    
    fprintf(stderr,
    " [-f | --fps ]..........: frames per second\n" \
    " [-q | --quality ] .....: set quality of JPEG encoding\n" \
    " ---------------------------------------------------------------\n" \
    " Optional parameters (may not be supported by all cameras):\n\n"
    " [-br ].................: Set image brightness (integer)\n"\
    " [-co ].................: Set image contrast (integer)\n"\
    " [-sh ].................: Set image sharpness (integer)\n"\
    " [-sa ].................: Set image saturation (integer)\n"\
    " [-ex ].................: Set exposure (off, or integer)\n"\
    " [-gain ]...............: Set gain (integer)\n"
    " ---------------------------------------------------------------\n" \
    " Optional filter plugin:\n" \
    " [ -filter ]............: filter plugin .so\n" \
    " [ -fargs ].............: filter plugin arguments\n" \
    " ---------------------------------------------------------------\n\n"\
    );
}

static context_settings* init_settings() {
    context_settings *settings;
    
    settings = (context_settings*)calloc(1, sizeof(context_settings));
    if (settings == NULL) {
        IPRINT("error allocating context");
        exit(EXIT_FAILURE);
    }
    
    settings->quality = 80;
    return settings;
}

/*** plugin interface functions ***/

/******************************************************************************
Description.: parse input parameters
Input Value.: param contains the command line string and a pointer to globals
Return Value: 0 if everything is ok
******************************************************************************/


int input_init(input_parameter *param, int plugin_no)
{
    const char * device = "default";
    const char *filter = NULL, *filter_args = "";
    int width = 640, height = 480, i, device_idx;
    
    input * in;
    context *pctx;
    context_settings *settings;
    
    pctx = new context();
    
    settings = pctx->init_settings = init_settings();
    pglobal = param->global;
    in = &pglobal->in[plugin_no];
    in->context = pctx;

    param->argv[0] = plugin_name;

    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    /* parse the parameters */
    reset_getopt();
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"h", no_argument, 0, 0},
            {"help", no_argument, 0, 0},
            {"d", required_argument, 0, 0},
            {"device", required_argument, 0, 0},
            {"r", required_argument, 0, 0},
            {"resolution", required_argument, 0, 0},
            {"f", required_argument, 0, 0},
            {"fps", required_argument, 0, 0},
            {"q", required_argument, 0, 0},
            {"quality", required_argument, 0, 0},
            {"co", required_argument, 0, 0},
            {"br", required_argument, 0, 0},
            {"sa", required_argument, 0, 0},
            {"gain", required_argument, 0, 0},
            {"ex", required_argument, 0, 0},
            {"filter", required_argument, 0, 0},
            {"fargs", required_argument, 0, 0},
            {0, 0, 0, 0}
        };
    
        /* parsing all parameters according to the list above is sufficent */
        c = getopt_long_only(param->argc, param->argv, "", long_options, &option_index);

        /* no more options to parse */
        if(c == -1) break;

        /* unrecognized option */
        if(c == '?') {
            help();
            return 1;
        }

        /* dispatch the given options */
        switch(option_index) {
        /* h, help */
        case 0:
        case 1:
            help();
            return 1;
        /* d, device */
        case 2:
        case 3:
            device = optarg;
            break;
        /* r, resolution */
        case 4:
        case 5:
            DBG("case 4,5\n");
            parse_resolution_opt(optarg, &width, &height);
            break;
        /* f, fps */
        case 6:
        OPTION_INT(7, fps)
            break;
        /* q, quality */
        case 8:
        OPTION_INT(9, quality)
            settings->quality = MIN(MAX(settings->quality, 0), 100);
            break;
        OPTION_INT(10, co)
            break;
        OPTION_INT(11, br)
            break;
        OPTION_INT(12, sa)
            break;
        OPTION_INT(13, gain)
            break;
        OPTION_INT(14, ex)
            break;
            
        /* filter */
        case 15:
            filter = optarg;
            break;
            
        /* fargs */
        case 16:
            filter_args = optarg;
            break;
            
        default:
            help();
            return 1;
        }
    }

    IPRINT("device........... : %s\n", device);
    IPRINT("Desired Resolution: %i x %i\n", width, height);
    
    // need to allocate a VideoCapture object: default device is 0
    try {
        if (!strcasecmp(device, "default")) {
            pctx->capture.open(0);
        } else if (sscanf(device, "%d", &device_idx) == 1) {
            pctx->capture.open(device_idx);
        } else {
            pctx->capture.open(device);
        }
    } catch (Exception e) {
        IPRINT("VideoCapture::open() failed: %s\n", e.what());
        goto fatal_error;
    }
    
    // validate that isOpened is true
    if (!pctx->capture.isOpened()) {
        IPRINT("VideoCapture::open() failed\n");
        goto fatal_error;
    }
    
    pctx->capture.set(CAP_PROP_FRAME_WIDTH, width);
    pctx->capture.set(CAP_PROP_FRAME_HEIGHT, height);
    
    if (settings->fps_set)
        pctx->capture.set(CAP_PROP_FPS, settings->fps);
    
    /* filter stuff goes here */
    if (filter != NULL) {
        
        IPRINT("filter........... : %s\n", filter);
        IPRINT("filter args ..... : %s\n", filter_args);
        
        pctx->filter_handle = dlopen(filter, RTLD_LAZY | RTLD_GLOBAL);
        if(!pctx->filter_handle) {
            LOG("ERROR: could not find input plugin\n");
            LOG("       Perhaps you want to adjust the search path with:\n");
            LOG("       # export LD_LIBRARY_PATH=/path/to/plugin/folder\n");
            LOG("       dlopen: %s\n", dlerror());
            goto fatal_error;
        }
        
        pctx->filter_init = (filter_init_fn)dlsym(pctx->filter_handle, "filter_init");
        if (pctx->filter_init == NULL) {
            LOG("ERROR: %s\n", dlerror());
            goto fatal_error;
        }
        
        pctx->filter_process = (filter_process_fn)dlsym(pctx->filter_handle, "filter_process");
        if (pctx->filter_process == NULL) {
            LOG("ERROR: %s\n", dlerror());
            goto fatal_error;
        }
        
        pctx->filter_free = (filter_free_fn)dlsym(pctx->filter_handle, "filter_free");
        if (pctx->filter_free == NULL) {
            LOG("ERROR: %s\n", dlerror());
            goto fatal_error;
        }
        
        // optional functions
        pctx->filter_init_frame = (filter_init_frame_fn)dlsym(pctx->filter_handle, "filter_init_frame");
        
        // initialize it
        if (!pctx->filter_init(filter_args, &pctx->filter_ctx)) {
            goto fatal_error;
        }
        
    } else {
        pctx->filter_handle = NULL;
        pctx->filter_ctx = NULL;
        pctx->filter_process = null_filter;
        pctx->filter_free = NULL;
    }
    
    return 0;
    
fatal_error:
    worker_cleanup(in);
    closelog();
    exit(EXIT_FAILURE);
}

/******************************************************************************
Description.: stops the execution of the worker thread
Input Value.: -
Return Value: 0
******************************************************************************/
int input_stop(int id)
{
    input * in = &pglobal->in[id];
    context *pctx = (context*)in->context;
    
    if (pctx != NULL) {
        DBG("will cancel input thread\n");
        pthread_cancel(pctx->worker);
    }
    return 0;
}

/******************************************************************************
Description.: starts the worker thread and allocates memory
Input Value.: -
Return Value: 0
******************************************************************************/
int input_run(int id)
{
    input * in = &pglobal->in[id];
    context *pctx = (context*)in->context;
    
    in->buf = NULL;
    in->size = 0;
    
    if(pthread_create(&pctx->worker, 0, worker_thread, in) != 0) {
        worker_cleanup(in);
        fprintf(stderr, "could not start worker thread\n");
        exit(EXIT_FAILURE);
    }
    pthread_detach(pctx->worker);

    return 0;
}

void *worker_thread(void *arg)
{
    input * in = (input*)arg;
    context *pctx = (context*)in->context;
    context_settings *settings = (context_settings*)pctx->init_settings;
    
    /* set cleanup handler to cleanup allocated resources */
    pthread_cleanup_push(worker_cleanup, arg);

    /* set VideoCapture options */
    #define CVOPT_OPT(prop, var, desc) \
        if (!pctx->capture.set(prop, settings->var)) {\
            IPRINT("%-18s: %d\n", desc, settings->var); \
        } else {\
            fprintf(stderr, "Failed to set " desc "\n"); \
        }
    
    #define CVOPT_SET(prop, var, desc) \
        if (settings->var##_set) { \
            CVOPT_OPT(prop, var,desc) \
        }
    
    CVOPT_SET(CAP_PROP_FPS, fps, "frames per second")
    CVOPT_SET(CAP_PROP_BRIGHTNESS, co, "contrast")
    CVOPT_SET(CAP_PROP_CONTRAST, br, "brightness")
    CVOPT_SET(CAP_PROP_SATURATION, sa, "saturation")
    CVOPT_SET(CAP_PROP_GAIN, gain, "gain")
    CVOPT_SET(CAP_PROP_EXPOSURE, ex, "exposure")
    
    /* setup imencode options */
    vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_JPEG_QUALITY);
    compression_params.push_back(settings->quality); // 1-100
    
    free(settings);
    pctx->init_settings = NULL;
    settings = NULL;
    
    Mat src, dst;
    vector<uchar> jpeg_buffer;
    
    // this exists so that the numpy allocator can assign a custom allocator to
    // the mat, so that it doesn't need to copy the data each time
    if (pctx->filter_init_frame != NULL)
        src = pctx->filter_init_frame(pctx->filter_ctx);
    
    while (!pglobal->stop) {
        if (!pctx->capture.read(src))
            break; // TODO
            
        // call the filter function
        pctx->filter_process(pctx->filter_ctx, src, dst);
            
        /* copy JPG picture to global buffer */
        pthread_mutex_lock(&in->db);
        
        // take whatever Mat it returns, and write it to jpeg buffer
        imencode(".jpg", dst, jpeg_buffer, compression_params);
        
        // TODO: what to do if imencode returns an error?
        
        // std::vector is guaranteed to be contiguous
        in->buf = &jpeg_buffer[0];
        in->size = jpeg_buffer.size();
        
        /* signal fresh_frame */
        pthread_cond_broadcast(&in->db_update);
        pthread_mutex_unlock(&in->db);
    }
    
    IPRINT("leaving input thread, calling cleanup function now\n");
    pthread_cleanup_pop(1);

    return NULL;
}

/******************************************************************************
Description.: this functions cleans up allocated resources
Input Value.: arg is unused
Return Value: -
******************************************************************************/
void worker_cleanup(void *arg)
{
    input * in = (input*)arg;
    if (in->context != NULL) {
        context *pctx = (context*)in->context;
        
        if (pctx->filter_free != NULL && pctx->filter_ctx != NULL) {
            pctx->filter_free(pctx->filter_ctx);
            pctx->filter_free = NULL;
        }
        
        if (pctx->filter_handle != NULL) {
            dlclose(pctx->filter_handle);
            pctx->filter_handle = NULL;
        }
        
        delete pctx;
        in->context = NULL;
    }
}
