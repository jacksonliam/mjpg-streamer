/* 
*      control.c
*
*      HTTP Control interface for uvc_stream
*      
*      2007 Lucas van Staden (lvs@softhome.net) 
*
*      Some of this code is based on the webhttpd.c code from the motion project
*
*      Copyright 2004-2005 by Angel Carpintero  (ack@telefonica.net)
*      This software is distributed under the GNU Public License Version 2
*
*/

#define SLEEP(seconds, nanoseconds) {              \
                struct timespec tv;                \
                tv.tv_sec = (seconds);             \
                tv.tv_nsec = (nanoseconds);        \
                while (nanosleep(&tv, &tv) == -1); \
        } 

#define INCPANTILT 64 

  struct control_data {
    int control_port, stream_port;
    struct vdIn *videoIn;
    int width, height;
    int minmaxfound, panmin, tiltmin, panmax, tiltmax, pan_angle, tilt_angle;
    int moved, video_dev, snapshot;
  };

struct coord {
	int x;
	int y;
	int width;
	int height;
	int minx;
	int maxx;
	int miny;
	int maxy;
};


void * uvcstream_control(void *arg);
void control(struct control_data *);
int uvc_move(struct coord *cent, struct control_data *cd);
int uvc_reset(struct control_data *cp, int mode);
