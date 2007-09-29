#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>

#include "../../utils.h"
#include "../../mjpg_streamer.h"
#include "../input.h"

#define INPUT_PLUGIN_NAME "FILE input plugin"
#define MAX_ARGUMENTS 32

/* private functions and variables to this plugin */
pthread_t   worker;
globals     *global;

void *worker_thread( void *);
void worker_cleanup(void *);
void help(void);

/*** plugin interface functions ***/
int input_init(input_parameter *param) {
  char *argv[MAX_ARGUMENTS]={NULL};
  int argc=1, i;

  /* convert the single parameter-string to an array of strings */
  argv[0] = INPUT_PLUGIN_NAME;
  if ( param->parameter_string != NULL && strlen(param->parameter_string) != 0 ) {
    char *arg, *saveptr, *token;

    arg=(char *)strdup(param->parameter_string);

    if ( strchr(arg, ' ') != NULL ) {
      token=strtok_r(arg, " ", &saveptr);
      if ( token != NULL ) {
        argv[argc] = strdup(token);
        argc++;
        while ( (token=strtok_r(NULL, " ", &saveptr)) != NULL ) {
          argv[argc] = strdup(token);
          argc++;
          if (argc >= MAX_ARGUMENTS) {
            IPRINT("ERROR: too many arguments to input plugin\n");
            return 1;
          }
        }
      }
    }
  }

  /* show all parameters for DBG purposes */
  for (i=0; i<argc; i++) {
      DBG("argv[%d]=%s\n", i, argv[i]);
  }

  reset_getopt();
  while(1) {
    int option_index = 0, c=0;
    static struct option long_options[] = \
    {
      {"h", no_argument, 0, 0},
      {"help", no_argument, 0, 0},
      {"f", required_argument, 0, 0},
      {"folder", required_argument, 0, 0},
      {0, 0, 0, 0}
    };

    c = getopt_long_only(argc, argv, "", long_options, &option_index);

    /* no more options to parse */
    if (c == -1) break;

    /* unrecognized option */
    if (c == '?'){
      help();
      return 1;
    }

    switch (option_index) {
      /* h, help */
      case 0:
      case 1:
        DBG("case 0,1\n");
        help();
        return 1;
        break;

      /* d, device */
      case 2:
      case 3:
        DBG("case 2,3\n");
        folder = strdup(optarg);
        break;

      default:
        DBG("default case\n");
        help();
        return 1;
    }
  }

  global = param->global;

  /* allocate webcam datastructure */
  videoIn = (struct vdIn *) calloc(1, sizeof(struct vdIn));

  IPRINT("Using V4L2 device.: %s\n", dev);
  IPRINT("Resolution........: %i x %i\n", width, height);
  IPRINT("frames per second.: %i\n", fps);

  /* open video device and prepare data structure */
  if (init_videoIn(videoIn, dev, width, height, fps, V4L2_PIX_FMT_MJPEG, 1) < 0) {
    fprintf(stderr, "init_VideoIn failed\n");
    exit(1);
  }

  return 0;
}

int input_stop(void) {
  DBG("will cancel input thread\n");
  pthread_cancel(cam);

  return 0;
}

int input_run(void) {
  global->buf = (unsigned char *) calloc(1, (size_t)videoIn->framesizeIn);
  if (global->buf == NULL) {
    fprintf(stderr, "could not allocate memory\n");
    exit(1);
  }

  pthread_create(&cam, 0, cam_thread, NULL);
  pthread_detach(cam);

  return 0;
}

/*** private functions for this plugin below ***/
void help(void) {
    fprintf(stderr, " ---------------------------------------------------------------\n" \
                    " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
                    " ---------------------------------------------------------------\n" \
                    " The following parameters can be passed to this plugin:\n\n" \
                    " [-d | --device ].......: video device to open (your camera)\n" \
                    " [-r | --resolution ]...: 960x720, 640x480, 320x240, 160x120\n" \
                    " [-f | --fps ]..........: frames per second\n" \
                    " ---------------------------------------------------------------\n");
}

int is_huffman(unsigned char *buf)
{
    unsigned char *ptbuf;
    int i = 0;
    ptbuf = buf;
    while (((ptbuf[0] << 8) | ptbuf[1]) != 0xffda) {
        if (i++ > 2048)
            return 0;
        if (((ptbuf[0] << 8) | ptbuf[1]) == 0xffc4)
            return 1;
        ptbuf++;
    }
    return 0;
}

#if 0
int print_picture(int fd, unsigned char *buf, int size)
{
    unsigned char *ptdeb, *ptcur = buf;
    int sizein;

    if (!is_huffman(buf)) {
        ptdeb = ptcur = buf;
        while (((ptcur[0] << 8) | ptcur[1]) != 0xffc0)
            ptcur++;
        sizein = ptcur - ptdeb;
        if( write(fd, buf, sizein) <= 0) return -1;
        if( write(fd, dht_data, DHT_SIZE) <= 0) return -1;
        if( write(fd, ptcur, size - sizein) <= 0) return -1;
    } else {
        if( write(fd, ptcur, size) <= 0) return -1;
    }
    return 0;
}
#endif

int memcpy_picture(unsigned char *out, unsigned char *buf, int size)
{
    unsigned char *ptdeb, *ptcur = buf;
    int sizein, pos=0;

    if (!is_huffman(buf)) {
        ptdeb = ptcur = buf;
        while (((ptcur[0] << 8) | ptcur[1]) != 0xffc0)
            ptcur++;
        sizein = ptcur - ptdeb;

        memcpy(out+pos, buf, sizein); pos += sizein;
        memcpy(out+pos, dht_data, DHT_SIZE); pos += DHT_SIZE;
        memcpy(out+pos, ptcur, size - sizein); pos += size-sizein;
    } else {
        memcpy(out+pos, ptcur, size); pos += size;
    }
    return pos;
}

/* the single writer thread */
void *cam_thread( void *arg ) {
  /* set cleanup handler to cleanup allocated ressources */
  pthread_cleanup_push(cam_cleanup, NULL);

  while( !global->stop ) {
    /* DBG("grabbing frame\n"); */

    /* grab a frame */
    if( uvcGrab(videoIn) < 0 ) {
      fprintf(stderr, "Error grabbing frames\n");
      exit(1);
    }

    /* copy JPG picture to global buffer */
    pthread_mutex_lock( &global->db );
    global->size = memcpy_picture(global->buf, videoIn->tmpbuffer, videoIn->buf.bytesused);

#if 0
    /* motion detection can be done just by comparing the picture size, but it is not very accurate!! */
    if ( (prev_size - global->size)*(prev_size - global->size) > 4*1024*1024 ) {
        DBG("motion detected (delta: %d kB)\n", (prev_size - global->size) / 1024);
    }
    prev_size = global->size;
#endif

    /* signal fresh_frame */
    pthread_cond_broadcast(&global->db_update);
    pthread_mutex_unlock( &global->db );

    /* only use usleep if the fps is below 5, otherwise the overhead is too long */
    if ( videoIn->fps < 5 ) {
      usleep(1000*1000/videoIn->fps);
    }
  }

  DBG("leaving input thread, calling cleanup function now\n");
  pthread_cleanup_pop(1);

  return NULL;
}

void cam_cleanup(void *arg) {
  static unsigned char first_run=1;

  if ( !first_run ) {
    DBG("already cleaned up ressources\n");
    return;
  }

  first_run = 0;
  DBG("cleaning up ressources allocated by input thread\n");

  close_v4l2(videoIn);
  if (videoIn->tmpbuffer != NULL) free(videoIn->tmpbuffer);
  if (videoIn != NULL) free(videoIn);
  if (global->buf != NULL) free(global->buf);
}




