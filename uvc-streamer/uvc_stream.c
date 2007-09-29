/*******************************************************************************
# "uvc_stream" is a command line application to stream JPEG files over an      #
# IP-based network from the webcam to a viewer like Firefox, Cambozola,        #
# Videolanclient or even to a Windows Mobile device running the TCPMP-Player.  #
#                                                                              #
# It was written for embedded devices with very limited ressources in terms of #
# RAM and CPU. The decision for Linux-UVC was done, because supported cameras  #
# directly produce JPEG-data, allowing fast and perfomant M-JPEG streams even  #
# from slow embedded devices like those supported by OpenWRT.                  #
#                                                                              #
# I would suggest not to open this server to the internet. Use it as input to  #
# the programm "motion" [http://motion.sf.net] running at your DMZ instead.    #
# Motion has many users and i expect it to be checked more often for security  #
# issues. Keep in mind, that motions advanced capabilties like                 #
# motion-detection, writing of avifiles etc require much more ressources.      #
#                                                                              #
# In contrast to the better known SPCA5XX-LE, UVC-cameras in average produce   #
# better image quality (See Michel XHaards comparison table and rating at      #
# his site) [http://mxhaard.free.fr/embedded.html].                            #
#                                                                              #
# This programm was written in 2007 by Tom Stöveken, basing on luvcview.       #
# The luvcview sources were modified using the tool "indent" and afterwards    #
# SDL dependencies were removed to reduce dependencies to other packages.      #
#                                                                              #
# This package work with the Logitech UVC based webcams with the mjpeg feature.#
#                                                                              #
#     Copyright (C) 2005 2006 Laurent Pinchart &&  Michel Xhaard               #
#     Copyright (C) 2007      Tom Stöveken                                     #
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

#include "control.h"
#include "v4l2uvc.h"
#include "utils.h"

#define SOURCE_VERSION "1.3"
#define BOUNDARY "arflebarfle"

typedef enum { SNAPSHOT, STREAM } answer_t;

/* globals */
int stop=0, sd;
int force_delay=0;
struct control_data cd, *cdata;

/* signal fresh frames */
pthread_mutex_t db        = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  db_update = PTHREAD_COND_INITIALIZER;

/* global JPG frame, this is more or less the "database" */
unsigned char *g_buf = NULL;
int g_size = 0;

/* thread for clients that connected to this server */
void *client_thread( void *arg ) {
  int fd = *((int *)arg);
  fd_set fds;
  unsigned char *frame = (unsigned char *)calloc(1, (size_t)cd.videoIn->framesizeIn);
  int ok = 1, frame_size=0;
  char buffer[1024] = {0};
  struct timeval to;
  answer_t answer = STREAM;

  if (arg!=NULL) free(arg); else exit(1);

  /* set timeout to 5 seconds */
  to.tv_sec  = 5;
  to.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  if( select(fd+1, &fds, NULL, NULL, &to) <= 0) {
    close(fd);
    free(frame);
    return NULL;
  }

  /* find out if we should deliver something other than a stream */
  read(fd, buffer, sizeof(buffer));
  if ( strstr(buffer, "snapshot") != NULL ) {
    answer = SNAPSHOT;
  }

  if( answer == SNAPSHOT) {
    sprintf(buffer, "HTTP/1.0 200 OK\r\n" \
                    "Server: UVC Streamer\r\n" \
                    "Content-type: image/jpeg\r\n"
                    "\r\n");
    cd.snapshot = 0;                
  } else {
    sprintf(buffer, "HTTP/1.0 200 OK\r\n" \
                    "Server: UVC Streamer\r\n" \
                    "Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n" \
                    "Cache-Control: no-cache\r\n" \
                    "Cache-Control: private\r\n" \
                    "Pragma: no-cache\r\n" \
                    "\r\n" \
                    "--" BOUNDARY "\n");
  }
  ok = ( write(fd, buffer, strlen(buffer)) >= 0)?1:0;

  while ( ok >= 0 && !stop ) {

    /* having a problem with windows (do we not always) browsers not updating the 
       stream display, unless the browser cache is disabled - try and implement a delay
       to allow movement to end before streem goes on - kind of works, but not well enough */
       
    if (cd.moved > 0){
      SLEEP(1,0);
      cd.moved = 0;
    }
    /* wait for fresh frames */
    pthread_cond_wait(&db_update, &db);

    /* read buffer */
    frame_size = g_size;
    memcpy(frame, g_buf, frame_size);

    pthread_mutex_unlock( &db );

    if ( answer == STREAM ) {
      sprintf(buffer, "Content-type: image/jpeg\n\n");
      ok = ( write(fd, buffer, strlen(buffer)) >= 0)?1:0;
      if( ok < 0 ) break;
    }
    
    ok = print_picture(fd, frame, frame_size);
    if( ok < 0 || answer == SNAPSHOT ) break;

    sprintf(buffer, "\n--" BOUNDARY "\n");
    ok = ( write(fd, buffer, strlen(buffer)) >= 0)?1:0;
    if( ok < 0 ) break;
  }
  
  close(fd);
  free(frame);

  return NULL;
}

/* the single writer thread */
void *cam_thread( void *arg ) {
  while( !stop ) {
    /* grab a frame */
    if( uvcGrab(cd.videoIn) < 0 ) {
      fprintf(stderr, "Error grabbing\n");
      exit(1);
    }

    /* copy frame to global buffer */
    pthread_mutex_lock( &db );

    g_size = cd.videoIn->buf.bytesused;
    memcpy(g_buf, cd.videoIn->tmpbuffer, cd.videoIn->buf.bytesused);

    /* signal fresh_frame */
    pthread_cond_broadcast(&db_update);
    pthread_mutex_unlock( &db );

    /* only use usleep if the fps is below 5, otherwise the overhead is too long */
    if ( cd.videoIn->fps < 5 ) {
      usleep(1000*1000/cd.videoIn->fps);
    }
  }

  return NULL;
}

void help(char *progname)
{
  fprintf(stderr, "------------------------------------------------------------------\n");
  fprintf(stderr, "Usage: %s\n" \
                  " [-h | --help ]........: display this help\n" \
                  " [-d | --device ]......: video device to open (your camera)\n" \
                  " [-r | --resolution ]..: 960x720, 640x480, 320x240, 160x120\n" \
                  " [-f | --fps ].........: frames per second\n" \
                  " [-p | --port ]........: TCP-port for the server\n" \
                  " [-c | --control_port ]: TCP-port for the motor control server\n" \
                  
                  " [-v | --version ].....: display version information\n" \
                  " [-b | --background]...: fork to the background, daemon mode\n" \
                  " --disable_control.....: disable the motor control server\n", progname);
  fprintf(stderr, "------------------------------------------------------------------\n");
}

void signal_handler(int sigm) {
  /* signal "stop" to threads */
  stop = 1;

  /* cleanup most important structures */
  fprintf(stderr, "shutdown...\n");
  usleep(1000*1000);
  close_v4l2(cd.videoIn);
  free(cd.videoIn);
  if (close (sd) < 0)
	  perror ("close sd");;
  pthread_cond_destroy(&db_update);
  pthread_mutex_destroy(&db);
  exit(0);
  return;
}

void daemon_mode(void) {
  int fr=0;

  fr = fork();
  if( fr < 0 ) {
    fprintf(stderr, "fork() failed\n");
    exit(1);
  }
  if ( fr > 0 ) {
    exit(0);
  }

  if( setsid() < 0 ) {
    fprintf(stderr, "setsid() failed\n");
    exit(1);
  }

  fr = fork();
  if( fr < 0 ) {
    fprintf(stderr, "fork() failed\n");
    exit(1);
  }
  if ( fr > 0 ) {
    fprintf(stderr, "forked to background (%d)\n", fr);
    exit(0);
  }

  umask(0);

  chdir("/");
  close(0);
  close(1);
  close(2);

  open("/dev/null", O_RDWR);
  dup(0);
  dup(0);
}

/* #########################################################################
Main
######################################################################### */
int main(int argc, char *argv[])
{
  struct sockaddr_in addr;
  int on=1, disable_control_port = 0;
  pthread_t client, cam, cntrl;
  char *dev = "/dev/video0";
  int fps=5, daemon=0;


  cd.width=640;
  cd.height=480;
   
  cdata = &cd;  
  cd.control_port = htons(8081);
  cd.stream_port = htons(8080);
  while(1) {
    int option_index = 0, c=0;
    static struct option long_options[] = \
    {
      {"h", no_argument, 0, 0},
      {"help", no_argument, 0, 0},
      {"d", required_argument, 0, 0},
      {"device", required_argument, 0, 0},
      {"r", required_argument, 0, 0},
      {"resolution", required_argument, 0, 0},
      {"f", required_argument, 0, 0},
      {"fps", required_argument, 0, 0},
      {"p", required_argument, 0, 0},
      {"port", required_argument, 0, 0},
      {"v", no_argument, 0, 0},
      {"version", no_argument, 0, 0},
      {"b", no_argument, 0, 0},
      {"background", no_argument, 0, 0},
      {"c", required_argument, 0, 0},
      {"control_port", required_argument, 0, 0},
      {"disable_control", no_argument, 0, 0},
      {0, 0, 0, 0}
    };

    c = getopt_long_only(argc, argv, "", long_options, &option_index);

    /* no more options to parse */
    if (c == -1) break;

    /* unrecognized option */
    if(c=='?'){ help(argv[0]); return 0; }

    switch (option_index) {
      /* h, help */
      case 0:
      case 1:
        help(argv[0]);
        return 0;
        break;

      /* d, device */
      case 2:
      case 3:
        dev = strdup(optarg);
        break;

      /* r, resolution */
      case 4:
      case 5:
        if ( strcmp("960x720", optarg) == 0 ) { cd.width=960; cd.height=720; }
        else if ( strcmp("640x480", optarg) == 0 ) { cd.width=640; cd.height=480; }
        else if ( strcmp("320x240", optarg) == 0 ) { cd.width=320; cd.height=240; }
        else if ( strcmp("160x120", optarg) == 0 ) { cd.width=160; cd.height=120; }
        else { 
              fprintf(stderr, "ignoring unsupported resolution\n");
        }      
        break;

      /* f, fps */
      case 6:
      case 7:
        fps=atoi(optarg);
        break;

      /* p, port */
      case 8:
      case 9:
        cd.stream_port=htons(atoi(optarg));
        break;

      /* v, version */
      case 10:
      case 11:
        printf("UVC Streamer Version: %s\n" \
               "Compilation Date....: %s\n" \
               "Compilation Time....: %s\n", SOURCE_VERSION, __DATE__, __TIME__);
        return 0;
        break;

      /* v, version */
      case 12:
      case 13:
        daemon=1;
        break;
        
      /* c, control_port */
      case 14:
      case 15:
        cd.control_port=htons(atoi(optarg));
        break;
      /* disable_control */  
      case 16:
        disable_control_port = 1;
        break;
      default:
        help(argv[0]);
        return 0;
    }
  }

  /* ignore SIGPIPE (send if transmitting to closed sockets) */
  signal(SIGPIPE, SIG_IGN);
  if (signal(SIGINT, signal_handler) == SIG_ERR) {
    fprintf(stderr, "could not register signal handler\n");
    exit(1);
  }

  /* fork to the background */
  if ( daemon ) {
    daemon_mode();
  }

  /* allocate webcam datastructure */
  cd.videoIn = (struct vdIn *) calloc(1, sizeof(struct vdIn));

  fprintf(stderr, "Using V4L2 device.....: %s\n", dev);
  fprintf(stderr, "Resolution............: %i x %i\n", cdata->width, cdata->height);
  fprintf(stderr, "frames per second.....: %i\n", fps);
  fprintf(stderr, "TCP port..............: %i\n", ntohs(cd.stream_port));
  if (disable_control_port == 1){
    fprintf(stderr, "motor control server..: disabled\n");
  } else {
    fprintf(stderr, "motor control TCP port: %i\n", ntohs(cd.control_port));
  }  

  /* open video device and prepare data structure */
  cd.video_dev = init_videoIn(cd.videoIn, dev, cd.width, cd.height, fps, V4L2_PIX_FMT_MJPEG, 1);
  if (cd.video_dev < 0) {
    fprintf(stderr, "init_VideoIn failed\n");
    exit(1);
  }
  
  /* open socket for server */
  sd = socket(PF_INET, SOCK_STREAM, 0);
  if ( sd < 0 ) {
    fprintf(stderr, "socket failed\n");
    exit(1);
  }

  /* ignore "socket already in use" errors */
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    perror("setsockopt(SO_REUSEADDR) failed");
    exit(1);
  }

  /* configure server address to listen to all local IPs */
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = cd.stream_port;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if ( bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 ) {
    fprintf(stderr, "bind failed\n");
    perror("Bind");
    exit(1);
  }

  /* start listening on socket */
  if ( listen(sd, 10) != 0 ) {
    fprintf(stderr, "listen failed\n");
    exit(1);
  }

  /* start to read the camera, push picture buffers into global buffer */
  cd.videoIn->tmpbuffer = (unsigned char *) calloc(1, (size_t)cd.videoIn->framesizeIn);
                  g_buf = (unsigned char *) calloc(1, (size_t)cd.videoIn->framesizeIn);
  pthread_create(&cam, 0, cam_thread, NULL);
  pthread_detach(cam);

  /* start motor control server */
  if (disable_control_port == 0){
    pthread_create(&cntrl, NULL, &uvcstream_control, cdata);
    pthread_detach(cntrl);
  }  

  /* create a child for every client that connects */
  while ( 1 ) {
    int *pfd = (int *)calloc(1, sizeof(int));
    *pfd = accept(sd, 0, 0);
    pthread_create(&client, NULL, &client_thread, pfd);
    pthread_detach(client);
    
  }

  return 0;
}
