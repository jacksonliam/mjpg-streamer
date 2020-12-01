/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2020 Jeff Tchang                                          #
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
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "../../mjpg_streamer.h"
#include "../../utils.h"

#define INPUT_PLUGIN_NAME "v4l2loopback input plugin"

/* private functions and variables to this plugin */
static pthread_t worker;
static globals *pglobal;

void *worker_thread(void *);
void worker_cleanup(void *);
void help(void);

static double delay_in_seconds = 1.0;
static int rm = 0;
static int plugin_number;

/* global variables for this plugin */
static int rc, size;

static int v4l2_device_fd;
uint8_t *buffer;
char v4l2_device[255];
int fps = 0;

/*** plugin interface functions ***/
int input_init(input_parameter *param, int id)
{
    int i;
    plugin_number = id;

    param->argv[0] = INPUT_PLUGIN_NAME;

    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    reset_getopt();
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"h", no_argument, 0, 0},
            {"help", no_argument, 0, 0},
            {"d", required_argument, 0, 0},
            {"device", required_argument, 0, 0},
            {"f", required_argument, 0, 0},
            {"fps", required_argument, 0, 0},
            {0, 0, 0, 0}
        };

        c = getopt_long_only(param->argc, param->argv, "", long_options, &option_index);

        /* no more options to parse */
        if(c == -1) break;

        /* unrecognized option */
        if(c == '?') {
            help();
            return 1;
        }

        switch(option_index) {

            // help
            case 0:
            case 1:
                help();
                return 1;
                break;

            // device
            case 2:
            case 3:
                strcpy(v4l2_device, optarg);
                break;

            // fps
            case 4:
            case 5:
                fps = atoi(optarg);
                break;

            /* h, help */
            default:
                help();
                return 1;
        }
    }

    pglobal = param->global;

    param->global->in[id].name = malloc((strlen(INPUT_PLUGIN_NAME) + 1) * sizeof(char));
    sprintf(param->global->in[id].name, INPUT_PLUGIN_NAME);



    DBG("Opening v4l2_device: %s\n", v4l2_device);

    v4l2_device_fd = open(v4l2_device, O_RDWR);

    if (v4l2_device_fd < 0) {
        perror("Opening video device");
        return 1;
    }

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 1920;
    fmt.fmt.pix.height = 1080;

    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.colorspace = V4L2_COLORSPACE_JPEG;

    if (xioctl(v4l2_device_fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("Error setting v4l2_format using VIDIOC_S_FMT\n");
        return 1;
    }

    // Calculate out the frames per second to use

    struct v4l2_streamparm parm = {0};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(v4l2_device_fd, VIDIOC_G_PARM, &parm) == -1) {
        perror("Error getting v4l2 parameters\n");
        return 1;
    }

    struct v4l2_fract tf = parm.parm.capture.timeperframe;

    delay_in_seconds = (1.0 * tf.numerator) / tf.denominator;

    // The passed in fps parameter overrides the one taken from the v4l2 device
    if (fps > 0) {
        delay_in_seconds = 1.0 / fps;
    }

    IPRINT("Initialized the v4l2loopback input plugin\n");
    IPRINT("Frames per second from v4l2 device: %.3f (%d/%d)\n", 
        (1.0 * tf.denominator) / tf.numerator, tf.denominator, tf.numerator);
    IPRINT("Delay in seconds: %.3f\n", delay_in_seconds);

    return 0;
}

int input_stop(int id)
{
    DBG("Canceling input thread.\n");
    pthread_cancel(worker);
    return 0;
}

int input_run(int id)
{
    // Make sure the pointer to the global frame buffer is null
    pglobal->in[id].buf = NULL;

    // Start the thread that will feed frames into the global frame buffer
    if(pthread_create(&worker, 0, worker_thread, NULL) != 0) {
        free(pglobal->in[id].buf);
        fprintf(stderr, "Could not start worker thread\n");
        exit(EXIT_FAILURE);
    }

    pthread_detach(worker);

    return 0;
}

/*** private functions for this plugin below ***/
void help(void)
{
    // fprintf(stderr, " ---------------------------------------------------------------\n" \
    // " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
    // " ---------------------------------------------------------------\n" \
    // " The following parameters can be passed to this plugin:\n\n" \
    // " [-d | --delay ]........: delay (in seconds) to pause between frames\n" \
    // " [-f | --folder ].......: folder to watch for new JPEG files\n" \
    // " [-r | --remove ].......: remove/delete JPEG file after reading\n" \
    // " [-n | --name ].........: ignore changes unless filename matches\n" \
    // " [-e | --existing ].....: serve the existing *.jpg files from the specified directory\n" \
    // " ---------------------------------------------------------------\n");
}

// Entrypoint for the thread that reads from v4l2loopback device
// and writes frames into the global video buffer
void *worker_thread(void *arg)
{
    int r;
    struct timeval timestamp;
    struct v4l2_requestbuffers req = {0};
    struct v4l2_buffer buf = {0};

    fd_set fds;
    struct timeval tv = {0};

    // Setup a handler to cleanup allocated resources when thread exits
    pthread_cleanup_push(worker_cleanup, NULL);

    DBG("Entering the worker thread.\n");

    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(v4l2_device_fd, VIDIOC_REQBUFS, &req)) {
        perror("Error requesting buffer");
        return NULL;
    }


    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    DBG("Querying the buffer using VIDIOC_QUERYBUF\n");
    if(-1 == xioctl(v4l2_device_fd, VIDIOC_QUERYBUF, &buf)) {
        perror("Error querying buffer");
        return NULL;
    }

    buffer = mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, v4l2_device_fd, buf.m.offset);


    buf.bytesused = 0;
    if(xioctl(v4l2_device_fd, VIDIOC_QBUF, &buf) == -1) {
        perror("Unable to queue up a buffer to the v4l2 device via VIDIOC_QBUF\n");
        return NULL;
    }


    // Tell the device to start streaming (called only once)
    if(xioctl(v4l2_device_fd, VIDIOC_STREAMON, &buf.type) == -1) {
        perror("Unable to start stream using VIDIOC_STREAMON\n");
        return NULL;
    }
    DBG("Streaming has started: VIDIOC_STREAMON\n");

    
    while(!pglobal->stop) {

        // After queueing the buffer we need to wait till the camera
        // writes to the image so that we can read from the buffer.
        // We can achieve this by the select() system call.

        FD_ZERO(&fds);
        FD_SET(v4l2_device_fd, &fds);
        
        // Timeout: Wait up to 2 seconds for the camera to fill the buffer
        tv.tv_sec = 2;

        // The first argument is the highest-numbered file
        // descriptor in any of the three sets plus 1.
        DBG("Before select.\n");
        r = select(v4l2_device_fd+1, &fds, NULL, NULL, &tv);
        DBG("After select. Return code of select is: %d\n", r);

        if(r == -1) {
            fprintf(stderr, "Error while waiting for frame");
            return NULL;
        }

        if(xioctl(v4l2_device_fd, VIDIOC_DQBUF, &buf) == -1) {
            perror("Error dequeueing the buffer");
            return NULL;
        }

        DBG("Got a frame from v4l2 device of size %d at index %d\n", buf.bytesused, buf.index);
        if (buf.bytesused > 1024*1024) {
            continue;
        }


        // HACK: We dequeue the buffer and queue it up again.
        // We are doing this twice to avoid some type of artifact
        // issue. https://github.com/umlaeute/v4l2loopback/issues/191
        buf.bytesused = 0;
        if(xioctl(v4l2_device_fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Unable to queue up a buffer to the v4l2 device via VIDIOC_QBUF\n");
            return NULL;
        }
        DBG("Queued up a buffer for the v4l2 device to write to.\n");

        r = select(v4l2_device_fd+1, &fds, NULL, NULL, &tv);
        if(r == -1) {
            fprintf(stderr, "Error while waiting for frame");
            return NULL;
        }
        if(xioctl(v4l2_device_fd, VIDIOC_DQBUF, &buf) == -1) {
            perror("Error dequeueing the buffer");
            return NULL;
        }




        // Copy the frame into the global shared memory buffer
        // Since this is shared it is protected by a mutex

        pthread_mutex_lock(&pglobal->in[plugin_number].db);

        /* allocate memory for frame */
        if(pglobal->in[plugin_number].buf != NULL) {            
            free(pglobal->in[plugin_number].buf);
        }


        // pglobal->in[plugin_number].buf = malloc(buf.bytesused + (1 << 16));
        pglobal->in[plugin_number].buf = calloc(1, buf.bytesused);

        if(pglobal->in[plugin_number].buf == NULL) {
            fprintf(stderr, "Could not allocate memory for buffer.\n");
            break;
        }

        memcpy(pglobal->in[plugin_number].buf, buffer, buf.bytesused);

        pglobal->in[plugin_number].size = buf.bytesused;

        gettimeofday(&timestamp, NULL);
        pglobal->in[plugin_number].timestamp = timestamp;

        DBG("New frame copied into buffer of size: %d\n", pglobal->in[plugin_number].size);

        // Enqueue the buffer again.
        buf.bytesused = 0;
        if(xioctl(v4l2_device_fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Unable to queue up a buffer to the v4l2 device via VIDIOC_QBUF\n");
            return NULL;
        }
        DBG("Queued up a buffer for the v4l2 device to write to.\n");


        // Signal a new video frame has been put into the buffer
        pthread_cond_broadcast(&pglobal->in[plugin_number].db_update);
        

        // Unlock the mutex
        pthread_mutex_unlock(&pglobal->in[plugin_number].db);



        if(delay_in_seconds != 0) {
            usleep(1000 * 1000 * delay_in_seconds);
        }
    }

    // Tell the device to stop streaming (called only once)
    if(xioctl(v4l2_device_fd, VIDIOC_STREAMOFF) == -1) {
        perror("Unable to stop stream using VIDIOC_STREAMOFF\n");
        return NULL;
    }
    DBG("Streaming has stopped: VIDIOC_STREAMOFF\n");


    DBG("leaving input thread, calling cleanup function now\n");
    /* call cleanup handler, signal with the parameter */
    pthread_cleanup_pop(1);

    return NULL;
}

void worker_cleanup(void *arg)
{
    static unsigned char first_run = 1;

    if(!first_run) {
        DBG("Already cleaned up resources\n");
        return;
    }

    first_run = 0;
    DBG("Cleaning up resources allocated by input thread\n");

    if(pglobal->in[plugin_number].buf != NULL) {
        free(pglobal->in[plugin_number].buf);
    }

}

// Helper functions
int xioctl(int fd, int request, void *arg)
{
  int r;

  do r = ioctl (fd, request, arg);
  while (-1 == r && EINTR == errno);

  return r;
}




