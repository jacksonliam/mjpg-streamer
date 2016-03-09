#ifndef INPUT_OPENCV_H_
#define INPUT_OPENCV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "../../mjpg_streamer.h"
#include "../../utils.h"

int input_init(input_parameter* param, int id);
int input_stop(int id);
int input_run(int id);
int input_cmd(int plugin, unsigned int control_id, unsigned int typecode, int value);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_OPENCV_H_ */
