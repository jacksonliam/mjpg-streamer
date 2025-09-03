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

#ifndef OUTPUT_QRSCANNER_H
#define OUTPUT_QRSCANNER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <jpeglib.h>

#include <quirc.h>

#include "../../mjpg_streamer.h"
#include "../../utils.h"

#ifdef __cplusplus
extern "C" {
#endif

// Plugin function prototypes
int output_init(output_parameter *param, int id);
int output_stop(int id);
int output_run(int id);
int output_cmd(int plugin_id, unsigned int control_id, unsigned int group, int value, char *value_str);

// Function prototypes
void *worker_thread(void *arg);
void worker_cleanup(void *arg);
int process_frame(unsigned char *frame_data, int frame_size);
int decode_qr_codes_quirc(unsigned char *gray_data, int width, int height);
int execute_external_program(const char *program_path, const char *qr_data, int qr_data_len, const char *qr_error);
void cleanup_child_processes(void);

// Signal handling functions
void signal_enable_scanning(int sig);
void signal_disable_scanning(int sig);
int setup_signal_handlers(void);

// JPEG decoding functions
int decode_jpeg_to_gray(unsigned char *jpeg_data, int jpeg_size,
                       unsigned char **gray_data, int *width, int *height);

// Helper functions
void help(void);

#ifdef __cplusplus
}
#endif

#endif /* OUTPUT_QRSCANNER_H */
