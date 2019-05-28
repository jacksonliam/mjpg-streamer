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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>

#include <errno.h>

#include "input_event.h"
#include "../../mjpg_streamer.h"
#include "../../utils.h"

#define INPUT_PLUGIN_NAME "input_event"

#define ONE_BILLION 1000000000
#define EVENT_TIMEOUT 610

/* private functions and variables to this plugin */
static pthread_t   worker;
static globals     *pglobal;

void *worker_thread(void *);
void worker_cleanup(void *);
void help(void);

static char *directory = NULL;
static int plugin_number;

/* global variables for this plugin */
int input_idx;
int speed = EVENT_SPEED_DFLT;
int frame = -1, jump_frame;
int nframes = 0;
int mode, prev_mode;
struct dirent **filelist;

/* we die if no one sending commands */
void command_timeout() {
	kill(getpid(), SIGINT); /* quit entire process */
}

void *chk_malloc( unsigned char *buf, int *current_size, ulong new_size) {
	unsigned char *retbuf = buf;
	if ( retbuf == NULL ) 
		retbuf = malloc( *current_size=new_size );
	else if ( new_size > *current_size )
		retbuf = realloc(retbuf, *current_size=new_size);

	return retbuf;
}

/* timespec math funcs in case embedded */

/* subtract timespec y from x */
struct timespec timespec_sub(struct timespec x, struct timespec y){
	struct timespec result; 
	if( x.tv_sec > y.tv_sec ){
		if( x.tv_nsec > y.tv_nsec ){
			result.tv_sec = x.tv_sec - y.tv_sec;
			result.tv_nsec = x.tv_nsec - y.tv_nsec;
		}
		else {
			result.tv_sec = x.tv_sec - y.tv_sec - 1;
			result.tv_nsec = ONE_BILLION - y.tv_nsec + x.tv_nsec;
		}
	}
	else {
		if( x.tv_sec == y.tv_sec ){
			result.tv_sec = 0;
			if( x.tv_nsec > y.tv_nsec ){
				result.tv_nsec = x.tv_nsec - y.tv_nsec;
			}
			else {
				result.tv_nsec = y.tv_nsec - x.tv_nsec;
			}
		}
		else {
			if( x.tv_nsec > y.tv_nsec ){
				result.tv_sec = y.tv_sec - x.tv_sec - 1;
				result.tv_nsec = ONE_BILLION - x.tv_nsec + y.tv_nsec;
			}
			else {
				result.tv_sec = y.tv_sec - x.tv_sec;
				result.tv_nsec = y.tv_nsec - x.tv_nsec;
			}
		}
	}
	return result;
}

/* multiply struct timespec 'x' by n, result returned in 'result' */
struct timespec timespec_mult(struct timespec x, int n){
	int i;
	struct timespec result = {0,0};

	result.tv_sec = x.tv_sec * n;
	for ( i=0; i<n; i++ ) {
		result.tv_nsec += x.tv_nsec;
		if ( result.tv_nsec > ONE_BILLION ) {
			result.tv_nsec -= ONE_BILLION;
			result.tv_sec++;
		}
	}
	return result;
}

/* divide struct timespec 'x' by n */
struct timespec timespec_div(struct timespec x, int n){
	struct timespec result;

	result.tv_sec = x.tv_sec / n;

	result.tv_nsec = x.tv_nsec/n + ONE_BILLION/n * (x.tv_sec%n);
	while ( result.tv_nsec > ONE_BILLION ) {
		result.tv_nsec -= ONE_BILLION;
		result.tv_sec++;
	}
	return result;
}

int scandir_filter(const struct dirent *entry) {
	int rc;
	struct stat s;
	char buf[1024];

	char *p = (char *)entry->d_name + strlen(entry->d_name)-4;

	/* skip file if not a jpeg */
	if ( strcmp(p, ".jpg") )
		return 0;

	sprintf(buf, "%s/%s", directory, entry->d_name);
	rc = stat(buf, &s);
	if(rc == -1) {
		fprintf(stderr,"stat failed on %s: %s\n", buf, strerror(errno));
		return 0;
	}

	if ( (s.st_mode & S_IFMT) != S_IFREG )
		return 0;

	return 1;
}

/*** plugin interface functions ***/
int input_init(input_parameter *param, int id) {
	int i;
	plugin_number = id;

	param->argv[0] = INPUT_PLUGIN_NAME;

	reset_getopt();
	while(1) {
		int option_index = 0, c = 0;
		static struct option long_options[] = {
			{"h", no_argument, 0, 0},
			{"help", no_argument, 0, 0},
			{"d", required_argument, 0, 0},
			{"directory", required_argument, 0, 0},
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

			/* d, directory */
		case 2:
		case 3:
			DBG("case 2,3\n");
			directory = strdup(optarg);
			break;

		default:
			DBG("default case\n");
			help();
			return 1;
		}
	}

	/* check for required parameters */
	if(directory == NULL) {
		IPRINT("ERROR: no folder specified\n");
		return 1;
	}

/* get list of jpegs in given directory */
	nframes = scandir(directory, &filelist, scandir_filter, versionsort);
	if ( nframes < 0 ) {
		fprintf(stderr, "Error reading %s: %s\n", directory, strerror(errno));
		return 1;
	}
	else if ( nframes == 0 ) {
		fprintf(stderr, "No jpeg files found in %s\n", directory);
		return 1;
	}

	IPRINT("directory of event.....: %s\n", directory);

	pglobal = param->global;
	input_idx = param->id;
	pglobal->in[input_idx].in_parameters = ieparms;
	pglobal->in[input_idx].parametercount = sizeof(ieparms)/sizeof(struct _control);

	mode = ieparms[EVENT_ID_PLAY_MODE].value;
	ieparms[EVENT_ID_FRAME].ctrl.maximum = nframes-1;

	signal(SIGALRM, command_timeout);
	alarm(EVENT_TIMEOUT);

	return 0;
}

int input_stop(int id) {
	DBG("will cancel input thread\n");
	pthread_cancel(worker);
	return 0;
}

int input_run(int id) {
	pglobal->in[id].buf = NULL;

	if(pthread_create(&worker, 0, worker_thread, NULL) != 0) {
		free(pglobal->in[id].buf);
		fprintf(stderr, "could not start worker thread\n");
		exit(EXIT_FAILURE);
	}

	pthread_detach(worker);

	return 0;
}

/*** private functions for this plugin below ***/
void help(void){

	fprintf(stderr, " ---------------------------------------------------------------\n" \
	" Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
	" ---------------------------------------------------------------\n" \
	" [-d | --directory ].......: directory with sequential jpegs to play back\n");
}

void frame_delay(struct timespec x, struct timespec y, int speed) {
	struct timespec delay, rem;

	if ( y.tv_sec==0 && y.tv_nsec==0 ) 
		return; 

	delay = timespec_sub(x, y);

	if ( speed < -1 )
		delay = timespec_mult(delay, -speed);
	else if ( speed > 1 )
		delay = timespec_div(delay, speed);

	nanosleep(&delay, &rem);
}

/* the single worker thread */
void *worker_thread(void *arg) {
	struct stat fst;
	int fd;
	char buf[1024];
	struct timespec rem, idle_timer = {0,50000000}; /* 0 2 sec */
	struct timespec frametime, last_frametime = {0,0};

	/* set cleanup handler to cleanup allocated ressources */
	pthread_cleanup_push(worker_cleanup, NULL);

	while( !pglobal->stop  ) {

		while ( mode == EVENT_MODE_PAUSE ) 
			nanosleep(&idle_timer, &rem);

		switch( mode ) {

			case EVENT_MODE_PLAY:
				/* If we're at the end, pause it there */
				if ( (ieparms[EVENT_ID_FRAME].value = ++frame) >= nframes ) {
					ieparms[EVENT_ID_FRAME].value = frame = nframes-1;
					mode = ieparms[EVENT_ID_PLAY_MODE].value = EVENT_MODE_PAUSE;
					continue;
				}
				/* set time to wait before sending next frame */
				frame_delay(frametime, last_frametime, speed);								
				last_frametime = frametime;
				break;

			case EVENT_MODE_STEP_FWD:
				mode = ieparms[EVENT_ID_PLAY_MODE].value = EVENT_MODE_PAUSE;
				if ( (ieparms[EVENT_ID_FRAME].value = ++frame) >= nframes ) {
					ieparms[EVENT_ID_FRAME].value = frame = nframes-1;
					continue;
				}
				last_frametime = frametime;
				break;

			case EVENT_MODE_STEP_BACK:
				mode = ieparms[EVENT_ID_PLAY_MODE].value = EVENT_MODE_PAUSE;
				if ( (ieparms[EVENT_ID_FRAME].value = --frame) < 0 ) {
					ieparms[EVENT_ID_FRAME].value = frame = 0;
					continue;
				}
				last_frametime = frametime;
				break;

			case EVENT_MODE_JUMP:
				/* user clicked on the progress bar */
				frame = ieparms[EVENT_ID_FRAME].value = jump_frame;
				mode = ieparms[EVENT_ID_PLAY_MODE].value = prev_mode;
				last_frametime.tv_sec = last_frametime.tv_nsec = 0;
				break;

			default:
				fprintf(stderr, "Unknown  mode\n");
		}

		/* next frame filename */		
		sprintf(buf, "%s/%s", directory, filelist[frame]->d_name);
		fd = open(buf, O_RDONLY);
		if(fd == -1) {
			fprintf(stderr, "could not open file %s: %s\n",buf, strerror(errno));
			break;
		}
		fstat(fd, &fst); /* assume status ok since it was previously stat'ed */ 

		/* file modification/creation time */

		/* This assumes the file mod time hasn't been altered.
		   This is accurate, but breakable; alternative is to
			use an average fps. Maybe do an option.
		*/
		frametime.tv_sec = fst.st_mtim.tv_sec;
		frametime.tv_nsec = fst.st_mtim.tv_nsec;

		pthread_mutex_lock(&pglobal->in[plugin_number].db);

		/* allocate memory for frame, if needed */
		pglobal->in[plugin_number].buf = 
				chk_malloc(pglobal->in[plugin_number].buf, 
							&pglobal->in[plugin_number].size, fst.st_size);

		if(pglobal->in[plugin_number].buf == NULL) {
			fprintf(stderr, "could not allocate memory\n");
			break;
		}

		/* read frame from file to global buffer */
		if((pglobal->in[plugin_number].size = read(fd, pglobal->in[plugin_number].buf, fst.st_size)) == -1) {
			fprintf(stderr, "read error on %s: %s\n", buf, strerror(errno));
			free(pglobal->in[plugin_number].buf);
			pglobal->in[plugin_number].buf = NULL;
			pglobal->in[plugin_number].size = 0;
			pthread_mutex_unlock(&pglobal->in[plugin_number].db);
			close(fd);
			break;
		}

		/* signal fresh_frame */
		pthread_cond_broadcast(&pglobal->in[plugin_number].db_update);
		pthread_mutex_unlock(&pglobal->in[plugin_number].db);

		close(fd);
	}

	/* call cleanup handler, signal with the parameter */
	pthread_cleanup_pop(1);
	return NULL;
}

void worker_cleanup(void *arg) {
	static unsigned char first_run = 1;

	if(!first_run) {
		DBG("already cleaned up ressources\n");
		return;
	}

	first_run = 0;
	DBG("cleaning up ressources allocated by input thread\n");

	if(pglobal->in[plugin_number].buf != NULL) free(pglobal->in[plugin_number].buf);
	kill(getpid(), SIGINT); /* quit entire process */
}

/* Playback controls from external source */
int input_cmd(int plugin, unsigned int control, unsigned int group, int value) {
	if ( control >= pglobal->in[input_idx].parametercount ) {
		fprintf(stderr, "Invalid control: %d\n", control);
		return -1;
	}
	if ( value < ieparms[control].ctrl.minimum ||
			value > ieparms[control].ctrl.maximum ) {
		fprintf(stderr, "Value out of bounds: %d (%d<>%d)\n", control, ieparms[control].ctrl.minimum,
				ieparms[control].ctrl.maximum);
		return -1;
	}
	if ( ieparms[control].ctrl.flags != EVENT_FLG_DYNAMIC ) {
		fprintf(stderr, "Parameter %d %s not dynamic\n\n", control, ieparms[control].ctrl.name);
		return -1;
	}

	alarm(EVENT_TIMEOUT); 		/* reset timeout */

	switch( control ) {

	case EVENT_ID_PLAY_MODE:
		mode = ieparms[EVENT_ID_PLAY_MODE].value = value;
		break;

	case EVENT_ID_STEP:
		mode = value > 0 ? EVENT_MODE_STEP_FWD : EVENT_MODE_STEP_BACK;
		break;

	case EVENT_ID_SPEED:
		if ( value == 1 )
			speed = ( ++speed==0 ) ? 2 : MIN(speed, 5);
		else if ( value == -1 )
			speed = ( --speed==0 ) ? -2 : MAX(speed, -5);
		ieparms[EVENT_ID_SPEED].value = speed;
		mode = ieparms[EVENT_ID_PLAY_MODE].value = EVENT_MODE_PLAY;
		break;

	case EVENT_ID_FRAME:
		jump_frame = value;
		prev_mode = mode;
		mode = EVENT_MODE_JUMP;
		break;

	case EVENT_ID_STOP:
		kill(getpid(), SIGINT);
		break;

	default:
		fprintf(stderr, "Unsupported command\n");
	}
	return 0;
}

