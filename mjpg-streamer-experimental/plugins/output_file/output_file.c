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
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <syslog.h>

#include <dirent.h>

#include "../../utils.h"
#include "../../mjpg_streamer.h"

#define OUTPUT_PLUGIN_NAME "FILE output plugin"

static pthread_t worker;
static globals *pglobal;
static int fd, delay, ringbuffer_size = -1, ringbuffer_exceed = 0, max_frame_size;
static char *folder = "/tmp";
static unsigned char *frame = NULL;
static char *command = NULL;
static int input_number = 0;

/******************************************************************************
Description.: print a help message
Input Value.: -
Return Value: -
******************************************************************************/
void help(void)
{
    fprintf(stderr, " ---------------------------------------------------------------\n" \
            " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
            " ---------------------------------------------------------------\n" \
            " The following parameters can be passed to this plugin:\n\n" \
            " [-f | --folder ]........: folder to save pictures\n" \
            " [-m | --mjpeg ]........: save the stream to an mjpeg file\n" \
            " [-d | --delay ].........: delay after saving pictures in ms\n" \
            " [-s | --size ]..........: size of ring buffer (max number of pictures to hold)\n" \
            " [-e | --exceed ]........: allow ringbuffer to exceed limit by this amount\n" \
            " [-c | --command ].......: execute command after saving picture\n\n" \
            " [-i | --input ].......: read frames from the specified input plugin\n\n" \
            " ---------------------------------------------------------------\n");
}

/******************************************************************************
Description.: clean up allocated ressources
Input Value.: unused argument
Return Value: -
******************************************************************************/
void worker_cleanup(void *arg)
{
    static unsigned char first_run = 1;

    if(!first_run) {
        DBG("already cleaned up ressources\n");
        return;
    }

    first_run = 0;
    OPRINT("cleaning up ressources allocated by worker thread\n");

    if(frame != NULL) {
        free(frame);
    }
    close(fd);
}

/******************************************************************************
Description.: compares a directory entry with a pattern
Input Value.: directory entry
Return Value: 0 if string do not match, 1 if they match
******************************************************************************/
int check_for_filename(const struct dirent *entry)
{
    int rc;

    int year, month, day, hour, minute, second;
    unsigned long long number;

    /*
     * try to scan the string using scanf
     * I would like to use a define for this format string later...
     */
    rc = sscanf(entry->d_name, "%d_%d_%d_%d_%d_%d_picture_%09llu.jpg", &year, \
                &month, \
                &day, \
                &hour, \
                &minute, \
                &second, \
                &number);

    DBG("%s, rc is %d (%d, %d, %d, %d, %d, %d, %llu)\n", entry->d_name, \
        rc, \
        year, \
        month, \
        day, \
        hour, \
        minute, \
        second, \
        number);

    /* if scanf could find all values, it matches our filenames */
    if(rc != 7) return 0;

    return 1;
}

/******************************************************************************
Description.: delete oldest files, just keep "size" most recent files
              This funtion MAY delete the wrong files if the time is not valid
Input Value.: how many files to keep
Return Value: -
******************************************************************************/
void maintain_ringbuffer(int size)
{
    struct dirent **namelist;
    int n, i;
    char buffer[1<<16];

    /* do nothing if ringbuffer is not set or wrong value is set */
    if(size < 0) return;

    /* get a sorted list of directory items */
    n = scandir(folder, &namelist, check_for_filename, alphasort);
    if(n < 0) {
        perror("scandir");
        return;
    }

    DBG("found %d directory entries\n", n);

    /* delete the first (thus oldest) number of files */
    for(i = 0; i < (n - size); i++) {

        /* put together the folder name and the directory item */
        snprintf(buffer, sizeof(buffer), "%s/%s", folder, namelist[i]->d_name);

        DBG("delete: %s\n", buffer);

        /* mark item for deletion */
        if(unlink(buffer) == -1) {
            perror("could not delete file");
        }

        /* free allocated memory for name */
        free(namelist[i]);
    }

    /* keep the rest, but we still have to free every result */
    for(i = MAX(n - size, 0); i < n; i++) {
        DBG("keep: %s\n", namelist[i]->d_name);
        free(namelist[i]);
    }

    /* free last just allocated ressources */
    free(namelist);
}

/******************************************************************************
Description.: this is the main worker thread
              it loops forever, grabs a fresh frame and stores it to file
Input Value.:
Return Value:
******************************************************************************/
void *worker_thread(void *arg)
{
    int ok = 1, frame_size = 0, rc = 0;
    char buffer1[1024] = {0}, buffer2[1024] = {0};
    unsigned long long counter = 0;
    time_t t;
    struct tm *now;
    unsigned char *tmp_framebuffer = NULL;

    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(worker_cleanup, NULL);

    while(ok >= 0 && !pglobal->stop) {
        DBG("waiting for fresh frame\n");
        pthread_mutex_lock(&pglobal->in[input_number].db);
        pthread_cond_wait(&pglobal->in[input_number].db_update, &pglobal->in[input_number].db);

        /* read buffer */
        frame_size = pglobal->in[input_number].size;

        /* check if buffer for frame is large enough, increase it if necessary */
        if(frame_size > max_frame_size) {
            DBG("increasing buffer size to %d\n", frame_size);

            max_frame_size = frame_size + (1 << 16);
            if((tmp_framebuffer = realloc(frame, max_frame_size)) == NULL) {
                pthread_mutex_unlock(&pglobal->in[input_number].db);
                LOG("not enough memory\n");
                return NULL;
            }

            frame = tmp_framebuffer;
        }

        /* copy frame to our local buffer now */
        memcpy(frame, pglobal->in[input_number].buf, frame_size);

        /* allow others to access the global buffer again */
        pthread_mutex_unlock(&pglobal->in[input_number].db);

        /* prepare filename */
        memset(buffer1, 0, sizeof(buffer1));
        memset(buffer2, 0, sizeof(buffer2));

        /* get current time */
        t = time(NULL);
        now = localtime(&t);
        if(now == NULL) {
            perror("localtime");
            return NULL;
        }

        /* prepare string, add time and date values */
        if(strftime(buffer1, sizeof(buffer1), "%%s/%Y_%m_%d_%H_%M_%S_picture_%%09llu.jpg", now) == 0) {
            OPRINT("strftime returned 0\n");
            free(frame); frame = NULL;
            return NULL;
        }

        /* finish filename by adding the foldername and a counter value */
        snprintf(buffer2, sizeof(buffer2), buffer1, folder, counter);

        counter++;

        DBG("writing file: %s\n", buffer2);

        /* open file for write */
        if((fd = open(buffer2, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
            OPRINT("could not open the file %s\n", buffer2);
            return NULL;
        }

        /* save picture to file */
        if(write(fd, frame, frame_size) < 0) {
            OPRINT("could not write to file %s\n", buffer2);
            perror("write()");
            close(fd);
            return NULL;
        }

        close(fd);

        /* call the command if user specified one, pass current filename as argument */
        if(command != NULL) {
            memset(buffer1, 0, sizeof(buffer1));

            /* buffer2 still contains the filename, pass it to the command as parameter */
            snprintf(buffer1, sizeof(buffer1), "%s \"%s\"", command, buffer2);
            DBG("calling command %s", buffer1);

            /* in addition provide the filename as environment variable */
            if((rc = setenv("MJPG_FILE", buffer2, 1)) != 0) {
                LOG("setenv failed (return value %d)\n", rc);
            }

            /* execute the command now */
            if((rc = system(buffer1)) != 0) {
                LOG("command failed (return value %d)\n", rc);
            }
        }

        /*
         * maintain ringbuffer
         * do not maintain ringbuffer for each picture, this saves ressources since
         * each run of the maintainance function involves sorting/malloc/free operations
         */
        if(ringbuffer_exceed <= 0) {
            /* keep ringbuffer excactly at specified size */
            maintain_ringbuffer(ringbuffer_size);
        } else if(counter == 1 || counter % (ringbuffer_exceed + 1) == 0) {
            DBG("counter: %llu, will clean-up now\n", counter);
            maintain_ringbuffer(ringbuffer_size);
        }

        /* if specified, wait now */
        if(delay > 0) {
            usleep(1000 * delay);
        }
    }

    /* cleanup now */
    pthread_cleanup_pop(1);

    return NULL;
}

/*** plugin interface functions ***/
/******************************************************************************
Description.: this function is called first, in order to initialize
              this plugin and pass a parameter string
Input Value.: parameters
Return Value: 0 if everything is OK, non-zero otherwise
******************************************************************************/
int output_init(output_parameter *param)
{
	int i;
    delay = 0;

    param->argv[0] = OUTPUT_PLUGIN_NAME;

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
            {"f", required_argument, 0, 0},
            {"folder", required_argument, 0, 0},
            {"d", required_argument, 0, 0},
            {"delay", required_argument, 0, 0},
            {"s", required_argument, 0, 0},
            {"size", required_argument, 0, 0},
            {"e", required_argument, 0, 0},
            {"exceed", required_argument, 0, 0},
            {"c", required_argument, 0, 0},
            {"command", required_argument, 0, 0},
            {"m", required_argument, 0, 0},
            {"mjpeg", required_argument, 0, 0},
            {"i", required_argument, 0, 0},
            {"input", required_argument, 0, 0},
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

            /* f, folder */
        case 2:
        case 3:
            DBG("case 2,3\n");
            folder = malloc(strlen(optarg) + 1);
            strcpy(folder, optarg);
            if(folder[strlen(folder)-1] == '/')
                folder[strlen(folder)-1] = '\0';
            break;

            /* d, delay */
        case 4:
        case 5:
            DBG("case 4,5\n");
            delay = atoi(optarg);
            break;

            /* s, size */
        case 6:
        case 7:
            DBG("case 6,7\n");
            ringbuffer_size = atoi(optarg);
            break;

            /* e, exceed */
        case 8:
        case 9:
            DBG("case 8,9\n");
            ringbuffer_exceed = atoi(optarg);
            break;

            /* c, command */
        case 10:
        case 11:
            DBG("case 10,11\n");
            command = strdup(optarg);
            break;
        case 12:
        case 13:
            DBG("case 12,13\n");
            input_number = atoi(optarg);
            break;
        }
    }

    pglobal = param->global;
    if(!(input_number < pglobal->incnt)) {
        OPRINT("ERROR: the %d input_plugin number is too much only %d plugins loaded\n", input_number, pglobal->incnt);
        return 1;
    }

    OPRINT("output folder.....: %s\n", folder);
    OPRINT("input plugin.....: %d: %s\n", input_number, pglobal->in[input_number].plugin);
    OPRINT("delay after save..: %d\n", delay);
    if(ringbuffer_size > 0) {
        OPRINT("ringbuffer size...: %d to %d\n", ringbuffer_size, ringbuffer_size + ringbuffer_exceed);
    } else {
        OPRINT("ringbuffer size...: %s\n", "no ringbuffer");
    }
    OPRINT("command...........: %s\n", (command == NULL) ? "disabled" : command);
    return 0;
}

/******************************************************************************
Description.: calling this function stops the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_stop(int id)
{
    DBG("will cancel worker thread\n");
    pthread_cancel(worker);
    return 0;
}

/******************************************************************************
Description.: calling this function creates and starts the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_run(int id)
{
    DBG("launching worker thread\n");
    pthread_create(&worker, 0, worker_thread, NULL);
    pthread_detach(worker);
    return 0;
}

int output_cmd(int plugin, unsigned int control_id, unsigned int group, int value)
{
    DBG("command (%d, value: %d) for group %d triggered for plugin instance #%02d\n", control_id, value, group, plugin);
    return 0;
}
