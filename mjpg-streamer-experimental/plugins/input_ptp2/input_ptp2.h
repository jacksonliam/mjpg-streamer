#ifndef INPUT_PTP2_H_
#define INPUT_PTP2_H_

#include "../../mjpg_streamer.h"

int input_init(input_parameter* param, int id);
int input_stop(int id);
int input_run(int id);
int input_cmd(int plugin, unsigned int control_id, unsigned int typecode, int value);

void help();
int camera_set(char* name, void* value);
void* capture(void* arg);
void cleanup(void *arg);

#endif /* INPUT_PTP2_H_ */
