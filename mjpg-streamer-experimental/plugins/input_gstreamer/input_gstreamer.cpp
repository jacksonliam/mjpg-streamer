#include <getopt.h>
#include <vector>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <gst/gst.h>
#include <time.h>

#include "input_gstreamer.h"

using namespace std;

static globals     *pglobal;

typedef struct {
    pthread_t worker;
    GstElement *pipeline;
    GMainLoop *main_loop;
    input * in;
    vector<unsigned char> jpeg_buffer;
    int width, height;
    int cvType;
} context;

void *worker_thread(void *);
void worker_cleanup(void *);

#define INPUT_PLUGIN_NAME "GStreamer plugin"
static char plugin_name[] = INPUT_PLUGIN_NAME;

static void help() {
    
    fprintf(stderr,
    " ---------------------------------------------------------------\n" \
    " Help for input plugin..: " INPUT_PLUGIN_NAME "\n" \
    " ---------------------------------------------------------------\n" \
    " This plugin accepts no parameters. Instead, set the environment\n" \
    " variable MJPG_GSTREAM_PIPELINE to your desired GStreamer pipeline. The pipeline should end with `! jpegenc ! appsink`\n\n" \
    " Here is the default pipeline, which works when using a rasberry pi camera on an Nvidia Jetson:\n" \
    " \"nvarguscamerasrc sensor-id=0 ! video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1 ! nvvidconv flip-method=0 ! video/x-raw, width=(int)920, height=(int)540, format=(string)BGRx ! videoconvert ! video/x-raw, format=(string)BGR ! jpegenc ! appsink\"\n" \
    " ---------------------------------------------------------------\n\n" \
    );
}


int input_init(input_parameter *param, int plugin_no) {

    input * in;
    context *pctx;
    
    pctx = new context();
    pglobal = param->global;
    in = &pglobal->in[plugin_no];
    in->context = pctx;
    pctx->in = in;
    pctx->width = -1;
    pctx->height = -1;
    pctx->cvType = -1;
    
    param->argv[0] = plugin_name;

    /* show all parameters for DBG purposes */
    for(int i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }
    
    // Get the pipeline string from the environment variable. If not found, default to pipeline that works for raspi cam v2 on the Jetson Nano.
    const char *pipeline_str = getenv("MJPEG_GSTREAM_PIPELINE");
    if (!pipeline_str) {
        pipeline_str = "nvarguscamerasrc sensor-id=0 ! "
                       "video/x-raw(memory:NVMM), width=(int)1920, height=(int)1080, framerate=(fraction)30/1 ! "
                       "nvvidconv flip-method=0 ! "
                       "video/x-raw, width=(int)920, height=(int)540, format=(string)BGRx ! "
                       "videoconvert ! "
                       "video/x-raw, format=(string)BGR ! jpegenc ! appsink";
    }

    IPRINT("GStreamer pipeline: %s\n", pipeline_str);
    
    // Initialize GStreamer
    gst_init(NULL, NULL);

    // Setup GStreamer pipeline
    GError *err = NULL;
    pctx->pipeline = gst_parse_launch(pipeline_str, &err);
    if (!pctx->pipeline) {
        IPRINT("Failed to create GStreamer pipeline: %s. Exiting...\n", err->message);
        g_error_free(err);
        goto cleanup;
    }
    return 0;
    
cleanup:
    worker_cleanup(in);
    closelog();
    exit(EXIT_FAILURE);
}

int input_stop(int id) {
    input * in = &pglobal->in[id];
    context *pctx = (context*)in->context;
    
    if (pctx != NULL) {
        DBG("will cancel GStreamer input thread\n");
        pthread_cancel(pctx->worker);
        
        // If there are any GStreamer-specific cleanup tasks, add them here.
        // For example, stopping the GStreamer pipeline or releasing resources.
    }
    return 0;
}

int input_run(int id) {
    input * in = &pglobal->in[id];
    context *pctx = (context*)in->context;
    
    in->buf = NULL;
    in->size = 0;
       
    if(pthread_create(&pctx->worker, 0, worker_thread, in) != 0) {
        worker_cleanup(in);
        fprintf(stderr, "could not start GStreamer worker thread\n");
        exit(EXIT_FAILURE);
    }
    pthread_detach(pctx->worker);

    return 0;
}

static GstFlowReturn appsink_callback(GstElement *sink, context *pctx) {
    GstSample *sample;
    GstBuffer *buffer;
    GstMapInfo map;

    // Get the sample from appsink
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (!sample) {
        IPRINT("Failed to retrieve sample from appsink. Exiting...\n");
        return GST_FLOW_ERROR;
    }
    buffer = gst_sample_get_buffer(sample);
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        IPRINT("Failed to map GStreamer buffer. Exiting...\n");
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }
    
    // Lock and update global buffer with JPEG data
    pthread_mutex_lock(&pctx->in->db);

    gettimeofday(&pctx->in->timestamp, NULL);

    pctx->jpeg_buffer.assign(map.data, map.data + map.size);
    
    pctx->in->buf = &pctx->jpeg_buffer[0];
    pctx->in->size = pctx->jpeg_buffer.size();
    
    // Signal fresh_frame
    pthread_cond_broadcast(&pctx->in->db_update);
    pthread_mutex_unlock(&pctx->in->db);

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
}

// Idle function to check the stop flag
static gboolean check_stop_flag(context *pctx) {
    if (pglobal->stop) {
        if (pctx->main_loop) {
            g_main_loop_quit(pctx->main_loop);
        }
        return FALSE; // Remove the idle function
    }
    return TRUE; // Keep calling the idle function
}

void *worker_thread(void *arg) {
    input * in = (input*)arg;
    context *pctx = (context*)in->context;

    // Push the cleanup handler
    pthread_cleanup_push(worker_cleanup, in);
    // Set up the appsink callback
    GstElement *appsink = gst_bin_get_by_name(GST_BIN(pctx->pipeline), "appsink0");
    g_object_set(G_OBJECT(appsink), "emit-signals", TRUE, NULL);
    gulong handler_id = g_signal_connect(appsink, "new-sample", G_CALLBACK(appsink_callback), pctx);
    if (handler_id == 0) {
        IPRINT("Failed to connect to new-sample signal. Exiting...\n");
        goto fatal_error;
    }
    // Start playing the GStreamer pipeline
    GstStateChangeReturn ret = gst_element_set_state(pctx->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        IPRINT("Failed to start GStreamer pipeline. Exiting...\n");
        goto fatal_error;
    }

    // Run the GStreamer main loop to process frames
    pctx->main_loop = g_main_loop_new(NULL, FALSE);
    
    // Add an idle function to check the stop flag
    g_idle_add((GSourceFunc)check_stop_flag, pctx);

    g_main_loop_run(pctx->main_loop);

    // Cleanup
    gst_element_set_state(pctx->pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pctx->pipeline));
    pctx->pipeline = NULL;
    g_main_loop_unref(pctx->main_loop);
    pctx->main_loop = NULL;

    IPRINT("leaving input thread, calling cleanup function now\n");

    // Pop the cleanup handler
    pthread_cleanup_pop(1);

    return NULL;

fatal_error:
    IPRINT("An error occurred in the worker thread.\n");
    pthread_exit(NULL);
}


void worker_cleanup(void *arg) {
    input * in = (input*)arg;
    if (in->context != NULL) {
        context *pctx = (context*)in->context;

        // Cleanup GStreamer resources
        if (pctx->pipeline) {
            gst_element_set_state(pctx->pipeline, GST_STATE_NULL);
            gst_object_unref(pctx->pipeline);
            pctx->pipeline = NULL;
        }

        if (pctx->main_loop) {
            g_main_loop_unref(pctx->main_loop);
            pctx->main_loop = NULL;
        }

        delete pctx;
        in->context = NULL;
    }
}


