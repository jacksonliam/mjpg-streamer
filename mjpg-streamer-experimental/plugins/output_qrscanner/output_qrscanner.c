/*******************************************************************************
#                                                                              #
#      MJPG-streamer QR Scanner Output Plugin                                  #
#      Scans captured frames for QR codes and executes external programs       #
#                                                                              #
#      Copyright (C) 2025 Ultimaker B.V. All rights reserved.                  #
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

#include "output_qrscanner.h"
#include <getopt.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

#define OUTPUT_PLUGIN_NAME "QR Scanner output plugin"
#define QR_SCAN_INTERVAL_MS 1000  // Scan every second
#define MAX_FRAME_SIZE (2 * 1024 * 1024)  // 2MB - covers most HD JPEG frames

static pthread_t worker;
static globals *pglobal;
static int input_number = 0;
static int scan_interval = QR_SCAN_INTERVAL_MS;
static unsigned char frame_buffer[MAX_FRAME_SIZE];  // Static frame buffer
static unsigned char *frame = frame_buffer;

static struct quirc *qr_decoder = NULL;

static char *external_program = NULL;
static int backoff_frames = 0;  // Backoff count in scan intervals after decode (default: 0)
static int remaining_backoff_frames = 0;  // Remaining frames to skip
static volatile sig_atomic_t scanning_enabled = 1;  // Flag to control scanning state

/******************************************************************************
Description.: cleanup any zombie child processes (non-blocking)
Input Value.: -
Return Value: -
******************************************************************************/
void cleanup_child_processes(void)
{
    pid_t pid;
    int status;

    /* Reap any zombie child processes without blocking */
    while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        /* Child process reaped */
    }
}

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
            " [-i | --input ].........: read frames from the specified input plugin (default: 0)\n" \
            " [-d | --delay ].........: delay between QR scans in ms (default: 1000)\n" \
            " [-e | --exec ]...........: external program to execute with QR data\n" \
            " [-b | --backoff ].......: backoff count in scan intervals after decode (default: 0)\n" \
            " [-s | --signals ].......: enable signal-based scanning control (default: disabled)\n" \
            " ---------------------------------------------------------------\n" \
            " This plugin scans incoming frames for QR codes and passes\n" \
            " the results to a specified external program.\n" \
            " \n" \
            " The -e option specifies the program to execute when a QR code\n" \
            " is detected or when QR decoding fails. The program will receive:\n" \
            " - QR_DATA_FILE: path to temporary file containing QR data (on success)\n" \
            " - QR_DATA_SIZE: size of QR data in bytes (on success)\n" \
            " - QR_ERROR: error message (on decode failure)\n" \
            " \n" \
            " The temporary file is created with mkstemp() and should be cleaned\n" \
            " up by the external program after processing.\n" \
            " \n" \
            " The -b option sets a backoff period after processing\n" \
            " to prevent repeated processing of the same QR code. Set to 0\n" \
            " to disable backoff (process every detected QR code).\n" \
            " The backoff is specified in scan intervals, not time.\n" \
            " \n" \
            " The -s option enables signal-based control of QR scanning:\n" \
            " - SIGUSR1: Enable QR scanning\n" \
            " - SIGUSR2: Disable QR scanning\n" \
            " When -s is specified, scanning is disabled by default until SIGUSR1 is received.\n" \
            " When -s is not specified, scanning runs continuously.\n" \
            " \n" \
            " Example usage:\n" \
            " mjpg_streamer -i input_uvc.so -o \"output_qrscanner.so -e /usr/local/bin/qr_handler.sh\"\n" \
            " \n" \
            " With backoff to prevent repeated processing:\n" \
            " mjpg_streamer -i input_uvc.so -o \"output_qrscanner.so -e /path/to/handler -b 5\"\n" \
            " \n" \
            " With signal-based control:\n" \
            " mjpg_streamer -i input_uvc.so -o \"output_qrscanner.so -e /path/to/handler -s\"\n" \
            " kill -USR1 <pid>  # Enable scanning\n" \
            " kill -USR2 <pid>  # Disable scanning\n" \
            " \n" \
            " The external program will receive environment variables indicating\n" \
            " success (QR_DATA_FILE, QR_DATA_SIZE) or failure (QR_ERROR) and will\n" \
            " be executed in a separate process.\n" \
            " ---------------------------------------------------------------\n");
}

/******************************************************************************
Description.: clean up allocated resources
Input Value.: unused argument
Return Value: -
******************************************************************************/
void worker_cleanup(void *arg)
{
    static unsigned char first_run = 1;

    if(!first_run) {
        DBG("already cleaned up resources\n");
        return;
    }

    first_run = 0;
    OPRINT("cleaning up resources allocated by worker thread\n");

    /* Clean up any remaining child processes */
    cleanup_child_processes();

    if(qr_decoder != NULL) {
        quirc_destroy(qr_decoder);
        qr_decoder = NULL;
    }

    if(external_program != NULL) {
        free(external_program);
        external_program = NULL;
    }
}

/******************************************************************************
Description.: initialize the plugin
Input Value.: plugin parameters
Return Value: 0 if ok, -1 on error
******************************************************************************/
int output_init(output_parameter *param, int id)
{
    int i;
    int option_index = 0, c = 0;
    static struct option long_options[] = {
        {"help", no_argument, 0, 0},
        {"input", required_argument, 0, 'i'},
        {"delay", required_argument, 0, 'd'},
        {"exec", required_argument, 0, 'e'},
        {"backoff", required_argument, 0, 'b'},
        {"signals", no_argument, 0, 's'},
        {0, 0, 0, 0}
    };

    OPRINT("initializing output plugin: \"%s\"\n", OUTPUT_PLUGIN_NAME);

    pglobal = param->global;

    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    /* parse command line parameters */
    reset_getopt();
    while(1) {
        c = getopt_long_only(param->argc, param->argv, "i:d:e:b:s", long_options, &option_index);

        /* no more options to parse */
        if(c == -1) break;

        /* unrecognized option */
        if(c == '?') {
            help();
            return -1;
        }

        switch(c) {
            case 0:
                DBG("option %s", long_options[option_index].name);
                if(optarg) DBG(" with arg %s", optarg);
                DBG("\n");
                break;

            case 'i':
                input_number = atoi(optarg);
                break;

            case 'd':
                scan_interval = atoi(optarg);
                if(scan_interval < 100) {
                    OPRINT("scan interval too small, setting to 100ms\n");
                    scan_interval = 100;
                }
                break;

            case 'e':
                if(external_program != NULL) {
                    free(external_program);
                }
                external_program = strdup(optarg);
                break;

            case 'b':
                backoff_frames = atoi(optarg);
                if(backoff_frames < 0) {
                    OPRINT("backoff frames cannot be negative, setting to 0\n");
                    backoff_frames = 0;
                }
                break;

            case 's':
                scanning_enabled = 0;  // Disable scanning when signal control is enabled
                if(setup_signal_handlers() < 0) {
                    OPRINT("ERROR: failed to setup signal handlers\n");
                    return -1;
                }
                break;

            default:
                help();
                return -1;
        }
    }

    /* check for required input plugin */
    if(pglobal->incnt <= input_number) {
        OPRINT("ERROR: input plugin #%d not available\n", input_number);
        return -1;
    }

    /* Initialize quirc decoder */
    qr_decoder = quirc_new();
    if(qr_decoder == NULL) {
        OPRINT("ERROR: failed to create quirc decoder\n");
        return -1;
    }
    OPRINT("using quirc QR decoder\n");

    /* Use static frame buffer */
    frame = frame_buffer;
    OPRINT("using static frame buffer of %d bytes\n", MAX_FRAME_SIZE);

    OPRINT("input plugin.....: %d\n", input_number);
    OPRINT("scan interval....: %d ms\n", scan_interval);
    OPRINT("backoff frames...: %d intervals\n", backoff_frames);
    if(external_program != NULL) {
        OPRINT("external program.: %s\n", external_program);
    } else {
        OPRINT("external program.: not specified (QR codes will only be logged)\n");
    }

    return 0;
}

/******************************************************************************
Description.: stop the plugin
Input Value.: plugin instance id
Return Value: 0 if ok, -1 on error
******************************************************************************/
int output_stop(int id)
{
    DBG("will cancel worker thread\n");
    pthread_cancel(worker);
    return 0;
}

/******************************************************************************
Description.: start the plugin thread
Input Value.: plugin instance id
Return Value: 0 if ok, -1 on error
******************************************************************************/
int output_run(int id)
{
    DBG("launching worker thread\n");
    pthread_create(&worker, 0, worker_thread, NULL);
    pthread_detach(worker);
    return 0;
}

/******************************************************************************
Description.: process commands sent to this plugin
Input Value.: plugin id, control id, group, value, value string
Return Value: 0 if ok, -1 on error
******************************************************************************/
int output_cmd(int plugin_id, unsigned int control_id, unsigned int group, int value, char *value_str)
{
    DBG("command (%d, value: %d) for group %d triggered for plugin instance #%02d\n",
        control_id, value, group, plugin_id);
    return 0;
}

/******************************************************************************
Description.: worker thread - continuously scans frames for QR codes
Input Value.: unused
Return Value: NULL
******************************************************************************/
void *worker_thread(void *arg)
{
    int ok = 1, frame_size = 0;
    struct timespec wait_time;

    /* Unblock SIGUSR1 and SIGUSR2 for this thread only */
    /* This allows the signal handlers to be called in this thread */
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGUSR1);
    sigaddset(&signal_set, SIGUSR2);

    int sig_result = pthread_sigmask(SIG_UNBLOCK, &signal_set, NULL);
    if(sig_result != 0) {
        OPRINT("ERROR: could not unblock SIGUSR1/SIGUSR2 signals: %s\n", strerror(sig_result));
        return NULL;
    }

    /* set cleanup handler to cleanup allocated resources */
    pthread_cleanup_push(worker_cleanup, NULL);

    while((ok >= 0) && (!pglobal->stop)) {
        /* Check if scanning is enabled before doing any frame processing */
        if(!scanning_enabled) {
            goto sleep_and_continue;
        }

        /* Decrement backoff counter if active */
        if(remaining_backoff_frames > 0) {
            remaining_backoff_frames--;
            DBG("In backoff period, %d frames remaining\n", remaining_backoff_frames);
            goto sleep_and_continue;
        }

        DBG("waiting for fresh frame\n");

        /* wait for fresh frame */
        pthread_mutex_lock(&pglobal->in[input_number].db);
        pthread_cond_wait(&pglobal->in[input_number].db_update, &pglobal->in[input_number].db);

        /* read buffer */
        frame_size = pglobal->in[input_number].size;

        /* check if frame size is within buffer limits */
        if(frame_size > MAX_FRAME_SIZE) {
            pthread_mutex_unlock(&pglobal->in[input_number].db);
            OPRINT("ERROR: frame size %d exceeds maximum buffer size %d\n", frame_size, MAX_FRAME_SIZE);
            goto sleep_and_continue;
        }

        /* copy frame to our local buffer now */
        memcpy(frame, pglobal->in[input_number].buf, frame_size);

        /* allow others to access the global buffer again */
        pthread_mutex_unlock(&pglobal->in[input_number].db);

        /* process the frame for QR codes */
        if(process_frame(frame, frame_size) < 0) {
            DBG("frame processing failed\n");
        }

    sleep_and_continue:
        /* cleanup any zombie child processes */
        cleanup_child_processes();

        /* wait for the specified interval before next scan */
        wait_time.tv_sec = scan_interval / 1000;
        wait_time.tv_nsec = (scan_interval % 1000) * 1000000;
        nanosleep(&wait_time, NULL);
    }

    pthread_cleanup_pop(1);
    return NULL;
}

/******************************************************************************
Description.: process a frame and scan for QR codes
Input Value.: frame data and size
Return Value: 0 if ok, -1 on error
******************************************************************************/
int process_frame(unsigned char *frame_data, int frame_size)
{
    unsigned char *gray_data = NULL;
    int width, height;
    int result = -1;

    /* Decode JPEG to grayscale */
    if(decode_jpeg_to_gray(frame_data, frame_size, &gray_data, &width, &height) < 0) {
        DBG("failed to decode JPEG image\n");
        return -1;
    }

    /* Scan for QR codes using quirc */
    result = decode_qr_codes_quirc(gray_data, width, height);

    /* Cleanup */
    if(gray_data) free(gray_data);

    return result;
}

/******************************************************************************
Description.: decode JPEG to grayscale using libjpeg
Input Value.: JPEG data, size, output pointers for gray data, width, height
Return Value: 0 if ok, -1 on error
******************************************************************************/
int decode_jpeg_to_gray(unsigned char *jpeg_data, int jpeg_size,
                       unsigned char **gray_data, int *width, int *height)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    unsigned char *output_data = NULL;
    int row_stride;

    /* Initialize the JPEG decompression object */
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    /* Set input source */
    jpeg_mem_src(&cinfo, jpeg_data, jpeg_size);

    /* Read header */
    if(jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }

    /* Force grayscale output */
    cinfo.out_color_space = JCS_GRAYSCALE;
    cinfo.output_components = 1;

    /* Start decompression */
    jpeg_start_decompress(&cinfo);

    *width = cinfo.output_width;
    *height = cinfo.output_height;
    row_stride = cinfo.output_width * cinfo.output_components;

    /* Allocate output buffer */
    output_data = (unsigned char*)malloc(cinfo.output_height * row_stride);
    if(!output_data) {
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }

    /* Read scanlines */
    while(cinfo.output_scanline < cinfo.output_height) {
        row_pointer[0] = &output_data[cinfo.output_scanline * row_stride];
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
    }

    /* Finish decompression */
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    *gray_data = output_data;
    return 0;
}

/******************************************************************************
Description.: decode QR codes using quirc from grayscale image data
Input Value.: grayscale data, width, height
Return Value: 0 if ok, -1 on error
******************************************************************************/
int decode_qr_codes_quirc(unsigned char *gray_data, int width, int height)
{
    uint8_t *image;
    int num_codes;
    struct quirc_code code;
    struct quirc_data data;
    quirc_decode_error_t err;

    /* Resize quirc decoder if needed */
    if(quirc_resize(qr_decoder, width, height) < 0) {
        OPRINT("ERROR: failed to resize quirc decoder\n");
        return -1;
    }

    /* Get image buffer and copy data */
    image = quirc_begin(qr_decoder, NULL, NULL);
    memcpy(image, gray_data, width * height);
    quirc_end(qr_decoder);

    /* Process detected QR codes */
    num_codes = quirc_count(qr_decoder);
    if(num_codes > 0) {
        OPRINT("found %d QR code(s)\n", num_codes);

        /* Process only the first QR code found (assume only 1 QR code exists) */
        quirc_extract(qr_decoder, 0, &code);
        err = quirc_decode(&code, &data);

        if(err) {
            DBG("DECODE FAILED: %s\n", quirc_strerror(err));

            /* Execute external program with QR error */
            execute_external_program(external_program, NULL, 0, quirc_strerror(err));
        } else {
            /* Check if data contains any non-printable characters */
            int has_binary = 0;
            for(int i = 0; i < data.payload_len; i++) {
                if(!isprint(data.payload[i]) && data.payload[i] != '\n' && data.payload[i] != '\r' && data.payload[i] != '\t') {
                    has_binary = 1;
                    break;
                }
            }

            if(has_binary) {
                OPRINT("QR code data (%d bytes): <binary data>\n", data.payload_len);
            } else {
                OPRINT("QR code data (%d bytes): %.*s\n", data.payload_len, data.payload_len, data.payload);
            }

            /* Execute external program with QR data */
            execute_external_program(external_program, (char*)data.payload, data.payload_len, NULL);

            /* Set backoff counter only on successful decode */
            remaining_backoff_frames = backoff_frames;
        }
    }

    return 0;
}

/******************************************************************************
Description.: execute external program with QR data or error (fire and forget)
Input Value.: program path, QR data bytes (NULL for errors), QR data length, QR error string (NULL for success)
Return Value: 0 if ok, -1 on error (but errors don't terminate plugin)
******************************************************************************/
int execute_external_program(const char *program_path, const char *qr_data, int qr_data_len, const char *qr_error)
{
    char temp_filename[64] = "";
    int temp_fd = -1;
    ssize_t bytes_written;
    pid_t pid;
    char size_str[32];

    if(program_path == NULL) {
        /* No external program specified - return without error */
        return 0;
    }

    if((qr_data == NULL) && (qr_error == NULL)) {
        OPRINT("WARNING: Cannot execute external program - no data or error provided\n");
        return -1;
    }

    if((qr_data != NULL) && (qr_error != NULL)) {
        OPRINT("WARNING: Cannot execute external program - both data and error provided\n");
        return -1;
    }

    if(qr_data != NULL) {
        /* Success case: write QR data to temporary file */
        OPRINT("Launching external program: %s\n", program_path);
        DBG("QR data length: %d bytes\n", qr_data_len);

        /* Create temporary file for QR data */
        strcpy(temp_filename, "/tmp/qr_data_XXXXXX");
        temp_fd = mkstemp(temp_filename);
        if(temp_fd == -1) {
            OPRINT("WARNING: Failed to create temporary file for QR data: %s\n", strerror(errno));
            return -1;
        }

        /* Write QR data to temporary file using actual length */
        bytes_written = write(temp_fd, qr_data, qr_data_len);
        if(bytes_written != (ssize_t)qr_data_len) {
            OPRINT("WARNING: Failed to write QR data to temporary file: %s\n", strerror(errno));
            close(temp_fd);
            unlink(temp_filename);
            return -1;
        }
        close(temp_fd);
    } else {
        /* Error case */
        OPRINT("Launching external program for QR error: %s\n", program_path);
        DBG("QR error: %s\n", qr_error);
    }

    /* Fork a child process for fire-and-forget execution */
    pid = fork();

    switch(pid) {
        case -1:
            /* Fork failed */
            OPRINT("WARNING: Failed to fork process for external program execution: %s\n", strerror(errno));
            OPRINT("         QR code processing will continue without external program\n");
            if(qr_data != NULL) {
                unlink(temp_filename);
            }
            return -1;

        case 0:
            /* Child process */
            if(qr_data != NULL) {
                /* Success case: set file-based environment variables */
                unsetenv("QR_ERROR");

                /* Set QR_DATA_FILE environment variable with temp file path */
                if(setenv("QR_DATA_FILE", temp_filename, 1) != 0) {
                    fprintf(stderr, "QR Scanner WARNING: Failed to set QR_DATA_FILE environment variable: %s\n", strerror(errno));
                }

                /* Set QR_DATA_SIZE environment variable with data size */
                snprintf(size_str, sizeof(size_str), "%d", qr_data_len);
                if(setenv("QR_DATA_SIZE", size_str, 1) != 0) {
                    fprintf(stderr, "QR Scanner WARNING: Failed to set QR_DATA_SIZE environment variable: %s\n", strerror(errno));
                }
            } else {
                /* Error case: set error environment variable */
                unsetenv("QR_DATA_FILE");
                unsetenv("QR_DATA_SIZE");

                /* Set QR_ERROR environment variable */
                if(setenv("QR_ERROR", qr_error, 1) != 0) {
                    fprintf(stderr, "QR Scanner WARNING: Failed to set QR_ERROR environment variable: %s\n", strerror(errno));
                }
            }

            /* Execute the program directly */
            execl(program_path, program_path, (char*)NULL);

            /* If we reach here, exec failed */
            fprintf(stderr, "QR Scanner ERROR: Failed to execute external program '%s': %s\n",
                    program_path, strerror(errno));

            /* Clean up temp file on exec failure (if it exists) */
            if(qr_data != NULL) {
                unlink(temp_filename);
            }
            exit(1);

        default:
            /* Parent process - return immediately (fire and forget) */
            if(qr_data != NULL) {
                DBG("Child process %d launched for external program, temp file: %s\n", pid, temp_filename);
            } else {
                DBG("Child process %d launched for external program\n", pid);
            }

            /* Reap child process to prevent zombies (non-blocking) */
            waitpid(pid, NULL, WNOHANG);

            /* Note: For success case, we don't unlink the temp file here as the child process needs it.
             * The child process or external program should clean it up. */
            return 0;
    }
}

/******************************************************************************
Description.: signal handler for SIGUSR1 - enable scanning
Input Value.: signal number
Return Value: -
******************************************************************************/
void signal_enable_scanning(int sig)
{
    scanning_enabled = 1;
}

/******************************************************************************
Description.: signal handler for SIGUSR2 - disable scanning
Input Value.: signal number
Return Value: -
******************************************************************************/
void signal_disable_scanning(int sig)
{
    scanning_enabled = 0;
}

/******************************************************************************
Description.: setup signal handlers for QR scanning control
Input Value.: -
Return Value: 0 if ok, -1 on error
******************************************************************************/
int setup_signal_handlers(void)
{
    struct sigaction sa_enable, sa_disable;

    /* Setup SIGUSR1 handler (enable scanning) */
    sa_enable.sa_handler = signal_enable_scanning;
    sigemptyset(&sa_enable.sa_mask);
    sa_enable.sa_flags = SA_RESTART;

    if(sigaction(SIGUSR1, &sa_enable, NULL) == -1) {
        OPRINT("ERROR: Failed to install SIGUSR1 handler: %s\n", strerror(errno));
        return -1;
    }

    /* Setup SIGUSR2 handler (disable scanning) */
    sa_disable.sa_handler = signal_disable_scanning;
    sigemptyset(&sa_disable.sa_mask);
    sa_disable.sa_flags = SA_RESTART;

    if(sigaction(SIGUSR2, &sa_disable, NULL) == -1) {
        OPRINT("ERROR: Failed to install SIGUSR2 handler: %s\n", strerror(errno));
        return -1;
    }

    OPRINT("Signal handlers installed: SIGUSR1=enable, SIGUSR2=disable scanning\n");
    return 0;
}
