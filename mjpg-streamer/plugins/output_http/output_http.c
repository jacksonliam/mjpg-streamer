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

#include "../../mjpg_streamer.h"
#include "../../utils.h"
#include "../output.h"

#define OUTPUT_PLUGIN_NAME "HTTP output plugin"
#define MAX_ARGUMENTS 32

typedef enum { SNAPSHOT, STREAM } answer_t;

pthread_t   server;
globals     *global;
int         sd, port;

void help(void) {
  fprintf(stderr, " ---------------------------------------------------------------\n" \
                  " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
                  " ---------------------------------------------------------------\n" \
                  " The following parameters can be passed to this plugin:\n\n" \
                  " [-p | --port ]..........: TCP port for this HTTP server\n" \
                  " ---------------------------------------------------------------\n");
}

/* thread for clients that connected to this server */
void *client_thread( void *arg ) {
  int fd = *((int *)arg);
  fd_set fds;
  unsigned char *frame=NULL;
  int ok = 1, frame_size=0;
  char buffer[1024] = {0};
  struct timeval to;
  answer_t answer = STREAM;

  if (arg != NULL) free(arg); else exit(1);
  if ((frame = (unsigned char *)calloc(1, 256*1024)) == NULL) {
    fprintf(stderr, "not enough memory\n");
    exit(1);
  }

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

  if( answer == SNAPSHOT ) {
    sprintf(buffer, "HTTP/1.0 200 OK\r\n" \
                    "Server: MJPG Streamer\r\n" \
                    "Content-type: image/jpeg\r\n"
                    "\r\n");
  } else {
    sprintf(buffer, "HTTP/1.0 200 OK\r\n" \
                    "Server: MJPG Streamer\r\n" \
                    "Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n" \
                    "\r\n" \
                    "--" BOUNDARY "\n");
  }
  ok = ( write(fd, buffer, strlen(buffer)) >= 0)?1:0;

  while ( ok >= 0 && !global->stop ) {

    /* wait for fresh frames */
    DBG("waiting for fresh frame\n");
    pthread_cond_wait(&global->db_update, &global->db);

    /* read buffer */
    frame_size = global->size;
    memcpy(frame, global->buf, frame_size);
    DBG("got frame (size: %d kB)\n", frame_size/1024);

    pthread_mutex_unlock( &global->db );
    DBG("creating answer\n");

    if ( answer == STREAM ) {
      sprintf(buffer, "Content-type: image/jpeg\n\n");
      ok = ( write(fd, buffer, strlen(buffer)) >= 0)?1:0;
      if( ok < 0 ) break;
    }

    DBG("printing picture\n");
    ok = write(fd, frame, frame_size);
    if( ok < 0 || answer == SNAPSHOT ) break;

    sprintf(buffer, "\n--" BOUNDARY "\n");
    ok = ( write(fd, buffer, strlen(buffer)) >= 0)?1:0;
    if( ok < 0 ) break;
  }

  close(fd);
  DBG("leaving HTTP thread for client\n");
  free(frame);

  return NULL;
}

void server_cleanup(void *arg) {
  static unsigned char first_run=1;

  if ( !first_run ) {
    DBG("already cleaned up ressources\n");
    return;
  }

  first_run = 0;
  DBG("cleaning up ressources allocated by server thread\n");

  close(sd);
}

void *server_thread( void *arg ) {
  struct sockaddr_in addr;
  int on;
  pthread_t client;

  /* set cleanup handler to cleanup allocated ressources */
  pthread_cleanup_push(server_cleanup, NULL);

  /* open socket for server */
  sd = socket(PF_INET, SOCK_STREAM, 0);
  if ( sd < 0 ) {
    fprintf(stderr, "socket failed\n");
    exit(1);
  }

  /* ignore "socket already in use" errors */
  on = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    perror("setsockopt(SO_REUSEADDR) failed");
    exit(1);
  }

  /* configure server address to listen to all local IPs */
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = port;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if ( bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 ) {
    fprintf(stderr, "bind(%d) failed\n", htons(port));
    perror("bind: ");
    exit(1);
  }

  /* start listening on socket */
  if ( listen(sd, 10) != 0 ) {
    fprintf(stderr, "listen failed\n");
    exit(1);
  }

  /* create a child for every client that connects */
  while ( !global->stop ) {
    int *pfd = (int *)calloc(1, sizeof(int));

    if (pfd == NULL) {
      fprintf(stderr, "failed to allocate (a very small amount of) memory\n");
      exit(1);
    }

    DBG("waiting for clients to connect\n");
    *pfd = accept(sd, 0, 0);
    DBG("create thread to handle client that just established a connection\n");

    pthread_create(&client, NULL, &client_thread, pfd);
    pthread_detach(client);
  }

  DBG("leaving server thread, calling cleanup function now\n");
  pthread_cleanup_pop(1);

  return NULL;
}

/*** plugin interface functions ***/
int output_init(output_parameter *param) {
  char *argv[MAX_ARGUMENTS]={NULL};
  int argc=1, i;

  port = htons(8080);

  /* convert the single parameter-string to an array of strings */
  argv[0] = OUTPUT_PLUGIN_NAME;
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
            OPRINT("ERROR: too many arguments to output plugin\n");
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
      {"p", required_argument, 0, 0},
      {"port", required_argument, 0, 0},
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

      /* p, port */
      case 2:
      case 3:
        DBG("case 2,3\n");
        port = htons(atoi(optarg));
        break;
    }
  }

  global = param->global;

  OPRINT("HTTP TCP port.....: %d\n", ntohs(port));
  return 0;
}

int output_stop(void) {
  DBG("will cancel server thread\n");
  pthread_cancel(server);
  return 0;
}

int output_run(void) {
  DBG("launching server thread\n");
  pthread_create(&server, 0, server_thread, NULL);
  pthread_detach(server);
  return 0;
}
