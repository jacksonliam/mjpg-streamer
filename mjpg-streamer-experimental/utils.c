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
#include <linux/types.h>
#include <string.h>
#include <fcntl.h>
#include <wait.h>
#include <time.h>
#include <limits.h>
#include <linux/stat.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <ctype.h>

#include "utils.h"

/******************************************************************************
Description.:
Input Value.:
Return Value:
******************************************************************************/
void daemon_mode(void)
{
    int fr = 0;

    fr = fork();
    if(fr < 0) {
        fprintf(stderr, "fork() failed\n");
        exit(1);
    }
    if(fr > 0) {
        exit(0);
    }

    if(setsid() < 0) {
        fprintf(stderr, "setsid() failed\n");
        exit(1);
    }

    fr = fork();
    if(fr < 0) {
        fprintf(stderr, "fork() failed\n");
        exit(1);
    }
    if(fr > 0) {
        fprintf(stderr, "forked to background (%d)\n", fr);
        exit(0);
    }

    umask(0);

    fr = chdir("/");
    if(fr != 0) {
        fprintf(stderr, "chdir(/) failed\n");
        exit(0);
    }

    close(0);
    close(1);
    close(2);

    open("/dev/null", O_RDWR);

    fr = dup(0);
    fr = dup(0);
}


/*
 * Common webcam resolutions with information from
 * http://en.wikipedia.org/wiki/Graphics_display_resolution
 */
static const struct {
    const char *string;
    const int width, height;
} resolutions[] = {
    { "QQVGA", 160,  120  },
    { "QCIF",  176,  144  },
    { "CGA",   320,  200  },
    { "QVGA",  320,  240  },
    { "CIF",   352,  288  },
    { "PAL",   720,  576  },
    { "VGA",   640,  480  },
    { "SVGA",  800,  600  },
    { "XGA",   1024, 768  },
    { "HD",    1280, 720  },
    { "SXGA",  1280, 1024 },
    { "UXGA",  1600, 1200 },
    { "FHD",   1920, 1280 },
};

/******************************************************************************
Description.: convienence function for input plugins
Input Value.:
Return Value:
******************************************************************************/
void parse_resolution_opt(const char * optarg, int * width, int * height) {
    int i;

    /* try to find the resolution in lookup table "resolutions" */
    for(i = 0; i < LENGTH_OF(resolutions); i++) {
        if(strcmp(resolutions[i].string, optarg) == 0) {
            *width  = resolutions[i].width;
            *height = resolutions[i].height;
            return;
        }
    }
    
    /* parse value as decimal value */
    if (sscanf(optarg, "%dx%d", width, height) != 2) {
        fprintf(stderr, "Invalid height/width '%s' specified!\n", optarg);
        exit(EXIT_FAILURE);
    }
}

void resolutions_help(const char * padding) {
    int i;
    for(i = 0; i < LENGTH_OF(resolutions); i++) {
        fprintf(stderr, "%s ", resolutions[i].string);
        if((i + 1) % 6 == 0)
            fprintf(stderr, "\n%s", padding);
    }
    fprintf(stderr, "\n%sor a custom value like the following" \
    "\n%sexample: 640x480\n", padding, padding);
}

/* get current time */
double wall_time() {
    struct timeval tm;
    gettimeofday(&tm, 0);
    return tm.tv_sec + tm.tv_usec/1000000.0;
}

/* open unix socket */
int journal_socket = 0;

int journal_log(const char * logging_sockfile, const char * data) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    int ret;
    int try = 0, size;
    char header[6];
    char * h;

    size = strlen(data);

    pthread_mutex_lock(&lock);
    while(try<2)  {
        if(!journal_socket) {
            journal_socket = open_journal_socket(logging_sockfile);
        }
        if(!journal_socket) {
            break;
        }
        h = header;
        *h++ = 's';
        *h++ = 0x00;
        memcpy(h, &size, 4);

        ret = send(journal_socket, header, 6, 0);
        if(ret <= 0) {
            close(journal_socket);
            journal_socket = 0;
            try++;
            continue;
        }
        
        ret = send(journal_socket, data, size, 0);
        if(ret <= 0) {
            close(journal_socket);
            journal_socket = 0;
            try++;
            continue;
        }
        break;
    }
    pthread_mutex_unlock(&lock);
}

int open_journal_socket(const char * logging_sockfile) {
    int sock;
    struct sockaddr_un server;
    char buf[1024];

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return 0;
    }
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, logging_sockfile);

    if (connect(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un)) < 0) {
        close(sock);
        return 0;
    }
    return sock;
}


/* report FPS */
void report_fps(int * frame_count,              /* FRAMES PASSED */
                double * elapsed,               /* LAST TIMESTAMP */
                const char * key_name,          /* Which section we are logging (uvc, http, sock, ...) */ 
                int key_id,                     /* Extra info, life http socket number */
                int logging_type,               /* How we log: no, print, journal  */
                const char * logging_sockfile,  /* If journal, socketfile location */
                const char * logging_section    /* If journal, name of the section  */
                ) {

    
    const int print_check_every  = 30;
    const int report_check_every = 100;
    double diff, new_ts;
    char keys[1024], keys2[1024];
    int i;

    if(logging_type == LOGGING_NO) {
        return ;
    }    
    
    (*frame_count) ++;

    if(logging_type == LOGGING_PRINT) {
        if (*frame_count % print_check_every == 0) {
            new_ts = wall_time();
            diff = new_ts - *elapsed;
            const char *s = key_name;
            i = 0;
            while (*s) {
                keys[i] = toupper((unsigned char) *s);
                i++;
                s++;
            }
            keys2[0] = 0;
            if(key_id>0) {
                snprintf(keys2, sizeof(keys2), "[SOCK=%d]", key_id);
            }
            printf("[%10.3lf][%s]%s Frames per sec: %lf\n", new_ts, keys, keys2, *frame_count/diff);
            *elapsed = new_ts;
            *frame_count = 0;
        }
        return ;
    }
    if(logging_type == LOGGING_JOURNAL) {
        if (*frame_count % report_check_every == 0) {
            new_ts = wall_time();
            diff = new_ts - *elapsed;

            snprintf(keys2, sizeof(keys2), "%s,%s "
                "{\"from\":\"%s\", \"from_id\":%d, \"fps\": %lf, \"at\": %lf}",
                logging_section, 
                key_name[0] == 'i' ? "in": "out",
                key_name, key_id,
                *frame_count/diff,
                new_ts
                );

            //printf("Sending to journal: %s\n", keys2);
            journal_log(logging_sockfile, keys2);

            *elapsed = new_ts;
            *frame_count = 0;
        }
        return ;
    }
}
