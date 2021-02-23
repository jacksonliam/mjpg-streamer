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
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <limits.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <turbojpeg.h>

#include "../../utils.h"
#include "../../mjpg_streamer.h"
#include "output_modect.h"
#include "omparms.h"

globals *pglobal;

/* threads */
static pthread_t worker;
extern void *motion_status_thread();
pthread_t motion_status;
extern pthread_t event_remover;

unsigned char *jpgbuf = NULL;
int jpg_width, jpg_height;
unsigned char *imgbuf[2] = {NULL, NULL};

tjscalingfactor *tjsf;
int ntjsf;
int tj_scale_idx;

/* parameter defaults */
int debug =                  MODECT_DEBUG_DFLT;
int jpg_scale =              MODECT_JPG_SCALE_DFLT;

int modect =                 MODECT_DETECT_DFLT;
int motion =                 0;
int motion_forced =          FALSE;
int motion_forced_state;
int pixdiff =                MODECT_PIXDIFF_DFLT;
int alarmpix =               MODECT_ALARMPIX_DFLT;
int linger =                 MODECT_LINGER_DFLT;

int save_events =            MODECT_SAVE_EVENTS_DFLT;
ulong storage_limit_mb =     MODECT_STORAGE_DFLT;
int recycle =                MODECT_RECYCLE_DFLT;
int save_init =              0;
char *action_script =        NULL;
int dt_stamp =               MODECT_DTSTAMP_DFLT;
char *name =                 NULL;
int status_port =            0;

int input_idx =              0;
int output_idx;
int stat_run =               0;

#ifdef MM_FRONTEND
int cam_id =                -1;
char *dbhost =              "localhost";
int dbport =                -1;
#endif

/* mjpeg_streamer functions */
int output_run(int);
int output_stop(int);
int output_init(output_parameter *);
int output_cmd(int, unsigned int, unsigned int, int);
void worker_cleanup(void *);
void help(void);

/* external modect functions */
unsigned char *check_malloc(unsigned char *, ulong *, ulong);
int unpack_jpeg(unsigned char *, ulong, unsigned char **, ulong *, int *, int *, int, int); 
void detect_motion( unsigned char **, int, int, int, int, int, struct timeval);
void info(char *, ...);
int output_file_init(char *, char *);
void export_motion(int, char *, char *,struct timeval);
void save_jpeg_to_file(unsigned char *, ulong,  int, int, struct timeval);
char *make_name(void);
void print_valid_scale_factors(int, int);
int get_scale_index(int);


/******************************* 
 The main worker thread
*******************************/
void *worker_thread(void *arg) {
   ulong frame_size;
   ulong jbufsize = 0;
   struct timeval timestamp;
   int img_width, img_height;
   int x = 1;
   int bad_frames = 0;
   ulong image_buf_size = 0;
   int scale_idx = tj_scale_idx;

   /* set cleanup handler to cleanup allocated ressources */
   pthread_cleanup_push(worker_cleanup, NULL);

   while ( !pglobal->stop ) {

      pthread_mutex_lock(&pglobal->in[input_idx].db);
      pthread_cond_wait(&pglobal->in[input_idx].db_update, &pglobal->in[input_idx].db);

      frame_size = pglobal->in[input_idx].size;

   /* make a local copy of the frame */
      jpgbuf = check_malloc(jpgbuf, &jbufsize, frame_size+ (dt_stamp? frame_size : 0));
      memcpy(jpgbuf, pglobal->in[input_idx].buf, frame_size );
      pthread_mutex_unlock(&pglobal->in[input_idx].db);

      /* don't rely  on a valid timestamp from input plugin */
      gettimeofday(&timestamp, NULL);

      if ( scale_idx != tj_scale_idx ) {
         /* jpg_scale changed via command interface */
         if ( imgbuf[0] != NULL ) free(imgbuf[0]);
         if ( imgbuf[1] != NULL ) free(imgbuf[1]);
         imgbuf[0] = imgbuf[1] = NULL;
         scale_idx = tj_scale_idx;
      }

      /* make a scaled-down monochrome image from the jpeg for modect comparison */
      if ( unpack_jpeg(jpgbuf, frame_size, &imgbuf[x^=1], &image_buf_size, 
                        &img_width, &img_height, scale_idx, TJPF_GRAY) == -1) { 

         if ( ++bad_frames >= 5 ) {
            fprintf(stderr, "Too many bad frames; output_modect quitting\n");
         /* something wrong, eg. cam firmware stuck in bad mode; kill the whole process */
            kill(getpid(), SIGINT);
         }
         continue;
      }
      bad_frames = 0;

      detect_motion(imgbuf, img_width, img_height, pixdiff, alarmpix, linger, timestamp);
      export_motion(motion, action_script, name, timestamp);

      if ( save_events )
         save_jpeg_to_file(jpgbuf, frame_size, motion, dt_stamp, timestamp);
   }

   pthread_cleanup_pop(1);
   return NULL;
}

/************************************
 Plugin interface functions
************************************/
int output_init(output_parameter *param) {

   char *output_dir =   NULL;
   param->argv[0] = OUTPUT_PLUGIN_NAME;
   reset_getopt();

   while(1) {
      int option_index = 0, c = 0;
      static struct option long_options[] = {
         {"h", no_argument, 0, 0},
         {"help", no_argument, 0, 0},
         {"g", no_argument, 0, 0},
         {"debug", no_argument, 0, 0},
         {"p", required_argument, 0, 0},
         {"pixel_threshold", required_argument, 0, 0},
         {"a", required_argument, 0, 0},
         {"alarm_pixels", required_argument, 0, 0},
         {"j", required_argument, 0, 0},
         {"jpeg_scale", required_argument, 0, 0},
         {"l", required_argument, 0, 0},
         {"linger", required_argument, 0, 0},
         {"e", required_argument, 0, 0},
         {"exec", required_argument, 0, 0},
         {"s", no_argument, 0, 0},
         {"save_events", no_argument, 0, 0},
         {"o", required_argument, 0, 0},
         {"output_directory", required_argument, 0, 0},
         {"m", required_argument, 0, 0},
         {"mb_storage_alloc", required_argument, 0, 0},
         {"r", required_argument, 0, 0},
         {"recycle", required_argument, 0, 0},
         {"t", required_argument, 0, 0},
         {"timestamp", required_argument, 0, 0},
         {"n", required_argument, 0, 0},
         {"name", required_argument, 0, 0},
         {"sp", required_argument, 0, 0},
         {"status_port", required_argument, 0, 0},

#ifdef MM_FRONTEND
         {"c", required_argument, 0, 0},
         {"cam_id", required_argument, 0, 0},
         {"dh", required_argument, 0, 0},
         {"db_server_host", required_argument, 0, 0},
         {"dp", required_argument, 0, 0},
         {"db_server_port", required_argument, 0, 0},
#endif
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

      if ( long_options[option_index].has_arg == required_argument 
            && optarg[0] == '-' ) {
         fprintf(stderr, "option -%s requires an argument\n", long_options[option_index].name);
         return 1;
      }

      switch(option_index) {

      /* h, help */
      case 0:
      case 1:
         help();
         return 1;
         break;

      /* g, debug */
      case 2:
      case 3:
         debug = omparms[MODECT_ID_DEBUG].value = TRUE;
         break;

      /* p, pixel threshold */
      case 4:
      case 5:
         pixdiff = omparms[MODECT_ID_PIXDIFF].value = atoi(optarg);
         break;

      /* a, alarm_pixels */
      case 6:
      case 7:
         alarmpix = omparms[MODECT_ID_ALARMPIX].value = atoi(optarg);
         break;

      /* j, jpeg_scale */
      case 8:
      case 9:
         jpg_scale = omparms[MODECT_ID_JPG_SCALE].value = atoi(optarg);
         break;

      /* l, linger */
      case 10:
      case 11:
         linger = omparms[MODECT_ID_LINGER].value = atoi(optarg);
         break;

      /* e, exec */
      case 12:
      case 13:
         action_script=strdup(optarg);
         break;

      /* s, save_events */
      case 14:
      case 15:
         save_events = omparms[MODECT_ID_SAVE_EVENTS].value = TRUE;
         break;

      /* o, output_directory */
      case 16:
      case 17:
         output_dir = strdup(optarg);
         break;

      /* m, mb_storage_alloc */
      case 18:
      case 19:
         storage_limit_mb = omparms[MODECT_ID_STORAGE].value = atoi(optarg);
         break;

      /* r, recycle */
      case 20:
      case 21:
         recycle = omparms[MODECT_ID_RECYCLE].value = atoi(optarg);
         break;

      /* t, timestamp */
      case 22:
      case 23:
         dt_stamp = omparms[MODECT_ID_DTSTAMP].value = atoi(optarg);
         break;

      /* n, name */
      case 24:
      case 25:
         name = strdup(optarg);
         break;

      /* sp, status-port */
      case 26:
      case 27:
         status_port = atoi(optarg);
         break;

#ifdef MM_FRONTEND
      /* c, cam_id */
      case 28:
      case 29:
         cam_id = atoi(optarg);
         break;

      /* dh, db_server_host */
      case 30:
      case 31:
         dbhost = strdup(optarg);
         break;

      /* dp, db_server_port */
      case 32:
      case 33:
         dbport = atoi(optarg);
         break;
#endif
      }
   }

   tjsf = tjGetScalingFactors( &ntjsf ); 
   if ( (tj_scale_idx = get_scale_index(jpg_scale)) == -1) {
      fprintf(stderr, "Index not fond\n");
      return -1;
   }

   pglobal = param->global;
   output_idx = param->id;

   /* set up the command  interface */
   pglobal->out[output_idx].out_parameters = omparms;
   pglobal->out[output_idx].parametercount = sizeof(omparms)/sizeof(struct _control);

   if ( name == NULL && ( save_events==1 || action_script!=NULL || status_port!=0 ))
      name = make_name();

   OPRINT("info...................: %d\n", debug);
   OPRINT("pixel threshold........: %d\n", pixdiff);
   OPRINT("alarm pixels...........: %d\n", alarmpix);
   OPRINT("jpeg scale.............: %d (%d/%d)\n", jpg_scale, 
                  tjsf[tj_scale_idx].num, tjsf[tj_scale_idx].denom );
   OPRINT("linger.................: %d\n", linger);
   OPRINT("action script..........: %s\n", action_script);
   OPRINT("save events............: %d\n", save_events);
   if ( save_events ) {
      OPRINT("output directory.......: %s\n", output_dir);
      OPRINT("storage allocation.....: %luMb\n", storage_limit_mb);
      OPRINT("recycle................: %d%%\n", recycle);
      OPRINT("timestamp..............: %d\n", dt_stamp);
   }
   OPRINT("status_port.............: %d\n", status_port);
#ifdef MM_FRONTEND
   OPRINT("cam_id.................: %d\n", cam_id);
   OPRINT("dbhost.,,,,............: %s\n", dbhost);
   OPRINT("dbport.................: %d\n", dbport);
#endif

   if ( save_events==1 || action_script!=NULL || status_port!=0 )
      OPRINT("name...............: %s\n", name);

   if ( save_events ) {
      if ( output_file_init(output_dir, name) != 0 )
         return 1;
      save_init =1;
   }
   return 0;
}

void help(void) {
   fprintf(stderr, " ---------------------------------------------------------------\n" \
      " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
       " ---------------------------------------------------------------\n" \
      " The following parameters can be passed to this plugin (defaults):\n\n" \
      "---------------- General ----------------------------------------\n" \
      " [-g | --debug ] .....................: print info on stderr (off)\n" \
      "\n" \
     "---------------- Motion Detection--------------------------------\n" \
      " [-p | --pixel_threshold ] %%d.........: diff between compared pixels to declare an alarm pixel (%d)\n" \
      "                                         lower = more sensitive\n" \
      " [-a | --alarm_pixels ] %%d............: number of alarmed pixels required to declare motion (%d)\n" \
      "                                         lower = more sensitive\n" \
      " [-j | --jpeg_scale ] %%d..............: scale to use for internal jpg decode (1-8) (%d)\n" \
      "                                         higher = more sensitive\n" \
      " [-l | --linger ] %%d..................: time to wait before turning off motion (ms) (%d)\n" \
      "\n" \
      "---------------- Output------------------------------------------\n" \
      " [-s | --save_events ] ...............: save motion events to disk (off)\n" \
      " [-o | --output_directory ] %%s........: path for event saving (NULL)\n" \
      " [-m | --mb_storage_alloc ] %%d........: disk alloc for events in Mb (0=no limit) (%dMb)\n" \
      " [-r | --recycle ] %%d.................: recycle when storage allocation full (0)\n" \
      "                                         0 = off else %% storage to delete (oldest events removed)\n" \
      " [-t | --timestamp %%d.................: embed date&time on frames 0=off, 1-4=font size (0)\n" \
      " [-n | --name %%s......................: id for output functions (see notes for default)\n"
      " [-e | --exec ] %%s....................: path/to/script to execute on motion on/off (NULL)\n" \
      " [-sp| --status_port ] %%d.............: output motion status on this port (0)\n", \
      MODECT_PIXDIFF_DFLT, MODECT_ALARMPIX_DFLT,  MODECT_JPG_SCALE_DFLT,
       MODECT_LINGER_DFLT, MODECT_STORAGE_DFLT
   );
#ifdef MM_FRONTEND
   fprintf(stderr,
      " [-c | --cam_id ] %%d..................: surveillance sys camera id (0)\n" \
      " [-dh | --db_host ] %%s................: host where db server is running (NULL)\n" \
      " [-dp | --db_port ] %%d................: port on db_host (0)\n"
   );
#endif
   fprintf(stderr," ------------------------------------------------------------------\n");
}

/**************************************************
 Kill all threads
 NB. Doesn't get called on (p)kill 
 to close properly use (p)kill -INT
**************************************************/
int output_stop(int id) {
/* close open event, if there is one open */
   info("Closing open event\n");

   struct timeval ts;
   gettimeofday(&ts, NULL);

   if ( save_events )
      save_jpeg_to_file(NULL, 0, 0, 0, ts);

#ifdef MM_FRONTEND
   /* close in-progress event if any */
   export_motion(0, action_script, name, ts);

   if ( status_port ) {
      info("cancelling motion status thread\n");
      pthread_cancel(motion_status);
   }
#endif

   if ( recycle ) {
      info("cancelling filesystem recycle thread\n");
      pthread_cancel(event_remover);
   }
   info("cancelling modect worker thread\n");
   pthread_cancel(worker);

   return 0;
}

/************************************
 Create & start worker thread
************************************/
int output_run(int id) {
   info("launching modect worker thread\n");
   pthread_create(&worker, 0, worker_thread, NULL);
   pthread_detach(worker);

/* create & start motion status output thread */
   if ( status_port  ) {
      info("launching motion status thread\n");
      pthread_create(&motion_status, 0, &motion_status_thread, NULL);
      pthread_detach(motion_status);
   }

   return 0;
}

void worker_cleanup(void *arg) {
   static unsigned char first_run = 1;

   if ( !first_run ) {
      info("already cleaned up resources\n");
      return;
   }
   first_run = 0;
   OPRINT("cleaning up ressources allocated by modect thread\n");
   if ( jpgbuf != NULL) free(jpgbuf);
   if ( imgbuf[0] != NULL) free(imgbuf[0]);
   if ( imgbuf[1] != NULL) free(imgbuf[1]);
}

/******************************************************************************
 Process commands, allows to dynamically set plugin parameters
 control specifies the selected parameter's id
 value is new value of parameter.
******************************************************************************/
int output_cmd(int plugin, unsigned int control, unsigned int group, int value) {
   int idx;
   info("output_cmd plugin=%d control=%d group=%d value=%d\n", plugin, control, group, value);

   if ( control >= pglobal->out[output_idx].parametercount ) {
      fprintf(stderr, "Invalid control: %d\n", control);
      return -1;
   }

   if ( value < omparms[control].ctrl.minimum ||
         value > omparms[control].ctrl.maximum ) {
      fprintf(stderr, "Value %d out of bounds: min %d max %d)\n",
         control, omparms[control].ctrl.minimum, omparms[control].ctrl.maximum);
      return -1;
   }

   if ( omparms[control].ctrl.flags != MODECT_FLG_DYNAMIC ) {
      fprintf(stderr, "Parameter %d %s not dynamically settable\n\n",
                     control, omparms[control].ctrl.name);
      return -1;
   }

   switch( control ) {

   case MODECT_ID_DEBUG:
      debug = omparms[MODECT_ID_DEBUG].value = value;
      info("setting debug %s\n", value==0 ? "off" : "on");
      break;

   case MODECT_ID_DETECT:
      modect = omparms[MODECT_ID_DETECT].value = value;
      motion_forced = FALSE;
      info("setting motion detection %s\n", value==0 ? "off" : "on");
      break;

   case MODECT_ID_JPG_SCALE:
      if ( (idx = get_scale_index(value)) != -1) { 
         jpg_scale = omparms[MODECT_ID_JPG_SCALE].value = value;
         info("setting jpg_scale %d (%d/%d)\n", 
               jpg_scale, tjsf[idx].num, tjsf[idx].denom);
         tj_scale_idx = idx;
      }
      else
         fprintf(stderr, "invalid scale %d\n", value);

      break;

   case MODECT_ID_PIXDIFF:
      pixdiff = omparms[MODECT_ID_PIXDIFF].value = value;
      info("setting pixel threshold to %d\n", value);
      break;

   case MODECT_ID_ALARMPIX:
      alarmpix = omparms[MODECT_ID_ALARMPIX].value = value;
      info("setting alarmpix to %d\n", value);
      break;

   case MODECT_ID_LINGER:
      linger = omparms[MODECT_ID_LINGER].value = value;
      info("setting linger to %d millisecs\n", value);
      break;

   case MODECT_ID_MOTION:
      motion_forced = TRUE;
      motion_forced_state = value;
      modect = omparms[MODECT_ID_DETECT].value = 1;
      info("forcing motion  %s\n", value==0 ? "off" : "on");
      break;

   case MODECT_ID_SAVE_EVENTS:
      if ( value == 1 && !save_init ) 
         fprintf(stderr, "File saving must be initialized at startup\n");
      else {
         save_events = omparms[MODECT_ID_SAVE_EVENTS].value = value;
         info("setting save_events %s\n", value==0 ? "off" : "on");
      }
      break;

   case MODECT_ID_STORAGE:
      storage_limit_mb = omparms[MODECT_ID_STORAGE].value = value;
      info("setting storage limit to %dMb\n", value);
      break;

   case MODECT_ID_RECYCLE:
      recycle = omparms[MODECT_ID_RECYCLE].value = value;
      info("will delete a minimum of %d%% of storage when full\n", value);
      break;

   case MODECT_ID_DTSTAMP:
      {
         char *fsiz[] = {"off", "8x8", "12x16", "16x20", "24x30"};
         dt_stamp = omparms[MODECT_ID_DTSTAMP].value = value;
         info("setting timestamp %s\n", fsiz[value]);
      }
      break;

   case MODECT_ID_STAT:
      if (stat_run) {
         fprintf(stderr, "stat already running\n");
         break;
      }
      stat_run = omparms[MODECT_ID_STAT].value = 2;
      info("stat test to run on next close event\n");
      break;

   default:
      fprintf(stderr, "Unsupported command\n");
   }
   return 0;
}
