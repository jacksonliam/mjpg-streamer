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
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h> // sockaddr_in 
#include <poll.h> // for poll

#include "../../mjpg_streamer.h"
#include "../../utils.h"

#define INPUT_PLUGIN_NAME "SOCKET input plugin"


/* private functions and variables to this plugin */
static pthread_t   worker;
static globals     *pglobal;

void *worker_thread(void *);
void worker_cleanup(void *);
void help(void);

static char *unix_socket_path = NULL;
static int socket_port = -1;
static int plugin_number;
const int MAX_CLIENTS = 1000;

/* global variables for this plugin */
static int fd, rc, wd, size;

/*** plugin interface functions ***/
int input_init(input_parameter *param, int id)
{
    int i;
    plugin_number = id;
    param->argv[0] = INPUT_PLUGIN_NAME;

    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    reset_getopt();
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"h", no_argument, 0, 0
            },
            {"help", no_argument, 0, 0},
            {"p", required_argument, 0, 0},
            {"port", required_argument, 0, 0},
            {"u", required_argument, 0, 0},
            {"path", required_argument, 0, 0},
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

            /* p, port */
        case 2:
        case 3:
            DBG("case 2,3\n");
            socket_port = atoi(optarg);
            break;

            /* u, path */
        case 4:
        case 5:
            DBG("case 4,5\n");
            unix_socket_path = malloc(strlen(optarg) + 2);
            strcpy(unix_socket_path, optarg);
            break;

        default:
            DBG("default case\n");
            help();
            return 1;
        }
    }

    pglobal = param->global;

    /* check for required parameters */
    if(unix_socket_path == NULL && socket_port<0) {
        IPRINT("ERROR: no port and no unix socket path specified\n");
        return 1;
    }

    if(unix_socket_path != NULL && socket_port>0) {
        IPRINT("ERROR: port and unix socket path could not be specified same time\n");
        return 1;
    }

    if(unix_socket_path) {
        IPRINT("unix socket path..: %s\n", unix_socket_path);
    } else {
        IPRINT("tcp port..........: %d\n", socket_port);
    }
    
    param->global->in[id].name = malloc((strlen(INPUT_PLUGIN_NAME) + 1) * sizeof(char));
    sprintf(param->global->in[id].name, INPUT_PLUGIN_NAME);

    return 0;
}

int input_stop(int id)
{
    DBG("will cancel input thread\n");
    pthread_cancel(worker);
    return 0;
}

int input_run(int id)
{
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
void help(void)
{
    fprintf(stderr, " ---------------------------------------------------------------\n" \
    " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
    " ---------------------------------------------------------------\n" \
    " The following parameters can be passed to this plugin:\n\n" \
    " [-p | --port ].........: TCP socket port to listen\n" \
    " [-u | --path ].........: UNIX socket path\n" \
    " ---------------------------------------------------------------\n");
}

// Header:
//  2 - magic
//  4 - size
//  4 - CRC32 if needed (not implemented)
const int PACKET_HEADER_SIZE = 10;
const int DEFAULT_RECV_WAIT = 5;
const char MAGIC[] = "RK";

int relay_arrived_pic(int sock) {

    char *buf, bufl[PACKET_HEADER_SIZE];
    int rsize = 0, fill_size = 0;
    int ret, req, bytes_avail;
    int alloc = 1;
    struct timeval tv;
    struct timeval timestamp;
    fd_set fds;
    
    // receive header
    ret = recv(sock, bufl, PACKET_HEADER_SIZE, 0);
    if(ret < PACKET_HEADER_SIZE) {
        DBG("wrong header received\n");
        return 0;
    }

    if(!(bufl[0] == MAGIC[0] && bufl[1] == MAGIC[1])) {
        DBG("wrong header magic\n");
        return 0;
    }

    memcpy(&rsize, bufl+2, 4);

    // now we know the size!
    rsize = ntohl(rsize);

    // lock everything
    pthread_mutex_lock(&pglobal->in[plugin_number].db);

    // allocate memory
    if(pglobal->in[plugin_number].buf != NULL) {
        if(pglobal->in[plugin_number].size < rsize) {
            free(pglobal->in[plugin_number].buf);
        } else {
            alloc = 0;
        }
    }
    if(alloc) {
        pglobal->in[plugin_number].buf = malloc(rsize + (1 << 16));
        if(pglobal->in[plugin_number].buf == NULL) {
            fprintf(stderr, "could not allocate memory\n");
            pthread_mutex_lock(&pglobal->in[plugin_number].db);
            return -1;
        }
    }
    // read form socket
    while(fill_size < rsize) {
        
        tv.tv_sec = DEFAULT_RECV_WAIT;
        tv.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        
        ret = select(sock+1, &fds, NULL, NULL, &tv);
        if ( ret <= 0 ) {
            pthread_mutex_unlock(&pglobal->in[plugin_number].db);
            return 0;
        }
        
        ret = recv(sock, pglobal->in[plugin_number].buf+fill_size, rsize-fill_size, 0);
        if(ret <= 0 ) {
            pthread_mutex_unlock(&pglobal->in[plugin_number].db);
            return 0;
        }
        fill_size += ret;
    }

    pglobal->in[plugin_number].size = fill_size;
    
    gettimeofday(&timestamp, NULL);
    pglobal->in[plugin_number].timestamp = timestamp;

    DBG("new frame copied (size: %d)\n", pglobal->in[plugin_number].size);

    /* signal fresh_frame */
    pthread_cond_broadcast(&pglobal->in[plugin_number].db_update);
    pthread_mutex_unlock(&pglobal->in[plugin_number].db);

    return 1;
}

/* get current time */
double cur_time() {
    struct timeval tm;
    gettimeofday(&tm, 0);
    return tm.tv_sec + tm.tv_usec/1000000.0;
}

/* the single writer thread */
void *worker_thread(void *arg)
{
    char buffer[1<<16];
    struct stat stats;
    

    /* socket related */
    int sock, res, sock_peer; 
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_size = sizeof(peer_addr);
    struct pollfd fds[MAX_CLIENTS];
    int fds_cnt = 0, i;
    struct sockaddr_in addr;
    struct sockaddr_un un_addr;
    struct linger linger_opt = { 1, 0 };
    int servlen;
    int ret;
    struct stat sb;

    /* logging fps */
    unsigned int frame_count = 0;
    double elapsed = wall_time();

    /* set cleanup handler to cleanup allocated resources */
    pthread_cleanup_push(worker_cleanup, NULL);


    // this is a TCP socket
    if(socket_port > 0) {

        sock = socket( AF_INET, SOCK_STREAM, 0 );
        if( sock < 0 ) {
            perror("socket() failed");
            return NULL;
        }

        // file the structre
        memset(&addr, 0, sizeof(addr));
        addr.sin_addr.s_addr = htonl( INADDR_ANY );
        addr.sin_port = htons( socket_port );
        addr.sin_family = AF_INET;

        // bind the listening port to our socket
        if( bind (sock, (const struct sockaddr *)&addr, sizeof(addr) ) != 0 ) {
            perror("bind() failed");
            close(sock);
            return NULL;
        }

        DBG("listenning on TCP port: %d\n", socket_port);

    // this is a UNIX socket
    } else {

        // remove socket if already exists
        if (stat(unix_socket_path, &sb) == 0) {
            unlink(unix_socket_path);
        }

        sock = socket( AF_UNIX, SOCK_STREAM, 0 );
        if( sock < 0 ) {
            perror("socket() failed");
            return NULL;
        }
        
        un_addr.sun_family = AF_UNIX;
        strcpy(un_addr.sun_path, unix_socket_path);
        servlen=strlen(un_addr.sun_path) + sizeof(un_addr.sun_family);
        // bind the listening port to our socket
        if( bind (sock, (const struct sockaddr *)&un_addr, servlen ) != 0 ) {
            perror("bind() failed");
            close(sock);
            return NULL;
        }

        DBG("listenning on UNIX socket: %d\n", unix_socket_path);

        chmod ( unix_socket_path,0777);
    }

    // allow this port to be used right after programs terminate
    if( setsockopt(sock, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt)) != 0) {
        perror("setsockopt() failed");
        close(sock);
        return NULL;
    }

    // set the listen queue
    if( listen( sock, 10 ) != 0) {
        perror("listen() failed");
        close(sock);
        return NULL;
    }

    fds[0].fd = sock;
    fds[0].events = POLLIN;
    fds_cnt++;

    while(!pglobal->stop) {

        // any event ?!
        res = poll(fds, fds_cnt, -1);
        if ( res < 0 ) {
            for(i=0;i<fds_cnt;i++) if(fds[i].fd>0) close(fds[i].fd);
            perror("poll failed");
            break;
        }
        if(res == 0) {
            continue;
        }

        // new client
        if(fds[0].revents & POLLIN) {
            sock_peer = accept(sock, (struct sockaddr*) &peer_addr, &peer_addr_size);
            if (sock_peer < 0) {
                for(i=0;i<fds_cnt;i++) if(fds[i].fd>0) close(fds[i].fd);
                perror("can't accept new connection");
                break;
            }
            for(i=1;i<fds_cnt;i++) {
                if(fds[i].fd<0) break;
            }
            if(i == MAX_CLIENTS) {
                DBG("max client reached\n");
                close(sock_peer);        
            } else {
                fds[i].fd = sock_peer;
                fds[i].events = POLLIN;
                if(i==fds_cnt) fds_cnt++;
            }
            res--;
        }

        // process client 
        for(i=1;(i<fds_cnt) && (res>0);i++) {
            if(fds[i].revents & POLLIN) {
                
                // data arrived - we expect a JPEG, read it                
                ret = relay_arrived_pic(fds[i].fd);

                if(ret<0) {
                    perror("system failure in client processing");
                    goto thread_quit;
                }

                // close socket arrived
                if(ret==0) {
                    DBG("client finished\n");
                    close(fds[i].fd);
                    fds[i].fd = -1;
                }
                res--;

                /* report FPS */
                report_fps(&frame_count, &elapsed, "in_sock", 0,
                       pglobal->logtype, pglobal->logging_sockfile, pglobal->logging_section);
                
            }
        }

    }

thread_quit:
    close(sock);

    DBG("leaving input thread, calling cleanup function now\n");
    /* call cleanup handler, signal with the parameter */
    pthread_cleanup_pop(1);

    return NULL;
}

void worker_cleanup(void *arg)
{
    static unsigned char first_run = 1;

    if(!first_run) {
        DBG("already cleaned up resources\n");
        return;
    }

    first_run = 0;
    DBG("cleaning up resources allocated by input thread\n");

    if(pglobal->in[plugin_number].buf != NULL) free(pglobal->in[plugin_number].buf);

}





