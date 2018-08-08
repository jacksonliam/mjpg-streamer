/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
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

/*
  This output plugin is based on code from output_file.c etc etc etc.
  Copyright (C) 2018 Vladimir Dronnikov <dronnikov@gmail.com>
  LICENSE: http://opensource.org/licenses/MIT
  Version 0.1, April 2018

  Grab the fresh frame.
  Resize frame by 1/8 and convert to grayscale.
  Perform background extraction: for each pixel in frame:
    - Subtract corresponding pixel of background frame
    - If absolute difference greater than diff-threshold,
        label this pixel as foreground pixel
    - Add difference scaled by background-learning-rate to background.
      This slowly drifts background to the current frame.
  Count share of foreground pixels as motion probability.
  Maintain motion probability timeline.
  Trigger alarm on if motion probability is seen for several frames.
  Call helper script `action.sh alarm 1` when alarm started.
  Popen helper script `action.sh record %Y-%m-%d/%H-%M-%S` when alarm started.
  Dump frames to its stdin while alarm is on.
  Trigger alarm off if motion probability is not seen for several frames.
  Close popen stdin when alarm stopped.
  Call helper script `action.sh alarm 0` when alarm stopped.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <getopt.h>

#include <jpeglib.h>
#include <jerror.h>

#include "../../utils.h"
#include "../../mjpg_streamer.h"

#define OUTPUT_PLUGIN_NAME "Motion detection output plugin"

static pthread_t worker;
static globals *pglobal;
static int input_number = 0;
static unsigned char *frame = NULL;

// motion detection parameters
static int scale_num = 1;
static int scale_denom = 8;
static int foreground_diff_threshold = 35;
static double background_learning_rate = 0.01;
static double motion_noise_threshold = 0.001;
static double motion_start_duration = 1.5;
static double motion_stop_duration = 8.0;
static int dump_motion_frames = 0;

// background image
static double *frame_back = NULL;
// image for analysis
static unsigned char *frame_gray = NULL;

// maintain frame timeline
#define FRAME_TIMELINE  32
struct frame_s {
    unsigned char *buf;
    unsigned int len;
};
static struct frame_s frame_timeline[FRAME_TIMELINE];
static void frame_append(unsigned char *buf, unsigned int len)
{
    if (frame_timeline[FRAME_TIMELINE - 1].buf) {
        free(frame_timeline[FRAME_TIMELINE - 1].buf);
    }
    for (int i = FRAME_TIMELINE; --i >= 1;) {
        frame_timeline[i] = frame_timeline[i - 1];
    }
    void *p = malloc(len);
    if (!p) {
        frame_timeline[0].buf = NULL;
        frame_timeline[0].len = 0;
    } else {
        memcpy(p, buf, len);
        frame_timeline[0].buf = p;
        frame_timeline[0].len = len;
    }
}

static ssize_t full_write(int fd, const void *buf, size_t len)
{
    ssize_t cc;
    ssize_t total;
    total = 0;
    while (len) {
        for (;;) {
            cc = write(fd, buf, len);
            if (cc >= 0 || EINTR != errno) {
                break;
            }
            errno = 0;
        }
        if (cc < 0) {
            if (total) {
                return total;
            }
            return cc;
        }
        total += cc;
        buf = ((const char *)buf) + cc;
        len -= cc;
    }
    return total;
}

static void frame_dump(FILE *fp, void *buf, size_t len)
{
    if (fp) {
        // TODO: ensure write is full!
        if (1 != fwrite(buf, len, 1, fp)) {
            perror("FRAME_DUMP: ");
            if (EPIPE == errno) {
                // FIXME: TODO: down the process?
            }
        }
        fflush(fp);
    }
    // dump frame to stdout
    if (dump_motion_frames) {
        if (full_write(1, buf, len) < 0) {
            perror("FULLWRITE: ");
        }
    }
}

static FILE *writer = NULL;
static char fname[1024];

/******************************************************************************
Description.: print a help message
Input Value.: -
Return Value: -
******************************************************************************/
void help(void)
{
    fprintf(stderr,
        " ---------------------------------------------------------------\n" \
        " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
        " ---------------------------------------------------------------\n" \
        " The following parameters can be passed to this plugin:\n\n" \
        " [-i | --input ]..................: read frames from the specified input plugin (first input plugin between the arguments is the 0th)\n\n" \
        " [--scale-num ]...................: image scale numerator. Default: %d\n\n" \
        " [--scale-denom ].................: image scale denominator (1,2,4,8). Default: %d\n\n" \
        " [--foreground-diff-threshold ]...: image difference threshold (0-255). Default: %d\n\n" \
        " [--background-learning-rate ]....: background learning rate. Default: %.3f\n\n" \
        " [--motion-noise-threshold ]......: motion probability noise threshold. Default: %.3f\n\n" \
        " [--motion-start-duration ].......: time with motion to report motion started. Default: %.1f\n\n" \
        " [--motion-stop-duration ]........: time without motion to report motion stopped. Default: %.1f\n\n" \
        " [--dump-motion-frames ]..........: dump motion frames to stdout.\n\n" \
        " ---------------------------------------------------------------\n",
        scale_num, scale_denom, foreground_diff_threshold,
        background_learning_rate, motion_noise_threshold,
        motion_start_duration, motion_stop_duration
    );
}

/******************************************************************************
Description.: clean up allocated resources
Input Value.: unused argument
Return Value: -
******************************************************************************/
void worker_cleanup(void *arg)
{
    static unsigned char first_run = 1;

    if (!first_run) {
        DBG("already cleaned up resources\n");
        return;
    }

    first_run = 0;
    OPRINT("cleaning up resources allocated by worker thread\n");

    if (frame) {
        free(frame);
    }

    if (frame_gray) {
        free(frame_gray);
    }
    if (frame_back) {
        free(frame_back);
    }
}

/******************************************************************************
Description.: this is the main worker thread
              it loops forever, grabs a fresh frame, extracts background
              and reports motion probablility
Input Value.:
Return Value:
******************************************************************************/
void *worker_thread(void *arg)
{
    int ok = 1, frame_size = 0;
    static int max_frame_size;
    unsigned char *tmp_framebuffer = NULL;
    char buf[4096];

    /* set cleanup handler to cleanup allocated resources */
    pthread_cleanup_push(worker_cleanup, NULL);

    // -----------------------------------------------------------

    // maintain frame rate
    double fps = 1.0;
    unsigned long frame_idx = 0;
    time_t started_at, now;
    time(&started_at);

    // -----------------------------------------------------------

    // maintain motion probability timeline
    #define MOTION_DIFF_TIMELINE    1024
    static double diff_timeline[MOTION_DIFF_TIMELINE];
    void diff_append(double v)
    {
        for (int i = MOTION_DIFF_TIMELINE; --i >= 1;) {
            diff_timeline[i] = diff_timeline[i - 1];
        }
        diff_timeline[0] = v;
    }
    int diff_any(unsigned n)
    {
        int r = 0;
        if (n > MOTION_DIFF_TIMELINE) n = MOTION_DIFF_TIMELINE;
        for (int i = 0; i < n; ++i) {
            if (0.0 != diff_timeline[i]) {
                r = 1;
                break;
            }
        }
        return r;
    }
    int diff_all(unsigned n)
    {
        int r = !!n;
        if (n > MOTION_DIFF_TIMELINE) n = MOTION_DIFF_TIMELINE;
        for (int i = 0; i < n; ++i) {
            if (0.0 == diff_timeline[i]) {
                r = 0;
                break;
            }
        }
        return r;
    }

    // -----------------------------------------------------------

    // JPEG decompressor
    struct jpeg_decompress_struct dinfo;
    struct jpeg_error_mgr jerr;

    // NB: silence annoying "extraneous data" warning
    dinfo.err = jpeg_std_error(&jerr);
    void (*old_output_message)(j_common_ptr cinfo) = dinfo.err->output_message;
    void dummy_output_message(j_common_ptr cinfo) {
        if (JWRN_EXTRANEOUS_DATA == cinfo->err->msg_code) return;
        old_output_message(cinfo);
    }
    dinfo.err->output_message = dummy_output_message;

    // -----------------------------------------------------------

    while (ok >= 0 && !pglobal->stop) {

        DBG("waiting for fresh frame\n");
        pthread_mutex_lock(&pglobal->in[input_number].db);
        pthread_cond_wait(
            &pglobal->in[input_number].db_update,
            &pglobal->in[input_number].db
        );

        /* read buffer */
        frame_size = pglobal->in[input_number].size;

        /* check if frame buffer is large enough, increase it if necessary */
        if (frame_size > max_frame_size) {
            DBG("increasing buffer size to %d\n", frame_size);

            max_frame_size = frame_size + (1 << 16);
            if (NULL == (tmp_framebuffer = realloc(frame, max_frame_size))) {
                pthread_mutex_unlock(&pglobal->in[input_number].db);
                LOG("not enough memory\n");
                return NULL;
            }

            frame = tmp_framebuffer;
        }

        /* copy frame to our local buffer now */
        memcpy(frame, pglobal->in[input_number].buf, frame_size);

        /* allow others to access the global buffer again */
        pthread_mutex_unlock(&pglobal->in[input_number].db);

        // -----------------------------------------------------------

        time(&now);

        // prepare frame decompressor
        // NB: frame is downscaled as 1/8 and converted to grayscale
        jpeg_create_decompress(&dinfo);
        jpeg_mem_src(&dinfo, frame, frame_size);
        jpeg_read_header(&dinfo, TRUE);
        dinfo.out_color_space = JCS_GRAYSCALE;
        dinfo.dct_method = JDCT_FASTEST;
        dinfo.do_fancy_upsampling = FALSE;
        // width = dinfo.image_width;
        // height = dinfo.image_height;
        dinfo.scale_num = scale_num;
        dinfo.scale_denom = scale_denom;
        jpeg_start_decompress(&dinfo);

        // allocate frame buffers
        // TODO: make static with just sufficient room for 2K original frame?
        static unsigned int frame_back_length = 0;
        if (!frame_back_length) {
            frame_back_length = dinfo.output_width * dinfo.output_height;
        }
        if (!frame_back) {
            frame_back = malloc(frame_back_length * sizeof(frame_back[0]));
        }
        if (!frame_gray) {
            frame_gray = malloc(dinfo.output_width * sizeof(frame_gray[0]));
        }

        // decompress line by line and calculate difference from background
        unsigned int fore = 0;  // foreground pixels counter
        unsigned int backp = 0; // background pixel index
        double change_rate = background_learning_rate / fps;
        // for each frame line
        while (dinfo.output_scanline < dinfo.output_height) {
            JSAMPROW line = frame_gray;
            jpeg_read_scanlines(&dinfo, &line, 1);
            // for each pixel in line
            for (int x = 0; x < dinfo.output_width; ++x) {
                // in no background known let it equals current frame
                if (0 == frame_idx) {
                    frame_back[backp] = frame_gray[x];
                }
                // calc difference
                double v = frame_gray[x] - frame_back[backp];
                // update background by learning rate
                frame_back[backp] += change_rate * v;
                ++backp;
                // if absolute difference is above difference threshold...
                if (v < 0) v = -v;
                if (v < foreground_diff_threshold) continue;
                // ...count pixel as foreground
                ++fore;
            }
        }
        // motion probability is the share of foreground pixels
        double motion_probability = fore * 1.0 / frame_back_length;
        // cleanup decompressor
        jpeg_finish_decompress(&dinfo);
        jpeg_destroy_decompress(&dinfo);

        // apply noise threshold
        if (motion_probability < motion_noise_threshold) {
            motion_probability = 0;
        }

        // maintain motion probability timeline
        static double max_motion_probability = 0.0;
        if (motion_probability > max_motion_probability) {
            max_motion_probability = motion_probability;
        }
        diff_append(motion_probability);

        // not in motion?
        static int alarm_status = 0;
        static int need_report_status = 1;
        if (!alarm_status) {
            // last M frames with non-zero motion probability?
            if (diff_all(motion_start_duration * fps)) {
                // alarm on
                alarm_status = 1;
                need_report_status = 1;
                // compose storage basename
                struct tm *ts = localtime(&now);
                strftime(fname, sizeof(fname), "%Y-%m-%d/%H-%M-%S", ts);
                // determine scene change factor
                double scene_change = 0.015;
                if (max_motion_probability < 0.03) {
                    scene_change = 0.1 * max_motion_probability;
                }
                if (scene_change < 0.003) {
                    scene_change = 0.003;
                }
                // start writer
                snprintf(buf, sizeof(buf),
                    "exec sh action.sh record %s %.3f %.3f",
                    fname, scene_change, fps
                );
                writer = popen(buf, "w");
                setvbuf(writer, NULL, _IONBF, 0); // FIXME: TODO: validate need
                if (!writer) {
                    perror("WRITER: ");
                }
                // write pre-motion frames
                int pre_motion_frames = motion_start_duration * fps + 1.5;
                if (pre_motion_frames > FRAME_TIMELINE) {
                    pre_motion_frames = FRAME_TIMELINE;
                }
                for (int i = pre_motion_frames; --i >= 0;) {
                    struct frame_s *f = &frame_timeline[i];
                    if (f->buf && f->len) {
                        frame_dump(writer, f->buf, f->len);
                    }
                }
            }
        // in motion
        } else {
            // frames timeline shows no motion probability?
            if (!diff_any(motion_stop_duration * fps)) {
                // alarm off
                alarm_status = 0;
                need_report_status = 1;
                // stop writer
                if (writer) {
                    pclose(writer);
                    writer = NULL;
                }
                // reset motion probability
                max_motion_probability = 0.0;
            }
        }

        // report state
        if (need_report_status) {
            need_report_status = 0;
            // run alarm helper
            snprintf(buf, sizeof(buf),
                "exec sh action.sh alarm %d %.3f %.3f",
                alarm_status, motion_probability, fps
            );
            int rc = system(buf);
        }

        // write frame
        if (alarm_status) {
            frame_dump(writer, frame, frame_size);
        }

        // maintain frame timeline
        frame_append(frame, frame_size);

        // // if not in motion skip the frame but pass each hundred-th one
        // int need_image = alarm_status || (0 == (frame_idx % 100));
        // if (need_image) {
        // }

        // update frame rate
        ++frame_idx;
        fps = frame_idx / difftime(now, started_at);

        // -----------------------------------------------------------

    }

    // -----------------------------------------------------------

    /* cleanup now */
    pthread_cleanup_pop(1);

    return NULL;
}

/*** plugin interface functions ***/
/******************************************************************************
Description.: this function is called first, in order to initialise
              this plugin and pass a parameter string
Input Value.: parameters
Return Value: 0 if everything is ok, non-zero otherwise
******************************************************************************/
int output_init(output_parameter *param)
{
    int i;

    param->argv[0] = OUTPUT_PLUGIN_NAME;

    /* show all parameters for DBG purposes */
    for (i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    reset_getopt();
    while (1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"h", no_argument, 0, 0},
            {"help", no_argument, 0, 0},
            {"i", required_argument, 0, 0},
            {"input", required_argument, 0, 0},
            {"scale_num", required_argument, 0, 0},
            {"scale_denom", required_argument, 0, 0},
            {"foreground-diff-threshold", required_argument, 0, 0},
            {"background-learning-rate", required_argument, 0, 0},
            {"motion-noise-threshold", required_argument, 0, 0},
            {"motion-start-duration", required_argument, 0, 0},
            {"motion-stop-duration", required_argument, 0, 0},
            {"dump-motion-frames", no_argument, 0, 0},
            {0, 0, 0, 0}
        };

        c = getopt_long_only(
            param->argc, param->argv,
            "",
            long_options,
            &option_index
        );

        /* no more options to parse */
        if (-1 == c) break;

        /* unrecognized option */
        if ('?' == c) {
            help();
            return 1;
        }

        switch (option_index) {
        /* h, help */
        case 0:
        case 1:
            help();
            return 1;
            break;
        /* i, input */
        case 2:
        case 3:
            input_number = atoi(optarg);
            break;
        /* scale_num */
        case 4:
            scale_num = atoi(optarg);
            break;
        /* scale_denom */
        case 5:
            scale_denom = atoi(optarg);
            break;
        /* foreground-diff-threshold */
        case 6:
            foreground_diff_threshold = atoi(optarg);
            break;
        /* background-learning-rate */
        case 7:
            background_learning_rate = atof(optarg);
            break;
        /* motion-noise-threshold */
        case 8:
            motion_noise_threshold = atof(optarg);
            break;
        /* motion-start-duration */
        case 9:
            motion_start_duration = atof(optarg);
            break;
        /* motion-stop-duration */
        case 10:
            motion_stop_duration = atof(optarg);
            break;
        /* dump-motion-frames */
        case 11:
            dump_motion_frames = 1;
            break;
        }
    }

    pglobal = param->global;
    if (!(input_number < pglobal->incnt)) {
        OPRINT(
            "ERROR: %d input_plugin number >= %d number of plugins loaded\n",
            input_number,
            pglobal->incnt
        );
        return 1;
    }

    OPRINT(
        "input plugin.................................: %d: %s\n",
        input_number, pglobal->in[input_number].plugin
    );
    OPRINT(
        "image scale numerator........................: %d\n",
        scale_num
    );
    OPRINT(
        "image scale denominator......................: %d\n",
        scale_denom
    );
    OPRINT(
        "image difference threshold...................: %d\n",
        foreground_diff_threshold
    );
    OPRINT(
        "background learning rate.....................: %.3f\n",
        background_learning_rate
    );
    OPRINT(
        "motion probability noise threshold...........: %.3f\n",
        motion_noise_threshold
    );
    OPRINT(
        "time with motion to report motion started....: %.1f\n",
        motion_start_duration
    );
    OPRINT(
        "time without motion to report motion stopped.: %.1f\n",
        motion_stop_duration
    );
    OPRINT(
        "dump motion frames to stdout.................: %d\n",
        dump_motion_frames
    );

    return 0;
}

/******************************************************************************
Description.: calling this function stops the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_stop(int id)
{
    DBG("will cancel worker thread\n");
    pthread_cancel(worker);
    return 0;
}

/******************************************************************************
Description.: calling this function creates and starts the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_run(int id)
{
    DBG("launching worker thread\n");
    pthread_create(&worker, 0, worker_thread, NULL);
    pthread_detach(worker);
    return 0;
}
