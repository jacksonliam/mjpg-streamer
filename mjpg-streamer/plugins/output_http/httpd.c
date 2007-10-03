#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define DEBUG

#include "httpd.h"
#include "../../mjpg_streamer.h"
#include "../../utils.h"
#include "../output.h"

extern globals *global;
int  sd, port;
char *credentials;

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
void init_iobuffer(iobuffer *iobuf) {
  memset(iobuf->buffer, 0, sizeof(iobuf->buffer));
  iobuf->level = 0;
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
void init_request(request *req) {
  req->type        = A_UNKNOWN;
  req->parameter   = NULL;
  req->client      = NULL;
  req->credentials = NULL;
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
void free_request(request *req) {
  if ( req->parameter != NULL ) free(req->parameter);
  if ( req->client != NULL ) free(req->client);
  if ( req->credentials != NULL ) free(req->credentials);
}

/* setsockopt(n, SOL_SOCKET, SO_KEEPALIVE, &const_int_1, sizeof(const_int_1)); */
/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
/* read with timeout, implemented without using signals */
int _read(int fd, iobuffer *iobuf, void *buffer, size_t len, int timeout) {
  int copied=0, rc, i;
  fd_set fds;
  struct timeval tv;

  memset(buffer, 0, len);

  while ( (copied < len) ) {
    i = MIN(iobuf->level, len-copied);
    memcpy(buffer+copied, iobuf->buffer+IO_BUFFER-iobuf->level, i);

    iobuf->level -= i;
    copied += i;
    if ( copied >= len )
      return copied;

    /* select will return in case of timeout or new data arrived */
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    if ( (rc = select(fd+1, &fds, NULL, NULL, &tv)) <= 0 ) {
      if ( rc < 0)
        exit(EXIT_FAILURE);

      /* this must be a timeout */
      return copied;
    }

    init_iobuffer(iobuf);

    /* there should be at least one byte, because select signalled it */
    if ( (iobuf->level = read(fd, &iobuf->buffer, IO_BUFFER)) < 0 ) {
      /* an error occured */
      return -1;
    }

    /* align data to the end of the buffer if less than IO_BUFFER bytes were read */
    memmove(iobuf->buffer+(IO_BUFFER-iobuf->level), iobuf->buffer, iobuf->level);
  }

  return 0;
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
/* read just a single line or timeout */
int _readline(int fd, iobuffer *iobuf, void *buffer, size_t len, int timeout) {
  char c='\0', *out=buffer;
  int i;

  memset(buffer, 0, len);

  for ( i=0; i<len && c != '\n'; i++ ) {
    if ( _read(fd, iobuf, &c, 1, timeout) <= 0 ) {
      /* timeout or error occured */
      return -1;
    }
    *out++ = c;
  }

  return i;
}

/******************************************************************************
Description.: (taken from busybox)
Input Value.: 
Return Value: 
******************************************************************************/
void decodeBase64(char *data) {
  const unsigned char *in = (const unsigned char *)data;
  /* The decoded size will be at most 3/4 the size of the encoded */
  unsigned ch = 0;
  int i = 0;

  while (*in) {
    int t = *in++;

    if (t >= '0' && t <= '9')
      t = t - '0' + 52;
    else if (t >= 'A' && t <= 'Z')
      t = t - 'A';
    else if (t >= 'a' && t <= 'z')
      t = t - 'a' + 26;
    else if (t == '+')
      t = 62;
    else if (t == '/')
      t = 63;
    else if (t == '=')
      t = 0;
    else
      continue;

    ch = (ch << 6) | t;
    i++;
    if (i == 4) {
      *data++ = (char) (ch >> 16);
      *data++ = (char) (ch >> 8);
      *data++ = (char) ch;
      i = 0;
    }
  }
  *data = '\0';
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
void send_snapshot(int fd) {
  unsigned char *frame=NULL;
  int frame_size=0;
  char buffer[256] = {0};

  if ( (frame = (unsigned char *)malloc(MAX_FRAME_SIZE)) == NULL ) {
    fprintf(stderr, "not enough memory\n");
    exit(EXIT_FAILURE);
  }

  sprintf(buffer, "HTTP/1.0 200 OK\r\n" \
                  "Server: MJPG Streamer\r\n" \
                  "Content-type: image/jpeg\r\n" \
                  "\r\n");

  if ( write(fd, buffer, strlen(buffer)) < 0 ) {
    free(frame);
    return; 
  }

  /* wait for a fresh frame */
  pthread_cond_wait(&global->db_update, &global->db);

  /* read buffer */
  frame_size = global->size;
  memcpy(frame, global->buf, frame_size);
  DBG("got frame (size: %d kB)\n", frame_size/1024);

  pthread_mutex_unlock( &global->db );

  write(fd, frame, frame_size);
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
void send_stream(int fd) {
  unsigned char *frame=NULL;
  int frame_size=0;
  char buffer[256] = {0};

  if ( (frame = (unsigned char *)malloc(MAX_FRAME_SIZE)) == NULL ) {
    fprintf(stderr, "not enough memory\n");
    exit(EXIT_FAILURE);
  }

  sprintf(buffer, "HTTP/1.0 200 OK\r\n" \
                  "Server: MJPG Streamer\r\n" \
                  "Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n" \
                  "\r\n" \
                  "--" BOUNDARY "\n");

  if ( write(fd, buffer, strlen(buffer)) < 0 ) {
    free(frame);
    return;
  }

  while ( !global->stop ) {

    /* wait for fresh frames */
    pthread_cond_wait(&global->db_update, &global->db);

    /* read buffer */
    frame_size = global->size;
    memcpy(frame, global->buf, frame_size);
    DBG("got frame (size: %d kB)\n", frame_size/1024);

    pthread_mutex_unlock( &global->db );

    sprintf(buffer, "Content-type: image/jpeg\n\n");
    if ( write(fd, buffer, strlen(buffer)) < 0 ) break;

    if( write(fd, frame, frame_size) < 0 ) break;

    sprintf(buffer, "\n--" BOUNDARY "\n");
    if ( write(fd, buffer, strlen(buffer)) < 0 ) break;
  }

  free(frame);
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
void send_error(int fd, int which) {
  char buffer[256] = {0};

  if ( which == 401 ) {
    sprintf(buffer, "HTTP/1.0 401 Unauthorized\r\n" \
                    "Content-type: text/plain\r\n" \
                    "Connection: close\r\n" \
                    "WWW-Authenticate: Basic realm=\"MJPG-Streamer\"\r\n" \
                    "\r\n" \
                    "401: Not Authenticated!");
  } else {
    sprintf(buffer, "HTTP/1.0 501 Not Implemented\r\n" \
                    "Content-type: text/plain\r\n" \
                    "Connection: close\r\n" \
                    "\r\n" \
                    "501: Not Implemented!");
  }

  write(fd, buffer, strlen(buffer));
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
/* thread for clients that connected to this server */
void *client_thread( void *arg ) {
  int fd = *((int *)arg), cnt;
  char buffer[256], *pb=buffer;
  iobuffer iobuf;
  request req;

  if (arg != NULL) free(arg); else exit(EXIT_FAILURE);

  init_iobuffer(&iobuf);
  init_request(&req);

  /* What does the client want to receive? */
  memset(buffer, 0, sizeof(buffer));
  cnt = _readline(fd, &iobuf, buffer, sizeof(buffer)-1, 5);

  /* determine what to deliver */
  if ( strstr(buffer, "GET /?action=snapshot") != NULL ) {
    req.type = A_SNAPSHOT;
  }
  else if ( strstr(buffer, "GET /?action=stream") != NULL ) {
    req.type = A_STREAM;
  }
  else if ( strstr(buffer, "GET /?action=command") != NULL ) {
    int len;

    DBG("Request for command\n");
    req.type = A_COMMAND;

    /* search for second variable "command" that specifies the command */
    if ( (pb = strstr(buffer, "command=")) == NULL ) {
      DBG("no command specified, it should be passed as second variable\n");
      send_error(fd, 501);
      close(fd);
      return NULL;
    }

    /* req.parameter = strdup(strsep(&pb, " ")+strlen("command=")); */
    /* i like more to validate against the character set using strspn() */
    pb += strlen("command=");
    len = strspn(pb, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_");
    req.parameter = malloc(len+1);
    memset(req.parameter, 0, len+1);
    strncpy(req.parameter, pb, len);

    DBG("parameter (len: %d): \"%s\"\n", len, req.parameter);
  }
  else {
    int len;

    DBG("try to serve a file\n");
    req.type = A_FILE;

    if ( (pb = strstr(buffer, "GET /")) == NULL ) {
      DBG("HTTP request seems to be malformed\n");
      send_error(fd, 501);
      close(fd);
      return NULL;
    }

    pb += strlen("GET /");
    len = strspn(pb, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._-");
    req.parameter = malloc(len+1);
    memset(req.parameter, 0, len+1);
    strncpy(req.parameter, pb, len);

    DBG("parameter (len: %d): \"%s\"\n", len, req.parameter);
  }

  /* parse the rest of the request */
  do {
    memset(buffer, 0, sizeof(buffer));
    cnt = _readline(fd, &iobuf, buffer, sizeof(buffer)-1, 5);

    if ( strstr(buffer, "User-Agent: ") != NULL ) {
      req.client = strdup(buffer+strlen("User-Agent: "));
    }
    else if ( strstr(buffer, "Authorization: Basic ") != NULL ) {
      req.credentials = strdup(buffer+strlen("Authorization: Basic "));
      decodeBase64(req.credentials);
      DBG("username:password: %s\n", req.credentials);
    }

  } while( cnt > 2 && !(buffer[0] == '\r' && buffer[1] == '\n') );

  /* check for username and password if parameter -c was given */
  if ( credentials != NULL ) {
    if ( req.credentials == NULL || strcmp(credentials, req.credentials) != 0 ) {
      DBG("access denied\n");
      send_error(fd, 401);
      close(fd);
      if ( req.parameter != NULL ) free(req.parameter);
      if ( req.client != NULL ) free(req.client);
      if ( req.credentials != NULL ) free(req.credentials);
      return NULL;
    }
    DBG("access granted\n");
  }

  /* now it's time to answer */
  switch ( req.type ) {
    case A_SNAPSHOT:
      DBG("Request for snapshot\n");
      send_snapshot(fd);
      break;
    case A_STREAM:
      DBG("Request for stream\n");
      send_stream(fd);
      break;
    case A_COMMAND:
      //command(fd, parameter);
      send_error(fd, 501);
      break;
    case A_FILE:
      //deliver_file(fd, parameter)
      send_error(fd, 501);
      break;
    default:
      DBG("unknown request\n");
  }

  close(fd);
  free_request(&req);

  DBG("leaving HTTP client thread\n");
  return NULL;
}

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
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

/******************************************************************************
Description.: 
Input Value.: 
Return Value: 
******************************************************************************/
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
        int *pfd = (int *)malloc(sizeof(int));

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

#undef DEBUG















