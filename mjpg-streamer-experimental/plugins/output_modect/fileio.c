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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <ftw.h>
#include <libgen.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <turbojpeg.h>
#include <dirent.h>
#include <ctype.h>
#include <stdint.h>

#include "../../mjpg_streamer.h"
#include "output_modect.h"

#ifdef MM_FRONTEND
#include "/usr/local/src/mmsrc/mmdbd/mmdbd.h"
#endif

#define MB_USED (tot_event_blocks/2048)

void info(char *, ...);

extern char *output_dir;
extern int storage_limit_mb;
extern int recycle;
extern int save_events;
extern int cam_id;
extern int status_port;
extern int debug;
extern char *dbhost;
extern int dbport;
extern int stat_run;

static pthread_t rootdir_stat;
pthread_t event_remover;
static pthread_mutex_t event_size_mutex;
extern pthread_t motion_status;

extern globals *pglobal;
extern struct _control omparms[];
static char rootdir[500];
static uint64_t tot_event_blocks;
static uint64_t removed_blocks;
static uint64_t stat_blocks;
static int fs_blksize;
static ulong maxeventnum, mineventnum;
static uint current_event = 0;

int unpack_jpeg(unsigned char *, unsigned long, unsigned char **, int *, int *, int, int); 
int pack_jpeg(unsigned char *, int, int, int, unsigned char **, ulong *);
ulong embed_timestamp( unsigned char *, ulong, struct timeval );

#ifdef MM_FRONTEND
int dbsock;

/* don't quit on SIGPIPE socket error */
void pipe_trap() {
   info("EPIPE trapped\n");
}

/*********************************************
 Send an event info to the datbase server
*********************************************/
int send_to_db_server(char *buffer, int bufsize) {
   int tot_sent = 0, nsent;

    while (tot_sent < bufsize) {
        if ( (nsent = write(dbsock, &buffer[tot_sent], bufsize - tot_sent)) == -1) {
         fprintf(stderr, "db server socket write failed: %s\n", strerror(errno));
         return -1;
        }
        tot_sent += nsent;
    }
   return 0;
}

/**************************************
 Connect to the database server mmdbd
**************************************/
int connect_to_db_server() {
   struct sockaddr_in sock_addr;
   struct hostent *server;
   int one=1;

   if ( (dbsock = socket (PF_INET, SOCK_STREAM, 0)) == -1) {
      fprintf(stderr, "create socket failed: %s\n", strerror(errno));
      return -1;
   }
   setsockopt(dbsock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

   if ( (server = gethostbyname(dbhost)) == NULL) {
      fprintf(stderr, "gethost failed: %s\n", strerror(errno));
      return -1;
   }

   sock_addr.sin_family = AF_INET;
   memcpy(&sock_addr.sin_addr.s_addr, server->h_addr, server->h_length);
   sock_addr.sin_port = htons(dbport);

   if ( connect(dbsock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) == -1) {
      fprintf(stderr, "socket connect failed: %s\n", strerror(errno));
      return -1;
   }

   return 0;
}
#endif

/*********** recycle functions *******************/

int isnumber(char *str) {
   int i;
   for ( i=0; str[i]!=0; i++ ) {
      if ( !isdigit((unsigned char)str[i]) )
         return 0;
   }
   return 1;
}

/*************************************************
 Find event number in directory path
 Return Value: Event number or -1 if not found.
**************************************************/
static int event_from_path(const char *path) {

#ifdef MM_FRONTEND
   if ( isnumber(basename((char *)path)) ) 
      return strtol(basename((char *)path), NULL, 10);
   else
      return -1;
#else
   char *buf;
   if ( (buf = strstr(path, "Event-")) != NULL ) {
      int eventnum;
      char *ebuf = NULL;
      buf = &buf[strlen("Event-")];
      errno = 0;
      eventnum = strtol(buf, &ebuf, 10);
      if ( errno == 0 && ebuf != buf ) 
         return eventnum;
   }
   return -1;
#endif
}

uint minmax_minevent;
uint minmax_maxevent;

/* scandir filter for minmax function */
int minmax_filter(const struct dirent *d) {

   int event = event_from_path(d->d_name);
   if ( event != -1 ) {
      if ( event < minmax_minevent )
         minmax_minevent = event;
      if ( event > minmax_maxevent )
         minmax_maxevent = event;
      if ( event == minmax_minevent || event == minmax_maxevent )
         return 1;
   }
   return 0;
}

/****************************************************
 Find the mininmum and maximum existing events
 and their directory paths
 Any arg other than event_dir can be NULL
****************************************************/
void minmax(char *event_dir, char *mindir, ulong *min, char *maxdir, ulong *max) {
   struct dirent **namelist;
   char buf[100];
   int i;

   minmax_minevent = UINT_MAX;
   minmax_maxevent = 0;

   int n = scandir(event_dir, &namelist, minmax_filter, versionsort);
   
   if ( mindir != NULL ) {
      sprintf(buf, "%d", minmax_minevent);
      for ( i=0; i<n; i++ ) {
         if ( strstr(namelist[i]->d_name, buf) != NULL ) {
            sprintf(mindir, "%s/%s", rootdir, namelist[i]->d_name);         
            break;
         }
      }
   }

   if ( maxdir != NULL ) {
      sprintf(buf, "%d", minmax_maxevent);
      for ( i=0; i<n; i++ ) {
         if ( strstr(namelist[i]->d_name, buf) != NULL ) {
            sprintf(maxdir, "%s/%s", rootdir, namelist[i]->d_name);         
            break;
         }
      }
   }

   if ( minmax_minevent == UINT_MAX ) minmax_minevent = 0;
   if ( min != NULL ) *min = minmax_minevent;
   if ( max != NULL ) *max = minmax_maxevent;

   while (n--) 
      free(namelist[n]);
   free(namelist);
}

/******************************************************************************
Description.: nftw (file tree walk) callback to delete directory tree
               processes files before directories
Input Value.: fpath: root of tree to delete
Return Value: always FTW_CONTINUE (nftw returns 0)
******************************************************************************/
static int delete_tree_callback(const char *fpath,
                                 const struct stat *finfo,
                                 int typeflag,
                                 struct FTW *ftwbuf) {
   int rv;

   if ( typeflag == FTW_D || typeflag == FTW_DP )
      rv = rmdir(fpath);  /* remove (now empty) directory */
   else
      rv = unlink(fpath); /* remove file */

   if ( rv != 0 )
      fprintf(stderr, "dt callback Failed to remove %s\n%s\n", fpath, strerror(errno));
   else 
      removed_blocks += finfo->st_blocks;
   return FTW_CONTINUE;
}

/******************************************************************************
Description.: nftw callback; used once on startup
              Stats dir tree for total size
               processes directories before files
Input Value.: see 'man nftw' 
Return Value: Always FTW_CONTINUE (nftw returns 0)
              sets tot_event_blocks and fs block size
******************************************************************************/
struct timespec max_stat;
ulong check_blocks;
static int nftw_callback_stat(const char *fpath, 
                              const struct stat *finfo,
                              int typeflag,
                              struct FTW *ftwbuf) {
   if ( typeflag == FTW_D ) {               /* is a directory */

      if ( ftwbuf->level == 0 )             /* is the root event dir */
         fs_blksize = finfo->st_blksize;    /* grab the filesystem block size */

      else {                                /* single event directory */

         if ( (finfo->st_mtim.tv_sec > max_stat.tv_sec)
            || ( (finfo->st_mtim.tv_sec == max_stat.tv_sec) &&
               (finfo->st_mtim.tv_nsec>max_stat.tv_nsec) ) ) {

            return FTW_SKIP_SUBTREE;
         }
         stat_blocks += finfo->st_blocks;
      }
   }
   else   /* it's a file */
      stat_blocks += finfo->st_blocks;
   usleep(5000);            /* no real hurry, so don't swamp the system */
   return FTW_CONTINUE;
}

void rootdir_stat_cleanup(void *arg) {}

/******************************************
   Thread to get event rootdir size
   Result is the same as 'du -B 512 rootdir'
******************************************/
static void *rootdir_stat_thread() {

   pthread_cleanup_push(rootdir_stat_cleanup, NULL);

   if ( stat_run == 2 )    /* a just-closed event dir often won't */ 
      sleep(2);            /* stat correctly for mtim without this */

   stat_run++;
   stat_blocks = 0;

   /* walks the file tree, calling nftw_callback_stat for each entry */
   if ( nftw(rootdir, nftw_callback_stat, 10, FTW_ACTIONRETVAL) != 0)
      fprintf(stderr, "stat_thread: Couldn't stat %s\n%s\n", rootdir, strerror(errno));

   else if ( stat_run == 1 ) {

      /* this is the startup run*/
      pthread_mutex_lock(&event_size_mutex);
      tot_event_blocks += stat_blocks;
      pthread_mutex_unlock(&event_size_mutex);
      info("***stat'd %lu blocks in %s\n", stat_blocks, rootdir);
   }

   else {

   /* this is a test run invoked by sending 'stat 1' to the control interface */
   /* if the result is ever non zero, it's a bug in the file size accounting  */
      info("stat_blocks = %lu check_blocks = %lu\n", stat_blocks, check_blocks);
      if ( stat_blocks > check_blocks )
         info("***stat test: (stat_blocks - check_blocks) = %lu\n", stat_blocks - check_blocks);
      else
         info("***stat test: (check_blocks - stat_blocks) = %lu\n", check_blocks - stat_blocks);

   }

   stat_run = omparms[MODECT_ID_STAT].value = 0;
   pthread_cleanup_pop(1);
   return NULL;
}

void event_remover_cleanup(void *arg) {
   info("event_remover_cleanup\n");
}

/*******************************************************
 Thread to remmove oldest events (recycling)
 When storage limit reached, deletes events until
 space used is <= than DELETE_TO_PERCENT
*******************************************************/
static void *event_remover_thread() {
   
   char min_event_dir[1024];
   int delete_to_percent;
   uint limit;

   pthread_cleanup_push(event_remover_cleanup, NULL);

   while ( !pglobal->stop ) {

      /* don't allow these to be changed during delete loop (via control interface) */
      delete_to_percent = 100 - recycle;
      limit = storage_limit_mb;

      if ( MB_USED > limit ) {

         info("%lu Mb used, deleting to %dMb\n", MB_USED, limit * delete_to_percent/100 );

         while ( MB_USED > (limit * delete_to_percent/100) ) {

            if ( mineventnum+1 == current_event ) 
               break;          /* don't delete the open event */

            /* find the oldest event on disk */
            minmax(rootdir, min_event_dir, &mineventnum, NULL, NULL);

            info("Removing event %lu %s\n", mineventnum, min_event_dir);

            /* delete it */
            removed_blocks = 0;
            if ( nftw(min_event_dir, delete_tree_callback, 4, FTW_DEPTH) != 0) 
               fprintf(stderr, "nftw error, dir not deleted %s\n%s\n", 
                        min_event_dir, strerror(errno));

            else {
               /* update our running size count */
               if ( removed_blocks > tot_event_blocks )      /* shouldn't happen */
                  removed_blocks = tot_event_blocks;         /* but just in case */

               pthread_mutex_lock(&event_size_mutex);
               tot_event_blocks -= removed_blocks;
               pthread_mutex_unlock(&event_size_mutex);

#ifdef MM_FRONTEND
               /* remove it from database */
               if ( dbhost != NULL ) {
                  struct dbinfo dbdata;
                  dbdata.type = MM_DB_DELETE;
                  dbdata.event_id = mineventnum;
                  send_to_db_server((char *)&dbdata, sizeof(dbdata));
               }
#endif
            }

            info("storage used now %luMb\n", MB_USED);
 
            if ( pglobal->stop ) 
               goto out;

            sleep(1);   /* don't overwhelm the system */
         }
      }
      sleep(5);
   }
out:
   pthread_cleanup_pop(1);
   return NULL;
}
/*************end recycle functions**********/


/********************************************
 Intiialize frame saving
 Returns 0 on success, -1 on any error
********************************************/
int output_file_init(char * output_dir, char *name) {
   struct stat s;
   char max_evt_dir[1024];

   /* if no output directory supplied, abort */
   if ( output_dir == NULL ) {
      fprintf(stderr, "output directory not supplied\n");
      return -1;
   }
#ifdef MM_FRONTEND
      if ( cam_id == -1 ) {
         fprintf(stderr, "Cam Id needed\n");
         return -1;
      }
      sprintf(rootdir, "%s/%d", output_dir, cam_id);
#else
      sprintf(rootdir, "%s/%s", output_dir, name);
#endif

   /* make the target directory if it doesn't exist */
   umask(0);
   if ( mkdir(rootdir, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) == -1 && errno != EEXIST ) {
      fprintf(stderr, "output_file_init: mkdir %s\n%s\n", rootdir, strerror(errno));
      return -1;
   }

#ifdef MM_FRONTEND
   /* open channel to db server */
   if ( connect_to_db_server() == -1) {
      fprintf(stderr, "Fatal: Could not connect to DB server at %s:%d\n%s\n",
                   dbhost, dbport, strerror(errno));
      return -1;
   }
#endif

   /* get oldest and newest events */
   minmax(rootdir, NULL, &mineventnum, max_evt_dir, &maxeventnum);

   info("minevent %lu, maxevent %lu\n", mineventnum, maxeventnum);

    if ( storage_limit_mb >0 ) {

      if ( recycle > 100 || recycle < 0 ) /* sanity check */
         return -1;
 
      if (pthread_mutex_init(&event_size_mutex, NULL) != 0) {
         fprintf(stderr,"could not initialize mutex variable\n%s\n", strerror(errno));
         return -1;
      }

      tot_event_blocks = 0;

      if ( maxeventnum == 0 )            /* no events yet */
         strcpy(max_evt_dir, rootdir);

      if ( stat(max_evt_dir, &s) == -1) {
         fprintf(stderr, "Couldn't stat max event dir %s\n%s\n", max_evt_dir, strerror(errno));
         return -1;
      }

      max_stat = s.st_mtim;

      /* stat the existing events in a seperate thread so we don't have to wait */
      pthread_create(&rootdir_stat, 0, rootdir_stat_thread, NULL);
      pthread_detach(event_remover);

      if ( recycle ) {
         /* launch the event remover thread */
         info("launching file management thread\n");
         pthread_create(&event_remover, 0, event_remover_thread, NULL);
         pthread_detach(event_remover);
      }
   }
   return 0;
}

/*****************************************
 Close the current event
*****************************************/
int close_event(char *dirname, struct timeval starttime, int nframes, struct timeval endtime) {

   info("event close %d\n", current_event); 

   if ( storage_limit_mb > 0 ) {
      struct stat s;
      info("total event size=%luMb %s\n", MB_USED, stat_run == 1 ? "(stat pending)" : "");

      /* add directory entry blocks to running block count */
      if ( stat(dirname, &s) == -1) {
         fprintf(stderr, "close_event: couldn't stat %s\n%s\n", dirname, strerror(errno));
         return -1;
      }

      pthread_mutex_lock(&event_size_mutex);                                                  
      tot_event_blocks += s.st_blocks;
      pthread_mutex_unlock(&event_size_mutex);

      if ( stat_run == 2 ) {
      /* launch the stat thread to verify the running block count */
      /* invoked manually by sending 'stat 1' to the control interface */
         info("launching stat thread\n");
         max_stat = s.st_mtim;
         check_blocks = tot_event_blocks;
         pthread_create(&rootdir_stat, 0, rootdir_stat_thread, NULL);
         pthread_detach(event_remover);
      }
   }

#ifdef MM_FRONTEND
   /* tell the server to update the database */
   /* and rename the TMP* dir to next event number */
   if ( dbhost != NULL ) {
      ulong stime = starttime.tv_sec*1000 + starttime.tv_usec/1000;
      ulong etime = endtime.tv_sec*1000 + endtime.tv_usec/1000;
      struct dbinfo db_data;
      db_data.type = MM_DB_INSERT;
      db_data.cam_id = cam_id;
      db_data.starttime = starttime.tv_sec;
      db_data.endtime = endtime.tv_sec;
      db_data.length = etime - stime;
      db_data.nframes = nframes;
      sprintf(db_data.event_dir, "TMP%ld", db_data.starttime);

      send_to_db_server((char *)&db_data, sizeof(db_data));
   }
#endif

   current_event = 0;
   return 0;
}

/************************************************
 New event: create the directory for it 
************************************************/
int new_event(char *dirname, int num_event, struct timeval timestamp) {

#ifdef MM_FRONTEND
   /* event dir name is TMPnnnnnnn where nnn... is the unix epoch time in seconds */
   /* on close, the dir gets renamed to an event number by the db server */
   sprintf(dirname, "%s/TMP%ld", rootdir, timestamp.tv_sec);

#else
   struct tm *timeofday;
   ulong eventtime = timestamp.tv_sec;

   timeofday = localtime((const time_t *)&eventtime);
   sprintf(dirname, "%s/Event-%03d [%4d-%02d-%02d %02d:%02d:%02d]",
   rootdir, num_event,
   timeofday->tm_year+1900, timeofday->tm_mon, timeofday->tm_mday,
   timeofday->tm_hour, timeofday->tm_min, timeofday->tm_sec);
#endif

   if ( mkdir(dirname, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) == -1 ) {
      fprintf(stderr, "new_event: mkdir %s\n%s\n", dirname, strerror(errno));
      return -1;
   }

   current_event = num_event;
   info("new event %d %s\n", num_event, dirname);
   return 0;
}

/******************************************
 Put a frame to disk
 On any error, turn off file saving
******************************************/
void save_jpeg_to_file( unsigned char *jpeg_buf, ulong framesize,
               int motion, int dt_stamp, struct timeval timestamp) {

   static char dirname[512];
   static int framenum = 0;
   char filename[1024];
   int rv, fd;
   size_t len;
   static struct timeval event_time;
   ulong file_blocks;
   ulong frametime = timestamp.tv_sec*1000 + timestamp.tv_usec/1000;
   ulong jpg_size = framesize;

   if ( jpeg_buf == NULL ) {
      /* program shut down */
      if ( framenum > 0 ) {
         info("shutdown: closing event %d\n", current_event); 
         close_event(dirname, event_time, framenum-1, timestamp );
      }
      return;
   }

   if ( motion != 0) {         /* if we have motion */

      if ( framenum > 0 ) {    /* do we have an event open? */

         if  ( (frametime - (event_time.tv_sec*1000 +  event_time.tv_usec/1000)) > MAX_EVENT_TIME ) {

            /* max event time reached, so close it and open a new one */
            info("Max event time reached, start new event\n");
            if ( close_event(dirname, event_time, framenum-1, timestamp ) == -1)
               goto errout;

            event_time = timestamp;
            if ( new_event(dirname, ++maxeventnum, event_time) == -1)
               goto errout;
            framenum = 1;
         }
      }

      else {
         /* this is a new event */
         event_time = timestamp;
         if ( new_event(dirname, ++maxeventnum, event_time) == -1)
            goto errout;

         framenum = 1;
      }

      /* do the timestamp embedding now */
      if ( dt_stamp )
         jpg_size = embed_timestamp(jpeg_buf, framesize, timestamp);

      /* write the file */
      sprintf(filename, "%s/%03d-capture.jpg", dirname, framenum);
      if ( (fd = open(filename, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)) == -1) {
         fprintf(stderr, "file create fail: %s\n%s\n", filename, strerror(errno));
         close_event(dirname, event_time, framenum-1, timestamp);
         goto errout;
      }

      framenum++;

      len = jpg_size;
      while ( len > 0 ) {
         if ( (rv = write(fd, jpeg_buf, len)) == -1) {
            fprintf(stderr, "file write error: %s\n%s\n", filename, strerror(errno));
            close_event(dirname, event_time, framenum-1, timestamp);      
            goto errout;
         }
         len -= rv;
      }
      close(fd);

      if ( storage_limit_mb > 0 ) {

      /* track disk usage */
         file_blocks = jpg_size / 512;
         file_blocks += (jpg_size%512)  ? 1 : 0;
         rv = file_blocks%(fs_blksize/512);
         if ( rv ) file_blocks += (fs_blksize/512 - rv);

      /* tot is shared with other threads, so lock it */
         pthread_mutex_lock(&event_size_mutex);
         tot_event_blocks += file_blocks;
         pthread_mutex_unlock(&event_size_mutex);

         if ( storage_limit_mb>0 && !recycle && 
               ( MB_USED >= storage_limit_mb) ) {
   
         /* storage limit reached with no recycling */
            fprintf(stderr, "Storage allocation full, stop recording\n");
            close_event(dirname, event_time, framenum-1, timestamp);      
            goto errout1;
         }
      }
   }

   else {
      /* no motion; if there's an event open, close it*/
      if ( framenum > 0 ) {
         if ( close_event(dirname, event_time, framenum-1, timestamp ) == -1) 
            goto errout;

         framenum = 0;
      }
   }
   return;

errout:
   fprintf(stderr, "db or filesave error, will turn off file saving\n");
errout1:
   save_events = omparms[MODECT_ID_SAVE_EVENTS].value = 0;
   if ( status_port )
      pthread_kill(motion_status, SIGUSR1);
}

