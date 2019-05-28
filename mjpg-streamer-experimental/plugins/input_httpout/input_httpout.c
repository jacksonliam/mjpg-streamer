/*******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#      input_httpout Copyright (C) 2019 Jim Allingham                          #
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>
#include <signal.h>
#include <sys/types.h>

#include "../../mjpg_streamer.h"
#include "../../utils.h"

#define INPUT_PLUGIN_NAME "input_httpout"

/* private functions and variables to this plugin */
static pthread_t   worker;
static globals     *pglobal;

void *worker_thread(void *);
void worker_cleanup(void *);
void help(void);

unsigned char *check_malloc(unsigned char *buf, int *current_size, ulong new_size);
int connect_to_http_out(int sock, char *host, int port);
int send_to_http_out(int fd, char *buffer, int bufsize);
int read_tmo(int fd, void *buf, size_t len);
int read_line(int fd, void *buf, size_t len);
int read_boundary(int fd);
int read_header(int fd);
int read_frame_header(int fd, ulong *fsize, struct timeval *tv);
int read_frame (int fd, unsigned char *buf, int bufsize);

static int myid;
char * host = NULL;
int port = -1;

void *worker_thread(void *arg) {
   int sock = -1;
   int fbufsize = 0;
   ulong framesize;
   struct timeval timestamp;

   /* set cleanup handler to cleanup allocated ressources */
   pthread_cleanup_push(worker_cleanup, NULL);

   while( !pglobal->stop ) {

      if ( (sock = connect_to_http_out(sock, host, port)) == -1 ) {
         sleep(1);                        /* never give up trying if host goes away */
         continue;
      }

      if ( read_header(sock) == -1 )      /* the main header...once per connection */
         continue;

      while (!pglobal->stop) {            /* loop for each frame */

         if ( read_frame_header(sock, &framesize, &timestamp) == -1) 
            break;

         pthread_mutex_lock(&pglobal->in[myid].db);
         pglobal->in[myid].buf = check_malloc(pglobal->in[myid].buf, &fbufsize, framesize);

         if ( read_frame(sock, pglobal->in[myid].buf, framesize) != framesize )
            break;                        /* keep mutex locked through re-connect */

         pglobal->in[myid].timestamp = timestamp;
         pglobal->in[myid].size = framesize;

         pthread_cond_broadcast(&pglobal->in[myid].db_update);      /* signal fresh frame */
         pthread_mutex_unlock(&pglobal->in[myid].db);

         if ( read_boundary(sock) == -1 ) 
            break;
      }            
   }

   /* call cleanup handler, signal with the parameter */
   pthread_cleanup_pop(1);
   return NULL;
}

/*** plugin interface functions ***/
int input_init(input_parameter *param, int id) {
   myid = id;

   param->argv[0] = INPUT_PLUGIN_NAME;

   reset_getopt();
   while(1) {
      int option_index = 0, c = 0;
      static struct option long_options[] = {
         {"help", no_argument, 0, 0},
         {"h", no_argument, 0, 0},
         {"H", required_argument, 0, 0},
         {"host", required_argument, 0, 0},
         {"p", required_argument, 0, 0},
         {"port", required_argument, 0, 0},
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
         help();
         return 1;
         break;

      /* h, host */
      case 2:
      case 3:
         host = strdup(optarg);
         break;

      /* p, port */
      case 4:
      case 5:
         port = atoi(optarg);
         break;

      default:
         help();
         return 1;
      }
   }

   /* check for required parameters */
   if ( host == NULL) 
      host = strdup("localhost");
   if ( port == -1 ) {
      IPRINT("No port supplied\n");
      return 1;
   }

   IPRINT("output_http host..........: %s\n", host);
   IPRINT("output_http port..........: %d\n", port);

   pglobal = param->global;

   return 0;
}

int input_stop(int id) {
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
   " [-H | --host ].....................: output_http host (default: localhost)\n" \
   " [-p | --port ].....................: output_http port\n");
}

void worker_cleanup(void *arg) {
   if(pglobal->in[myid].buf != NULL) free(pglobal->in[myid].buf);
   kill(getpid(), SIGINT); /* quit entire process */
}

/* read until len bytes received or timed out 
   return  number of bytes read or -1 on timeout or error */
#define TIMEOUT 3
int read_tmo(int fd, void *buf, size_t len) {
 
   fd_set fds;
   struct timeval timeout;
   int rv, n, bytes_read = 0;


   while ( bytes_read < len ) {
      FD_ZERO(&fds);
      FD_SET(fd, &fds);
      timeout.tv_sec = TIMEOUT;
      timeout.tv_usec = 0;

      if ( (rv = select(fd + 1, &fds, NULL, NULL, &timeout)) <= 0 )
        return -1;        /* 0 = timeout  -1 = error */

      if ( ( n = read(fd, buf + bytes_read, len-bytes_read) ) <= 0 )
         return -1;

      bytes_read += n;
   }
   return bytes_read;
}

/* read byte by byte until we see a newline 
   return line length or -1 on error */
int read_line(int fd, void *buf, size_t len) {
    char c = '\0', *out = buf;
    int i;

    memset(buf, 0, len);

    for(i = 0; i < len && c != '\n'; i++) {
        if( read_tmo(fd, &c, 1) <= 0)
            return -1;          /* timeout or error occured */

        *out++ = c;
    }
    return i;
}

/* read len bytes 
   return number of bytes read, or -1 on error */
int read_frame (int fd, unsigned char *buf, int len) {
    int n, nbytes = 0;

   while ( nbytes < len ) {
      n = read_tmo(fd, &buf[nbytes], len-nbytes);
      if ( n < 0 ) {
         fprintf(stderr, "client read error: %s\n", strerror(errno));
         return -1;
      }
       else if ( n == 0 ) {   /* EOF */ 
         return -1;
      }
      nbytes += n;
   }
   return nbytes;
}

/* read until we get an empty line ("\r\n")
   grab framesize and timestamp  
   return 0 on success, -1 on error */
int read_frame_header(int fd, ulong *fsize, struct timeval *tv) {
   char buf[1024];
   int n, flag = 0;

   do {
      if ( (n = read_line(fd, buf, 1024)) == -1)
         return -1;

      if ( strstr(buf, "Content-Length: ") != NULL ) {
         sscanf(buf+strlen("Content-Length: "), "%ld", fsize);
         flag++;
      }

      if ( strstr(buf, "X-Timestamp: ") != NULL ) {
         sscanf(buf+strlen("X-Timestamp: "), "%ld.%ld", &tv->tv_sec, &tv->tv_usec);
fprintf(stderr, "%s sec %ld usec %ld\n", buf, tv->tv_sec, tv->tv_usec);
        flag++;
      }
   }  while (!(n==2 && !memcmp(buf, "\r\n", 2) ));

   return flag == 2 ? 0 : -1;
}

/* read until we get an empty line ("\r\n") 
   return 0 on success, -1 on error */
int read_header(int fd) {
   char buf[1024];
   int n;
   char *req = "GET /?action=stream\n\n";

   send_to_http_out(fd, req, strlen(req));

   do {
      n = read_line(fd, buf, 1024);
      if ( n < 0 )
         return -1;

   } while  (!(n==2 && !memcmp(buf, "\r\n", 2) )); 
   return 0;
}

/* read 2-line boundary 
   return 0 on success, -1 on error */
int read_boundary(int fd) {
   char buf[100];
   int  i, n;
   for ( i=0; i<2; i++ ) {
      if ( ( n = read_line(fd, buf, 100)) == -1 )
         return -1;
   }
   return 0;
}

/* connect to the server 
   return open socket on success, -1 on error */
int connect_to_http_out(int sock, char *host, int port) {
   struct sockaddr_in sock_addr;
   struct hostent *server;
   int one=1;

   if ( sock >= 0 ) close(sock);

   if ( (sock = socket (PF_INET, SOCK_STREAM, 0)) == -1) {
      fprintf(stderr, "create socket failed: %s\n", strerror(errno));
      return -1;
   }

   if ( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1) {
      fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
      return -1;
   }

   if ( (server = gethostbyname(host)) == NULL) {
      fprintf(stderr, "gethost failed: %s\n", strerror(errno));
      return -1;
   }

   sock_addr.sin_family = AF_INET;
   memcpy(&sock_addr.sin_addr.s_addr, server->h_addr, server->h_length);
   sock_addr.sin_port = htons(port);

   if ( connect(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) == -1 )
		return -1;
	return sock; 
}

/* write len bytes to the host
   return 0 on success, -1 on error */
int send_to_http_out(int fd, char *buffer, int len) {
   int tot_sent = 0, nsent;

    while (tot_sent < len) {
        if ( (nsent = write(fd, &buffer[tot_sent],  len-tot_sent)) == -1) {
         fprintf(stderr, "db server socket write failed: %s\n", strerror(errno));
         return -1;
        }
        tot_sent += nsent;
    }
   return 0;
}

/* allocate/reallocate mem for a buffer
   reallocate only if more mem needed
   on success returns buf address 
   on error does not return (kills mjpg-streamer) */
unsigned char *check_malloc(unsigned char *buf, int *current_size, ulong new_size) {
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

