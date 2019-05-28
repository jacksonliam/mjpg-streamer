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
#define MAXMSG  512
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>

#include "../../mjpg_streamer.h"
#include "output_modect.h"

void info(char *, ...);

int spsock;
fd_set active_fd_set, read_fd_set;
globals *pglobal;
extern int motion;
extern int save_events;
char *motion_text[3] = { "Idle", "Alarm", "Alert" };
char *status_text[2] = { "Error", "ok" };
extern int status_port;
extern char *name;

int make_socket (int port) {
   int sock, true=1;
   struct sockaddr_in name;

/* Create the socket. */
   sock = socket (PF_INET, SOCK_STREAM, 0);
   if (sock < 0){
      fprintf(stderr, "Motion_status: socket %s\n", strerror(errno));
      exit (EXIT_FAILURE);
   }
   setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int));

/* Give the socket a name. */
   name.sin_family = AF_INET;
   name.sin_port = htons (port);
   name.sin_addr.s_addr = htonl (INADDR_ANY);
   if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0) {
      fprintf(stderr, "Motion_status: bind %s\n", strerror(errno));
      exit (EXIT_FAILURE);
   }
   return sock;
}

/***************************************
 Read whatever client sends & discard 
***************************************/
int read_from_client (int filedes) {
    int nbytes=1;
   char buffer[512];
   while (nbytes>0) {
      nbytes = read (filedes, buffer, sizeof(buffer));
      if (nbytes < 0) {
    /* Read error. */
         fprintf (stderr, "Motion_status: read %s\n", strerror(errno));
         return -1;
      }
   }
   return 0;
}

/************************************
 Send the motion status to a client 
************************************/
int write_to_client(int filedes, int closing) {
   int n, len, sent=0;
   char buf[100];

   if ( closing ) 
      sprintf(buf, "%s %s %s", name, "Offline", "Bye");
   else
      sprintf(buf, "%s %s %s", name, motion_text[motion], status_text[save_events]);
   len = strlen(buf);

   while ( sent<len ){
      n = write(filedes, &buf[sent], len-sent);
      if ( n<0 ) {
         return n;
      }
      sent += n;
   }
   return sent;
}

/***********************************************************************
 Signal handler SIGUSR1 - sent by plugin main thread on motion change
***********************************************************************/
void send_msg() {
   int j;
   for ( j=0; j<FD_SETSIZE; j++) {
      if ( j!=spsock && FD_ISSET (j, &active_fd_set)) {
         if ( (write_to_client(j, 0) ) <0) {
            close(j);
            FD_CLR (j, &active_fd_set);
         }
      }
   }
}

/****************************************
 Cleanup on shutdown
****************************************/
void motion_status_thread_cleanup(void *arg) {
   int j, rc;
   fprintf(stderr, "motion_status_thread_cleanup\n");
/* close all connections */
   for ( j=0; j<FD_SETSIZE; j++) {
      if ( j!=spsock && FD_ISSET (j, &active_fd_set)) {
            write_to_client(j, 1); /* say goodbye */
            rc = close(j);
            if ( rc != 0 ) 
               fprintf (stderr, "connection close fd=%d %s\n", j, strerror(errno));
            FD_CLR (j, &active_fd_set);
      }
   }
/* and unbind the socket */
   rc = close(spsock);
   if ( rc != 0 ) 
      fprintf (stderr, "socket close sock=%d %s\n", spsock, strerror(errno));
}

/************************************************************** 
 Thread to output motion status on status_port
 Sends status upon receipt of SIGUSR1 from main plugin thread
***************************************************************/
void *motion_status_thread() {
   int i;
   struct sockaddr_in clientname;
   socklen_t size;
   struct sigaction sigact;

   pthread_cleanup_push(motion_status_thread_cleanup, NULL);

   sigact.sa_handler=&send_msg;

  /* Create the socket and set it up to accept connections. */
   spsock = make_socket (status_port);
   if (listen (spsock, 1) < 0) {
      fprintf (stderr, "Motion_status: listen %s\n", strerror(errno));
      exit (EXIT_FAILURE);
   }

  /* Initialize the set of active sockets. */
   FD_ZERO (&active_fd_set);
   FD_SET (spsock, &active_fd_set);

   sigaction (SIGUSR1, &sigact, NULL);

   while ( !pglobal->stop ) {
 /* Block until input arrives on one or more active sockets. */
      read_fd_set = active_fd_set;

      i = select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL);
      if ( i < 0 ) {
         if ( errno == EINTR ) continue;
         fprintf (stderr, "Motion_status: select %s\n", strerror(errno));
         exit (EXIT_FAILURE);
      }

      /* Service all the sockets with input pending. */
      for (i = 0; i < FD_SETSIZE; ++i) { 
         if (FD_ISSET (i, &read_fd_set)) {
            if (i == spsock) {
             /* Connection request  */
               int new, rc;
               size = sizeof (clientname);
               new = accept (spsock,(struct sockaddr *) &clientname, &size);
               if (new < 0) {
                  fprintf (stderr, "Motion_status: accept %s\n", strerror(errno));
               }

               FD_SET (new, &active_fd_set);
               if ( (rc = write_to_client(new, 0))<0 ) {
                  close(new);
                  FD_CLR (new, &active_fd_set);
               }
            }
            else {        /* Client has sent something...so just close */
               close (i);
               FD_CLR (i, &active_fd_set);
            }
         }
      }
   }
   pthread_cleanup_pop(1);
   return NULL;
}

