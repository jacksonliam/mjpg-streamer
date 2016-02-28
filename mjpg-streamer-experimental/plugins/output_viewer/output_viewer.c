/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2008 Tom Stöveken                                         #
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

#include <SDL/SDL.h>
#include <jpeglib.h>


#include "../../utils.h"
#include "../../mjpg_streamer.h"

#define OUTPUT_PLUGIN_NAME "VIEWER output plugin"

static pthread_t worker;
static globals *pglobal;
static unsigned char *frame = NULL;
static int input_number = 0;

/******************************************************************************
Description.: print a help message
Input Value.: -
Return Value: -
******************************************************************************/
void help(void)
{
    fprintf(stderr, " ---------------------------------------------------------------\n" \
            " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
            " ---------------------------------------------------------------\n");
}

/******************************************************************************
Description.: clean up allocated resources
Input Value.: unused argument
Return Value: -
******************************************************************************/
void worker_cleanup(void *arg)
{
    static unsigned char first_run = 1;

    if(!first_run) {
        DBG("already cleaned up resources\n");
        return;
    }

    first_run = 0;
    OPRINT("cleaning up resources allocated by worker thread\n");

    free(frame);
    SDL_Quit();
}

typedef struct {
    struct jpeg_source_mgr pub;

    Uint8 *jpegdata;
    int jpegsize;
} my_source_mgr;

static void init_source(j_decompress_ptr cinfo)
{
    return;
}

static int fill_input_buffer(j_decompress_ptr cinfo)
{
    my_source_mgr * src = (my_source_mgr *) cinfo->src;

    src->pub.next_input_byte = src->jpegdata;
    src->pub.bytes_in_buffer = src->jpegsize;

    return TRUE;
}

static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    my_source_mgr * src = (my_source_mgr *) cinfo->src;

    if(num_bytes > 0) {
        src->pub.next_input_byte += (size_t) num_bytes;
        src->pub.bytes_in_buffer -= (size_t) num_bytes;
    }
}

static void term_source(j_decompress_ptr cinfo)
{
    return;
}

static void jpeg_init_src(j_decompress_ptr cinfo, Uint8 *jpegdata, int jpegsize)
{
    my_source_mgr *src;

    if(cinfo->src == NULL) {  /* first time for this JPEG object? */
        cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(my_source_mgr));
        src = (my_source_mgr *) cinfo->src;
    }

    src = (my_source_mgr *) cinfo->src;
    src->pub.init_source = init_source;
    src->pub.fill_input_buffer = fill_input_buffer;
    src->pub.skip_input_data = skip_input_data;
    src->pub.resync_to_restart = jpeg_resync_to_restart;
    src->pub.term_source = term_source;
    src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
    src->pub.next_input_byte = NULL; /* until buffer loaded */

    src->jpegdata = jpegdata;
    src->jpegsize = jpegsize;
}

static void my_error_exit(j_common_ptr cinfo)
{
    DBG("JPEG data contains an error\n");
}

static void my_error_output_message(j_common_ptr cinfo)
{
    DBG("JPEG data contains an error\n");
}

typedef struct {
    int height;
    int width;
    unsigned char *buffer;
    int buffersize;
} decompressed_image;

int decompress_jpeg(unsigned char *jpeg, int jpegsize, decompressed_image *image)
{
    struct jpeg_decompress_struct cinfo;
    JSAMPROW rowptr[1];
    struct jpeg_error_mgr jerr;

    /* create an error handler that does not terminate MJPEG-streamer */
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = my_error_exit;
    jerr.output_message = my_error_output_message;

    /* create the decompressor structures */
    jpeg_create_decompress(&cinfo);

    /* initalize the structures of decompressor */
    jpeg_init_src(&cinfo, jpeg, jpegsize);

    /* read the JPEG header data */
    if(jpeg_read_header(&cinfo, TRUE) < 0) {
        jpeg_destroy_decompress(&cinfo);
        DBG("could not read the header\n");
        return 1;
    }

    /*
     * I just expect RGB colored JPEGs, so the num_components must be three
     */
    if(cinfo.num_components != 3) {
        jpeg_destroy_decompress(&cinfo);
        DBG("unsupported number of components (~colorspace)\n");
        return 1;
    }

    /* just use RGB output and adjust decompression parameters */
    cinfo.out_color_space = JCS_RGB;
    cinfo.quantize_colors = FALSE;
    /* to scale the decompressed image, the fraction could be changed here */
    cinfo.scale_num   = 1;
    cinfo.scale_denom = 1;
    cinfo.dct_method = JDCT_FASTEST;
    cinfo.do_fancy_upsampling = FALSE;

    jpeg_calc_output_dimensions(&cinfo);

    /* store the image information */
    image->width = cinfo.output_width;
    image->height = cinfo.output_height;

    /*
     * just allocate a new buffer if not already allocated
     * pay a lot attention, that the calling function has to ensure, that the buffer
     * must be large enough
     */
    if(image->buffer == NULL) {
        image->buffersize = image->width * image->height * cinfo.num_components;
        /* the calling function has to ensure that this buffer will become freed after use! */
        image->buffer = malloc(image->buffersize);
        if(image->buffer == NULL) {
            jpeg_destroy_decompress(&cinfo);
            DBG("allocating memory failed\n");
            return 1;
        }
    }

    /* start to decompress */
    if(jpeg_start_decompress(&cinfo) < 0) {
        jpeg_destroy_decompress(&cinfo);
        DBG("could not start decompression\n");
        return 1;
    }

    while(cinfo.output_scanline < cinfo.output_height) {
        rowptr[0] = (JSAMPROW)(Uint8 *)image->buffer + cinfo.output_scanline * image->width * cinfo.num_components;

        if(jpeg_read_scanlines(&cinfo, rowptr, (JDIMENSION) 1) < 0) {
            jpeg_destroy_decompress(&cinfo);
            DBG("could not decompress this line\n");
            return 1;
        }
    }

    if(jpeg_finish_decompress(&cinfo) < 0) {
        jpeg_destroy_decompress(&cinfo);
        DBG("could not finish compression\n");
        return 1;
    }

    /* all is done */
    jpeg_destroy_decompress(&cinfo);

    return 0;
}

/******************************************************************************
Description.: this is the main worker thread
              it loops forever, grabs a fresh frame, decompressed the JPEG
              and displays the decoded data using SDL
Input Value.:
Return Value:
******************************************************************************/
void *worker_thread(void *arg)
{
    int frame_size = 0, firstrun = 1;

    SDL_Surface *screen = NULL, *image = NULL;
    decompressed_image rgbimage;

    /* initialze the buffer for the decompressed image */
    rgbimage.buffersize = 0;
    rgbimage.buffer = NULL;

    /* initialze the SDL video subsystem */
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    /* just allocate a large buffer for the JPEGs */
    if((frame = malloc(4096 * 1024)) == NULL) {
        OPRINT("not enough memory for worker thread\n");
        exit(EXIT_FAILURE);
    }

    /* set cleanup handler to cleanup allocated resources */
    pthread_cleanup_push(worker_cleanup, NULL);

    while(!pglobal->stop) {
        DBG("waiting for fresh frame\n");
        pthread_mutex_lock(&pglobal->in[input_number].db);
        pthread_cond_wait(&pglobal->in[input_number].db_update, &pglobal->in[input_number].db);

        /* read buffer */
        frame_size = pglobal->in[input_number].size;
        memcpy(frame, pglobal->in[input_number].buf, frame_size);

        pthread_mutex_unlock(&pglobal->in[input_number].db);

        /* decompress the JPEG and store results in memory */
        if(decompress_jpeg(frame, frame_size, &rgbimage)) {
            DBG("could not properly decompress JPEG data\n");
            continue;
        }

        if(firstrun) {
            /* create the primary surface (the visible window) */
            screen = SDL_SetVideoMode(rgbimage.width, rgbimage.height, 0, SDL_ANYFORMAT | SDL_HWSURFACE);
            SDL_WM_SetCaption("MJPG-Streamer Viewer", NULL);

            /* create a SDL surface to display the data */
            image = SDL_AllocSurface(SDL_SWSURFACE, rgbimage.width, rgbimage.height, 24,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
                                     0x0000FF, 0x00FF00, 0xFF0000,
#else
                                     0xFF0000, 0x00FF00, 0x0000FF,
#endif
                                     0);

            /* copy the decoded data across */
            memcpy(image->pixels, rgbimage.buffer, rgbimage.width * rgbimage.height * 3);
            free(rgbimage.buffer);

            /* now, that we know the dimensions, we can directly copy to the right surface */
            rgbimage.buffer = image->pixels;
            rgbimage.buffersize = rgbimage.width * rgbimage.height * 3;

            firstrun = 0;
        }

        /* copy the image to the primary surface */
        SDL_BlitSurface(image, NULL, screen, NULL);

        /* redraw the whole surface */
        SDL_Flip(screen);
    }

    pthread_cleanup_pop(1);

    /* get rid of the image */
    SDL_FreeSurface(image);

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
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    reset_getopt();
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"h", no_argument, 0, 0
            },
            {"help", no_argument, 0, 0},
            {"i", required_argument, 0, 0},
            {"input", required_argument, 0, 0},
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
            /* h, help */
        case 0:
        case 1:
            DBG("case 0,1\n");
            help();
            return 1;
            break;
            /* i, input */
        case 2:
        case 3:
            DBG("case 2,3\n");
            input_number = atoi(optarg);
            break;
        }
    }

    pglobal = param->global;
    if(!(input_number < pglobal->incnt)) {
        OPRINT("ERROR: the %d input_plugin number is too much only %d plugins loaded\n", input_number, pglobal->incnt);
        return 1;
    }
    OPRINT("input plugin.....: %d: %s\n", input_number, pglobal->in[input_number].plugin);

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

int output_cmd()
{


}

