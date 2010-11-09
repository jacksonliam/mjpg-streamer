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
#include <sys/inotify.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../../mjpg_streamer.h"
#include "../../utils.h"

#define INPUT_PLUGIN_NAME "FILE input plugin"

/* private functions and variables to this plugin */
static pthread_t   worker;
static globals     *pglobal;

void *worker_thread(void *);
void worker_cleanup(void *);
void help(void);

static int delay = 0;
static char *folder = NULL;
static char *filename = NULL;
static int rm = 0;
static int plugin_number;

/* global variables for this plugin */
static int fd, rc, wd, size;
static struct inotify_event *ev;

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
            {"d", required_argument, 0, 0},
            {"delay", required_argument, 0, 0},
            {"f", required_argument, 0, 0},
            {"folder", required_argument, 0, 0},
            {"r", no_argument, 0, 0},
            {"remove", no_argument, 0, 0},
            {"n", required_argument, 0, 0},
            {"name", required_argument, 0, 0},
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

            /* d, delay */
        case 2:
        case 3:
            DBG("case 2,3\n");
            delay = atoi(optarg);
            break;

            /* f, folder */
        case 4:
        case 5:
            DBG("case 4,5\n");
            folder = malloc(strlen(optarg) + 2);
            strcpy(folder, optarg);
            if(optarg[strlen(optarg)-1] != '/')
                strcat(folder, "/");
            break;

            /* r, remove */
        case 6:
        case 7:
            DBG("case 6,7\n");
            rm = 1;
            break;

            /* n, name */
        case 8:
        case 9:
            DBG("case 8,9\n");
            filename = malloc(strlen(optarg) + 2);
            strcpy(filename, optarg);
            break;

        default:
            DBG("default case\n");
            help();
            return 1;
        }
    }

    pglobal = param->global;

    /* check for required parameters */
    if(folder == NULL) {
        IPRINT("ERROR: no folder specified\n");
        return 1;
    }

    IPRINT("folder to watch...: %s\n", folder);
    IPRINT("forced delay......: %i\n", delay);
    IPRINT("delete file.......: %s\n", (rm) ? "yes, delete" : "no, do not delete");
    IPRINT("filename must be..: %s\n", (filename == NULL) ? "-no filter for certain filename set-" : filename);

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

    rc = fd = inotify_init();
    if(rc == -1) {
        perror("could not initilialize inotify");
        return 1;
    }

    rc = wd = inotify_add_watch(fd, folder, IN_CLOSE_WRITE | IN_MOVED_TO | IN_ONLYDIR);
    if(rc == -1) {
        perror("could not add watch");
        return 1;
    }

    size = sizeof(struct inotify_event) + (1 << 16);
    ev = malloc(size);
    if(ev == NULL) {
        perror("not enough memory");
        return 1;
    }

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
    " [-d | --delay ]........: delay to pause between frames\n" \
    " [-f | --folder ].......: folder to watch for new JPEG files\n" \
    " [-r | --remove ].......: remove/delete JPEG file after reading\n" \
    " [-n | --name ].........: ignore changes unless filename matches\n" \
    " ---------------------------------------------------------------\n");
}

/* the single writer thread */
void *worker_thread(void *arg)
{
    char buffer[1<<16];
    int file;
    size_t filesize = 0;
    struct stat stats;

    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(worker_cleanup, NULL);

    while(!pglobal->stop) {

        /* wait for new frame, read will block until something happens */
        rc = read(fd, ev, size);
        if(rc == -1) {
            perror("reading inotify events failed");
            break;
        }

        /* sanity check */
        if(wd != ev->wd) {
            fprintf(stderr, "This event is not for the watched directory (%d != %d)\n", wd, ev->wd);
            continue;
        }

        if(ev->mask & (IN_IGNORED | IN_Q_OVERFLOW | IN_UNMOUNT)) {
            fprintf(stderr, "event mask suggests to stop\n");
            break;
        }

        /* prepare filename */
        snprintf(buffer, sizeof(buffer), "%s%s", folder, ev->name);

        /* check if the filename matches specified parameter (if given) */
        if((filename != NULL) && (strcmp(filename, ev->name) != 0)) {
            DBG("ignoring this change (specified filename does not match)\n");
            continue;
        }
        DBG("new file detected: %s\n", buffer);

        /* open file for reading */
        rc = file = open(buffer, O_RDONLY);
        if(rc == -1) {
            perror("could not open file for reading");
            break;
        }

        /* approximate size of file */
        rc = fstat(file, &stats);
        if(rc == -1) {
            perror("could not read statistics of file");
            close(file);
            break;
        }

        filesize = stats.st_size;

        /* copy frame from file to global buffer */
        pthread_mutex_lock(&pglobal->in[plugin_number].db);

        /* allocate memory for frame */
        if(pglobal->in[plugin_number].buf != NULL) free(pglobal->in[plugin_number].buf);
        pglobal->in[plugin_number].buf = malloc(filesize + (1 << 16));
        if(pglobal->in[plugin_number].buf == NULL) {
            fprintf(stderr, "could not allocate memory\n");
            break;
        }

        if((pglobal->in[plugin_number].size = read(file, pglobal->in[plugin_number].buf, filesize)) == -1) {
            perror("could not read from file");
            free(pglobal->in[plugin_number].buf); pglobal->in[plugin_number].buf = NULL; pglobal->in[plugin_number].size = 0;
            pthread_mutex_unlock(&pglobal->in[plugin_number].db);
            close(file);
            break;
        }

        DBG("new frame copied (size: %d)\n", pglobal->in[plugin_number].size);

        /* signal fresh_frame */
        pthread_cond_broadcast(&pglobal->in[plugin_number].db_update);
        pthread_mutex_unlock(&pglobal->in[plugin_number].db);

        close(file);

        /* delete file if necessary */
        if(rm) {
            rc = unlink(buffer);
            if(rc == -1) {
                perror("could not remove/delete file");
            }
        }

        if(delay != 0) usleep(1000 * delay);
    }

    DBG("leaving input thread, calling cleanup function now\n");
    /* call cleanup handler, signal with the parameter */
    pthread_cleanup_pop(1);

    return NULL;
}

void worker_cleanup(void *arg)
{
    static unsigned char first_run = 1;

    if(!first_run) {
        DBG("already cleaned up ressources\n");
        return;
    }

    first_run = 0;
    DBG("cleaning up ressources allocated by input thread\n");

    if(pglobal->in[plugin_number].buf != NULL) free(pglobal->in[plugin_number].buf);

    free(ev);

    rc = inotify_rm_watch(fd, wd);
    if(rc == -1) {
        perror("could not close watch descriptor");
    }

    rc = close(fd);
    if(rc == -1) {
        perror("could not close filedescriptor");
    }
}




