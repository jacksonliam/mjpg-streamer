/*******************************************************************************
#                                                                              #
# Libcamera input plugin                                                       #
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
#include <linux/videodev2.h>

#include "input_libcamera.h"
#include "LibCamera.h"
#include "jpeg_utils.h"

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

typedef struct {
    pthread_t   worker;
    LibCamera camera;
    context_settings *init_settings;
    struct vdIn *videoIn;
} context;


void *worker_thread(void *);
void worker_cleanup(void *);

#define INPUT_PLUGIN_NAME "Libcamera Input plugin"
static char plugin_name[] = INPUT_PLUGIN_NAME;


static void help() {
    
    fprintf(stderr,
    " ---------------------------------------------------------------\n" \
    " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
    " ---------------------------------------------------------------\n" \
    " The following parameters can be passed to this plugin:\n\n" \
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
    " [-sa ].................: Set image saturation (integer)\n"\
    " [-ex ].................: Set exposure (integer)\n"\
    " [-gain ]...............: Set gain (integer)\n"
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
    int width = 640, height = 480, i, device_idx;
    
    input * in;
    context *pctx;
    context_settings *settings;
    int ret;
    ControlList controls_;
    int64_t frame_time;
    bool controls_flag = false;
    
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
            {"h", no_argument, 0, 0},                   // 0
            {"help", no_argument, 0, 0},                // 1
            {"r", required_argument, 0, 0},             // 2
            {"resolution", required_argument, 0, 0},    // 3
            {"f", required_argument, 0, 0},             // 4
            {"fps", required_argument, 0, 0},           // 5
            {"q", required_argument, 0, 0},             // 6
            {"quality", required_argument, 0, 0},       // 7
            {"co", required_argument, 0, 0},            // 8
            {"br", required_argument, 0, 0},            // 9
            {"sa", required_argument, 0, 0},            // 10
            {"gain", required_argument, 0, 0},          // 11
            {"ex", required_argument, 0, 0},            // 12
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
        /* r, resolution */
        case 2:
        case 3:
            DBG("case 2,3\n");
            parse_resolution_opt(optarg, &width, &height);
            break;
        /* f, fps */
        case 4:
        OPTION_INT(5, fps)
            break;
        /* q, quality */
        case 6:
        OPTION_INT(7, quality)
            settings->quality = MIN(MAX(settings->quality, 0), 100);
            break;
        OPTION_INT(8, co)
            break;
        OPTION_INT(9, br)
            break;
        OPTION_INT(10, sa)
            break;
        OPTION_INT(11, gain)
            break;
        OPTION_INT(12, ex)
            break;
            
        default:
            help();
            return 1;
        }
    }

    IPRINT("Desired Resolution: %i x %i\n", width, height);

    ret = pctx->camera.initCamera(&width, &height, formats::BGR888, 4, 0);
    if (ret) {
        IPRINT("LibCamera::initCamera() failed\n");
        goto fatal_error;
    }

    if (settings->fps){
        frame_time = 1000000 / settings->fps;
        // Set frame rate
	    controls_.set(controls::FrameDurationLimits, { frame_time, frame_time });
        controls_flag = true;
    }
    if (settings->br) {
        controls_.set(controls::Brightness, settings->br);
        controls_flag = true;
    }
    if (settings->co) {
        controls_.set(controls::Contrast, settings->co);
        controls_flag = true;
    }
    if (settings->sa) {
        controls_.set(controls::Saturation, settings->sa);
        controls_flag = true;
    }
    if (settings->gain) {
        controls_.set(controls::AnalogueGain, settings->gain);
        controls_flag = true;
    }
    if (settings->ex) {
        controls_.set(controls::ExposureTime, settings->ex);
        controls_flag = true;
    }
    if (controls_flag) {
        pctx->camera.set(controls_);
    }

    pctx->videoIn = (struct vdIn*)malloc(sizeof(struct vdIn));
    pctx->videoIn->width = width;
    pctx->videoIn->height = height;
    
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

    free(pctx->videoIn);

    free(in->buf);
    in->buf = NULL;
    in->size = 0;

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

    in->buf = (uint8_t *) malloc(pctx->videoIn->width * pctx->videoIn->height);
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
    int quality = settings->quality;
    LibcameraOutData frameData;
    
    /* set cleanup handler to cleanup allocated resources */
    pthread_cleanup_push(worker_cleanup, arg);
    
    free(settings);
    pctx->init_settings = NULL;
    settings = NULL;
    
    pctx->camera.startCamera();
    while (!pglobal->stop) {
        if (!pctx->camera.readFrame(&frameData))
            continue;
            
        pthread_mutex_lock(&in->db);

        pctx->videoIn->framebuffer = frameData.imageData;
        pctx->videoIn->formatIn = V4L2_PIX_FMT_RGB24;
        in->size = compress_image_to_jpeg(pctx->videoIn, in->buf, frameData.size, quality);
        
        /* signal fresh_frame */
        pthread_cond_broadcast(&in->db_update);
        pthread_mutex_unlock(&in->db);

        pctx->camera.returnFrameBuffer(frameData);
    }

    pctx->camera.stopCamera();
    pctx->camera.closeCamera();

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
        delete pctx;
        in->context = NULL;
    }
}
