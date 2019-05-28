/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#      output_modect Copyright (C) 2019 Jim Allingham                          #
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
#include <turbojpeg.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>

#include "../../mjpg_streamer.h"
#include "../../utils.h"
#include "output_modect.h"
#include "fonts.c"

#define MOTION_TRANSITION(m,lm) (m!=lm && ((m==0) || (m==1 && lm!=2)))

globals *pglobal;

extern int debug;
extern int modect, motion_forced, motion_forced_state;
extern struct _control omparms[];
extern int motion;
extern ulong tot_event_blocks;
extern int status_port;
extern pthread_t motion_status;
extern int input_idx;
extern int dt_stamp;
extern int jpg_width, jpg_height;
extern tjscalingfactor *tjsf;
extern int ntjsf;

/***************************************
 Print info messages if debug turned on
***************************************/
void info(char *format, ...) {
   if ( debug ) {
      va_list vl;
      va_start(vl, format);
      vfprintf(stderr, format, vl);
      va_end(vl);
   }
}

/*****************************************
 Allocate / reallocate buffer memory
******************************************/
unsigned char *check_malloc(unsigned char *buf, ulong *current_size, ulong new_size) {

   if ( buf == NULL ) 
         buf = malloc(*current_size=new_size);

   else if ( new_size > *current_size )
         buf = realloc(buf, *current_size = new_size);

   if ( buf == NULL ) {
      fprintf(stderr, "Out of Memory\n");
      kill(getpid(), SIGINT);     /* shut it all down */
   }
   return buf;
}

/* find the least common multiple of two numbers */
int lcm(int a, int b) {
   int a1 = a, b1 = b;
   while (b1 != 0) {
      int tmp = b1;
      b1 = a1 % b1;
      a1 = tmp;
   }
   return a/a1 * b;
}

/*****************************************************
 Return the index into turbojpeg's scaling factors
 corresponding to scale. jpeg_scale's range is 1-8
 because that is the range of tj's scale factors.
 However, we abstract away the relationship since
 tj's documentation seems to indicate that
 this could be subject to change. 
*****************************************************/
int get_scale_index(int scale) {
   int i, hd, lcd, idx, a, b, c;

   if ( scale < 1 || scale > 8 ) 
      return -1;         /* no point in making it bigger */

   for ( hd=i=0; i<ntjsf; i++) {
      /* highest denom in tj's scale factors */
      if ( tjsf[i].denom > hd ) hd = tjsf[i].denom;
   }

   lcd = lcm(8, hd);  /* lowest common denom between our 8 and tj's denom */ 

   for ( c=1000, idx=i=0 ; i< ntjsf; i++ ) {
      /* find the nearest tj factor to our scale/8 */
      a = scale * (lcd/8);
      b = tjsf[i].num * (lcd/tjsf[i].denom);
      if ( abs(a-b) < c ) { 
         idx = i;
         c = abs(a-b);
      }
   }
   return idx;
}

/***************************************************
 Convert jpeg frame to raw image using libturbojpeg
 Returns 0 on succes, -1 on error
****************************************************/
int unpack_jpeg(unsigned char *jbuf, ulong jbufsize, unsigned char **image_buf,
             ulong *image_buf_size, int *img_width, int *img_height, 
               int scale_idx, int pixelFormat) {

   int jpegSubsamp;
   ulong ibuf_size;
   int pitch;

   tjhandle handle = tjInitDecompress();
   if ( handle == NULL ) goto unpack_err_ret;

   if ( tjDecompressHeader2(handle, jbuf, jbufsize, 
                  &jpg_width, &jpg_height, &jpegSubsamp) == -1)
      goto unpack_err_ret;
 
   *img_width = TJSCALED(jpg_width, tjsf[scale_idx]);
   *img_height = TJSCALED(jpg_height, tjsf[scale_idx]);

   pitch = *img_width * tjPixelSize[pixelFormat];

   if ( *image_buf == NULL || image_buf_size != NULL ) {
      if ( image_buf_size != NULL ) ibuf_size = *image_buf_size;
      *image_buf = check_malloc( *image_buf, &ibuf_size, *img_height * pitch);
      if ( image_buf_size != NULL ) *image_buf_size = ibuf_size;
   }

   if (tjDecompress2(handle, jbuf, jbufsize, *image_buf, 
                        *img_width, 0, *img_height, pixelFormat, TJFLAG_FASTDCT) == -1)
      goto unpack_err_ret;

   tjDestroy(handle);
   return 0;

unpack_err_ret:
   fprintf(stderr, "%s\n", tjGetErrorStr() );
   tjDestroy(handle);
   return -1;
}

/*********************************************
 Create jpeg from rgb image using libturbojpeg
 Returns 0 on success, -1 on error
**********************************************/
int pack_jpeg(unsigned char *imgbuf, int w, int h, unsigned char **jpgbuf, ulong *jpgsize) {

   tjhandle handle = tjInitCompress();
   if ( handle == NULL ) goto pack_err_ret; 

   if (tjCompress2(handle, imgbuf, w, 0, h, TJPF_RGB,
                  jpgbuf, jpgsize, TJSAMP_444, 80, TJFLAG_FASTDCT|TJFLAG_NOREALLOC ) == -1)
      goto pack_err_ret;

   if ( tjDestroy(handle) != -1)
      return 0;

pack_err_ret:
   fprintf(stderr, "%s\n", tjGetErrorStr() );
   return -1;
}

/**********************************************************
 Put the date & time onto the frame.
 Since this involves unpacking and then repacking the jpeg,
 it *could* cause frame dropping, depending on frame size,
 frame rate, CPU horse power, etc.
 On success returns the new frame size.
 On error, returns the original frame size.
***********************************************************/
ulong embed_timestamp( unsigned char *jbuf, ulong framesize, struct timeval timestamp ) {
   int xoff=2, yoff=2;
   int w, h, bpc, bpr, bmidx, bit, i, t, x, r;
   unsigned char *p;
   char txt[100];
   char *bm;
   ulong jpeg_size = framesize;
   static unsigned char *imgbuf = NULL;
   int bmwidth, bmheight;
   static int first_pass = 1;
   static int scale_idx;

   /* select the font */
   bmwidth = bmwh[dt_stamp-1][0];
   bmheight = bmwh[dt_stamp-1][1];
   bm = bms[dt_stamp-1];

   if ( first_pass ) {
      scale_idx = get_scale_index(8);  /* full scale jpeg */
      first_pass = 0;
   }

   if ( unpack_jpeg(jbuf, framesize, &imgbuf, NULL, &w, &h, scale_idx, TJPF_RGB) == -1 )
      return framesize;

   struct tm *timeofday = localtime(&timestamp.tv_sec);
   sprintf(txt, "%02d/%02d/%02d %02d:%02d:%02d",
         timeofday->tm_year-100, timeofday->tm_mon+1, timeofday->tm_mday,
         timeofday->tm_hour, timeofday->tm_min, timeofday->tm_sec);   

   for (r=0; r<=bmheight; r++)                  /* black background */
      memset(&imgbuf[xoff*3 + (r+yoff-1)*w*3], 0, strlen(txt)*bmwidth*3);

   bpr = bmwidth/8 + (bmwidth%8  ? 1 : 0);      /* raster bytes per row */
   bpc = bpr * bmheight;                        /* raster bytes per char */

   for ( t=0; t<strlen(txt); t++ ) {            /* for each char */
      if ( txt[t] == ' ' )                      /* space char is a no-op */
         continue;

      /* index into font bitmap - we only have the chars '/', ':' and numbers */
      bmidx = (txt[t] == '/' ? 0 : txt[t] == ':' ? 1 : 2 + txt[t]- 0x30) * bpc;      

      for ( r=0; r<bmheight; r++ ) {                /* for each raster row */
         for ( i=x=0; i<bpr; i++ ) {                /* for each byte this row */
            for ( bit=1; bit<=0x80; bit*=2, x++ ) { /* for each bit this byte */
               if ( bit & bm[bmidx + r*bpr +i ] ) { /* if font bit is 1 */
                                                    /* set the image pixel */   
                  p = imgbuf + (t*bmwidth+x+xoff)*3 + (r+yoff)*w*3;
                  *p++ = 0xff;      /* red */
                  *p++ = 0xff;      /* green */
                  *p = 0xff;        /* blue */
               }
            }
         }
      }
   }

   if ( pack_jpeg(imgbuf, w, h, &jbuf, &jpeg_size) == -1)
      return framesize;

   return jpeg_size;
}

/***************************************** 
 Execute the action script
*****************************************/
void execute_action(char *action_script, char *name) {
   pid_t child;

   child = fork();
   if ( !child ) {
      child = fork();
      if (!child) {
         info("execute \"%s %s %s\"\n", action_script, name, motion ? "1" : "0");
         execl(action_script, action_script, name, motion ? "1" : "0", NULL);
         fprintf(stderr, "Failed to execute %s %s\n", action_script, strerror(errno));
      }
      else
         exit(0);
   }
   else
      waitpid(child, NULL, 0);
}

/***************************************
 Export motion status
***************************************/
void export_motion(int motion, char * action_script, char *name, struct timeval timestamp) {
   static int last_motion = -1, last_tp_motion = -1;;

   if ( status_port ) {
      if ( motion != last_tp_motion) {  
         /* signal status thread to update */ 
         pthread_kill(motion_status, SIGUSR1);
         last_tp_motion = motion;
      }
   }

   /* execute action script on startup and motion transition to 0 or 1 */
   if ( action_script != NULL ) {
      if ( (last_motion == -1) || MOTION_TRANSITION(motion, last_motion) ) {
         execute_action(action_script, name);
         last_motion = motion;
      }
   }
}

/******************************************** 
 Do the thing we're named for
 Compares current frame with previous frame
    motion
      0: no motion
      1: motion
      2: lingering
********************************************/
void detect_motion( unsigned char **image_buf,
               int width, int height, int pixdiff, int alarmpix, 
               int linger, struct timeval timestamp ) {
   static ulong linger_start;
   unsigned char *p0 = image_buf[0];
   unsigned char *p1 = image_buf[1];
   int i, bufsize = width*height;
   unsigned nalarmpix = 0;

   ulong frametime = timestamp.tv_sec*1000 + timestamp.tv_usec/1000;

   /* on first pass one of these will be NULL */

   if ( p0 == NULL || p1 == NULL )
   return;

   if (modect) {      /* can be turned off via command interface */
      if ( motion_forced ) { 
          /* motion forced on/off via command interface */
         motion = motion_forced_state;
         modect = omparms[MODECT_ID_DETECT].value = 0;
         info("motion forced to %d, turning analysis (modect) off\n", motion);
         info("re-enable by sending 'modect 1' to control interface\n");
      }

      else {

        /* do the motion detection */
         for ( i=0; i<bufsize; i++ ) {
            int diff = abs(*(p1++) - *(p0++));
            if ( diff > pixdiff )
               nalarmpix++;
            if ( nalarmpix >= alarmpix )
               break;
         }

         if ( nalarmpix >= alarmpix ) {      /* if motion detected */
            if ( motion != 1 ) {
               info("motion on\n");
               motion = 1;
            }
         }

         else if ( motion ) {                 /* motion not detected && prev state is 1 or 2 */
            if ( motion == 1 ) {
               linger_start = frametime;      /* was 1 so start lingering */
               motion = 2;
               info("lingering\n");
            }
            else if ( (frametime - linger_start) >= linger ) { 
               motion = 0;                     /* linger time over */
               info("motion off\n");
            }
         }
      }
   }
   omparms[MODECT_ID_MOTION].value = motion;
}

/********************************************************* 
 Try to make a unique name indicating the mjpeg source
*********************************************************/
char *make_name() {
   char *name = NULL;
   int i;

   /* if the input plugin is input_uvc, use the device name */
   if ( !strcmp(pglobal->in[input_idx].plugin, "input_uvc.so")) {
      for ( i=0; i<pglobal->in[input_idx].param.argc; i++ ) {
         if ( !strcmp("-d", pglobal->in[input_idx].param.argv[i]) ||
             !strcmp("--device", pglobal->in[input_idx].param.argv[i]) ) {   
            name = strdup(basename(pglobal->in[input_idx].param.argv[i+1]));
            break;
         }
      }
   }

   /* if the input plugin is input_http, use format "http:host:port" */
   else if ( !strcmp(pglobal->in[input_idx].plugin, "input_http.so")) {
      char httphost[100];
      int got=0;
      strcpy(httphost,"http:");

      for ( i=0; i<pglobal->in[input_idx].param.argc; i++ ) {

         if ( !strcmp("-H", pglobal->in[input_idx].param.argv[i]) ||
             !strcmp("--host", pglobal->in[input_idx].param.argv[i]) ) {
            strcat(httphost,basename(pglobal->in[input_idx].param.argv[i+1]));
            if ( ++got == 1) strcat(httphost, ":");
         }

         else if ( !strcmp("-p", pglobal->in[input_idx].param.argv[i]) ||
             !strcmp("--port", pglobal->in[input_idx].param.argv[i]) ) {
             strcat(httphost,basename(pglobal->in[input_idx].param.argv[i+1]));
            if ( ++got == 1 ) strcat(httphost, ":");
         }
      }
      if ( got>0 ) name=strdup(httphost);
   }

   /* else use the input plugin name - not guaranteed to be unique across all instances */
   if ( name == NULL )
      name = strdup(pglobal->in[input_idx].plugin);
   return name;
}

