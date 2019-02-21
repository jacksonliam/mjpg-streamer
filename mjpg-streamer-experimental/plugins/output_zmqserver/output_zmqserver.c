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
#include <netinet/in.h>
#include <zmq.h>

#include <linux/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>

#include "package.pb-c.h"
#include "output_zmqserver.h"

#include "../../utils.h"
#include "../../mjpg_streamer.h"

#define OUTPUT_PLUGIN_NAME "UDPSERVER output plugin"

#define MAX_ZMQ_BUFFER_SIZE 10

static pthread_t worker;
static globals *pglobal;
static int fd, ringbuffer_size = -1, ringbuffer_exceed = 0, max_frame_size;
static char *folder = "/tmp";
static unsigned char *frame = NULL;
static unsigned char *frames[MAX_ZMQ_BUFFER_SIZE];
static char *command = NULL;
static int input_number = 0;
static char *mjpgFileName = NULL;
static char *zmqAddress = NULL;
static int zmqBufferSize = 3;
static int zmqBufferPos = 0;
static Pb__Package pbPackage = PB__PACKAGE__INIT; // Package

static void *context;
static void *publisher;
static void *buf;                         // Buffer to store serialized data
static int bufferSize;

static clock_t begin, end;

/******************************************************************************
Description.: print a help message
Input Value.: -
Return Value: -
******************************************************************************/
void help(void)
{
    // TODO: Update available command line arguments.
    fprintf(stderr, " ---------------------------------------------------------------\n" \
            " Help for output plugin..: "OUTPUT_PLUGIN_NAME"\n" \
            " ---------------------------------------------------------------\n" \
            " The following parameters can be passed to this plugin:\n\n" \
            " [-f | --folder ]........: folder to save pictures\n" \
            " [-m | --mjpeg ].........: save the frames to an mjpg file \n" \
            " [-i | --input ].........: read frames from the specified input plugin\n" \
            " The following arguments are takes effect only if the current mode is not MJPG\n" \
            " [-s | --size ]..........: size of ring buffer (max number of pictures to hold)\n" \
            " [-e | --exceed ]........: allow ringbuffer to exceed limit by this amount\n" \
            " [-c | --command ].......: execute command after saving picture\n"\
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
    int i;

    if (mjpgFileName != NULL) {
        close(fd);
    }

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

    // cleanup zmq
    zmq_close (publisher);
    zmq_ctx_destroy (context);

    // Free the allocated serialized buffer
    free(buf);

    // Free protobuf message
    for (i = 0; i < pbPackage.n_frame; ++i)
    {
        free(pbPackage.frame[i]);
    }
    free(pbPackage.frame);
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
    unsigned char *tmp_framebuffer = NULL;

    //  Prepare our context and publisher
    //char zmqAddress[20];
    if (zmqAddress == NULL) {
        LOG("No ZMQ address specified");
    }

    context = zmq_ctx_new ();
    publisher = zmq_socket (context, ZMQ_PUB);
    //snprintf(zmqAddress, 20u, "epgm://eth0;239.1.1.1:%i", zmqPort);

    if (zmq_bind (publisher, zmqAddress) == -1) {
        LOG("Couldn't create zmq socket.\n");
    }

    unsigned len;                      // Length of serialized data
    char topic[] = "frames";
    struct timeval timestamp;
    int i;

    buf = NULL;
    for (i = 0; i < MAX_ZMQ_BUFFER_SIZE; ++i)
    {
        frames[i] = NULL;
    }

    pbPackage.n_frame = zmqBufferSize;
    if ((pbPackage.frame = malloc(sizeof(Pb__Package__Frame*) * pbPackage.n_frame)) == NULL) {
        LOG("not enough memory\n");
    }
    for (i = 0; i < pbPackage.n_frame; ++i)
    {
        if ((pbPackage.frame[i] = malloc(sizeof(Pb__Package__Frame))) == NULL) {
            LOG("not enough memory\n");
        }
        pb__package__frame__init(pbPackage.frame[i]);
    }
    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(worker_cleanup, NULL);

    while(ok >= 0 && !pglobal->stop) {
        DBG("waiting for fresh frame\n");

        pthread_mutex_lock(&pglobal->in[input_number].db);
        pthread_cond_wait(&pglobal->in[input_number].db_update, &pglobal->in[input_number].db);

        /* read buffer */
        frame_size = pglobal->in[input_number].size;

        /* set the right frame to store the data */
        frame = frames[zmqBufferPos];

        /* check if buffer for frame is large enough, increase it if necessary */
        if((frame_size > max_frame_size) || (frame == NULL)) {
            DBG("increasing buffer size to %d\n", frame_size);

            if (frame_size > max_frame_size) {
                max_frame_size = frame_size + (1 << 16);
            }
            if((tmp_framebuffer = realloc(frame, max_frame_size)) == NULL) {
                pthread_mutex_unlock(&pglobal->in[input_number].db);
                LOG("not enough memory\n");
                return NULL;
            }

            if ((setvbuf(stdout, NULL, _IOFBF, max_frame_size)) != 0) {
                DBG("setvbuf failed.\n");
            }

            frame = tmp_framebuffer;
        }

        /* copy v4l2_buffer timeval to user space */
        timestamp = pglobal->in[input_number].timestamp;

        /* copy frame to our local buffer now */
        memcpy(frame, pglobal->in[input_number].buf, frame_size);

        /* resync again with the frame buffer */
        frames[zmqBufferPos] = frame;

        /* allow others to access the global buffer again */
        pthread_mutex_unlock(&pglobal->in[input_number].db);

        if (mjpgFileName == NULL) { // single files with ringbuffer mode
            DBG("Packaging data: %lld\n", counter);
            counter++;

            begin = clock();

            /* allocate enough memory to store a serialized string */
            if ((buf == NULL) || (bufferSize < (max_frame_size * zmqBufferSize + 20)))
            {
                bufferSize = (max_frame_size * zmqBufferSize + 20);
                buf = realloc(buf, bufferSize);
                if (buf == NULL) {
                    LOG("Not enough memory");
                }
            }

            /* fill protobuf data */
            pbPackage.frame[zmqBufferPos]->timestamp_unix = (u_int32_t)time(NULL);
            pbPackage.frame[zmqBufferPos]->timestamp_s = (u_int32_t)timestamp.tv_sec;
            pbPackage.frame[zmqBufferPos]->timestamp_us = (u_int32_t)timestamp.tv_usec;
            pbPackage.frame[zmqBufferPos]->blob.data = frame;
            pbPackage.frame[zmqBufferPos]->blob.len = frame_size;

            zmqBufferPos++;

            if (zmqBufferPos == zmqBufferSize)
            {
                DBG("transmitting ZMQ: %lld\n", counter);
                /* pack protobuf data */
                len = pb__package__get_packed_size(&pbPackage);
                DBG("packing data: %i %i", max_frame_size, len);
                pb__package__pack(&pbPackage, buf);

                DBG("sending data");
                // send data using zmq
                if ((zmq_send(publisher, topic, strlen(topic), ZMQ_SNDMORE) == -1) || (zmq_send(publisher, buf, len, 0) == -1)) {
                    DBG("ZMQ Transmission failure");
                }

                zmqBufferPos = 0;
            }

            end = clock();
            DBG("Time1: %f\n", (double)(end-begin) / CLOCKS_PER_SEC);
            begin = clock();

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

            end = clock();
            DBG("Time2: %f\n", (double)(end-begin) / CLOCKS_PER_SEC);
            begin = clock();

            /*
             * maintain ringbuffer
             * do not maintain ringbuffer for each picture, this saves ressources since
             * each run of the maintainance function involves sorting/malloc/free operations
             */
            if(ringbuffer_exceed <= 0) {
                /* keep ringbuffer excactly at specified siOUTPUT_PLUGIN_NAMEze */
                maintain_ringbuffer(ringbuffer_size);
            } else if(counter == 1 || counter % (ringbuffer_exceed + 1) == 0) {
                DBG("counter: %llu, will clean-up now\n", counter);
                maintain_ringbuffer(ringbuffer_size);
            }

            end = clock();
            DBG("Time3: %f\n", (double)(end-begin) / CLOCKS_PER_SEC);

        } else { // recording to MJPG file
            DBG("Entered else branch!\n");
            /* save picture to file */
            //if(write(fileno(stdout), frame, frame_size) < 0) {
            if(fwrite(frame, sizeof(unsigned char), frame_size, stdout) < 0) {
                OPRINT("could not write to file %s\n", buffer2);
                perror("fwrite()");
                close(fd);
                return NULL;
            }
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
int output_init(output_parameter *param, int id)
{
	int i;
    pglobal = param->global;
    pglobal->out[id].name = malloc((1+strlen(OUTPUT_PLUGIN_NAME))*sizeof(char));
    sprintf(pglobal->out[id].name, "%s", OUTPUT_PLUGIN_NAME);
    DBG("OUT plugin %d name: %s\n", id, pglobal->out[id].name);

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
            {"s", required_argument, 0, 0},
            {"size", required_argument, 0, 0},
            {"e", required_argument, 0, 0},
            {"exceed", required_argument, 0, 0},
            {"i", required_argument, 0, 0},
            {"input", required_argument, 0, 0},
            {"m", required_argument, 0, 0},
            {"mjpeg", required_argument, 0, 0},
            {"a", required_argument, 0, 0},
            {"address", required_argument, 0, 0},
            {"b", required_argument, 0, 0},
            {"buffer_size", required_argument, 0, 0},
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

            /* s, size */
        case 4:
        case 5:
            DBG("case 4,5\n");
            ringbuffer_size = atoi(optarg);
            break;

            /* e, exceed */
        case 6:
        case 7:
            DBG("case 6,7\n");
            ringbuffer_exceed = atoi(optarg);
            break;
            /* i, input*/
        case 8:
        case 9:
            DBG("case 8,9\n");
            input_number = atoi(optarg);
            break;
            /* m mjpeg */
        case 10:
        case 11:
            DBG("case 10,11\n");
            mjpgFileName = strdup(optarg);
            break;
            /* a address */
        case 12:
        case 13:
            DBG("case 12,13\n");
            zmqAddress = strdup(optarg);
            break;
            /* buffer_size */
        case 14:
        case 15:
            DBG("case 14,15\n");
            zmqBufferSize = atoi(optarg);
            break;
        }
    }

    if(!(input_number < pglobal->incnt)) {
        OPRINT("ERROR: the %d input_plugin number is too much only %d plugins loaded\n", input_number, param->global->incnt);
        return 1;
    }

    OPRINT("output folder.....: %s\n", folder);
    OPRINT("input plugin.....: %d: %s\n", input_number, pglobal->in[input_number].plugin);
    if  (mjpgFileName == NULL) {
        if(ringbuffer_size > 0) {
            OPRINT("ringbuffer size...: %d to %d\n", ringbuffer_size, ringbuffer_size + ringbuffer_exceed);
        } else {
            OPRINT("ringbuffer size...: %s\n", "no ringbuffer");
        }
    } else {
        char *fnBuffer = malloc(strlen(mjpgFileName) + strlen(folder) + 3);
        sprintf(fnBuffer, "%s/%s", folder, mjpgFileName);

        OPRINT("output file.......: %s\n", fnBuffer);
        if((fd = open(fnBuffer, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
            OPRINT("could not open the file %s\n", fnBuffer);
            free(fnBuffer);
            return 1;
        }
        free(fnBuffer);
    }

    param->global->out[id].parametercount = 2;

    param->global->out[id].out_parameters = (control*) calloc(2, sizeof(control));

    control take_ctrl;
	take_ctrl.group = IN_CMD_GENERIC;
	take_ctrl.menuitems = NULL;
	take_ctrl.value = 1;
	take_ctrl.class_id = 0;

	take_ctrl.ctrl.id = OUT_FILE_CMD_TAKE;
	take_ctrl.ctrl.type = V4L2_CTRL_TYPE_BUTTON;
	strcpy((char*) take_ctrl.ctrl.name, "Take snapshot");
	take_ctrl.ctrl.minimum = 0;
	take_ctrl.ctrl.maximum = 1;
	take_ctrl.ctrl.step = 1;
	take_ctrl.ctrl.default_value = 0;

	param->global->out[id].out_parameters[0] = take_ctrl;

    control filename_ctrl;
	filename_ctrl.group = IN_CMD_GENERIC;
	filename_ctrl.menuitems = NULL;
	filename_ctrl.value = 1;
	filename_ctrl.class_id = 0;

	filename_ctrl.ctrl.id = OUT_FILE_CMD_FILENAME;
	filename_ctrl.ctrl.type = V4L2_CTRL_TYPE_STRING;
	strcpy((char*) filename_ctrl.ctrl.name, "Filename");
	filename_ctrl.ctrl.minimum = 0;
	filename_ctrl.ctrl.maximum = 32;
	filename_ctrl.ctrl.step = 1;
	filename_ctrl.ctrl.default_value = 0;

	param->global->out[id].out_parameters[1] = filename_ctrl;


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

int output_cmd(int plugin_id, unsigned int control_id, unsigned int group, int value, char *valueStr)
{
    int i = 0;
    DBG("command (%d, value: %d) for group %d triggered for plugin instance #%02d\n", control_id, value, group, plugin_id);
    switch(group) {
		case IN_CMD_GENERIC:
			for(i = 0; i < pglobal->out[plugin_id].parametercount; i++) {
				if((pglobal->out[plugin_id].out_parameters[i].ctrl.id == control_id) && (pglobal->out[plugin_id].out_parameters[i].group == IN_CMD_GENERIC)) {
					DBG("Generic control found (id: %d): %s\n", control_id, pglobal->out[plugin_id].out_parameters[i].ctrl.name);
					switch(control_id) {
                            case OUT_FILE_CMD_TAKE: {
                                if (valueStr != NULL) {
                                    int frame_size = 0;
                                    unsigned char *tmp_framebuffer = NULL;

                                    if(pthread_mutex_lock(&pglobal->in[input_number].db)) {
                                        DBG("Unable to lock mutex\n");
                                        return -1;
                                    }
                                    /* read buffer */
                                    frame_size = pglobal->in[input_number].size;

                                    /* check if buffer for frame is large enough, increase it if necessary */
                                    if(frame_size > max_frame_size) {
                                        DBG("increasing buffer size to %d\n", frame_size);

                                        max_frame_size = frame_size + (1 << 16);
                                        if((tmp_framebuffer = realloc(frame, max_frame_size)) == NULL) {
                                            pthread_mutex_unlock(&pglobal->in[input_number].db);
                                            LOG("not enough memory\n");
                                            return -1;
                                        }

                                        frame = tmp_framebuffer;
                                    }

                                    /* copy frame to our local buffer now */
                                    memcpy(frame, pglobal->in[input_number].buf, frame_size);

                                    /* allow others to access the global buffer again */
                                    pthread_mutex_unlock(&pglobal->in[input_number].db);

                                    DBG("writing file: %s\n", valueStr);

                                    int fd;
                                    /* open file for write */
                                    if((fd = open(valueStr, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
                                        OPRINT("could not open the file %s\n", valueStr);
                                        return -1;
                                    }

                                    /* save picture to file */
                                    //if(write(fileno(stdout), frame, frame_size) < 0) {
                                    if(fwrite(frame, sizeof(unsigned char), frame_size, stdout) < 0) {
                                        OPRINT("could not write to file %s\n", valueStr);
                                        perror("fwrite()");
                                        close(fd);
                                        return -1;
                                    }

                                    close(fd);
                                } else {
                                    DBG("No filename specified\n");
                                    return -1;
                                }
                            } break;
                            case OUT_FILE_CMD_FILENAME: {
                                DBG("Not yet implemented\n");
                                return -1;
                            } break;
                            default: {
                                DBG("Unknown command\n");
                                return -1;
                            } break;
					}
					DBG("Ctrl %s new value: %d\n", pglobal->out[plugin_id].out_parameters[i].ctrl.name, value);
					return 0;
				}
			}
			DBG("Requested generic control (%d) did not found\n", control_id);
			return -1;
			break;
	}
    return 0;
}
