#ifndef INPUT_H
#define INPUT_H

#define INPUT_PLUGIN_PREFIX " i: "
#define IPRINT(...) fprintf(stderr, "%s", INPUT_PLUGIN_PREFIX); fprintf(stderr, __VA_ARGS__)

/* parameters for input plugin */
typedef struct {
  char *parameter_string;
  void *global;
} input_parameter;

/* commands which can be send to the input plugin */
typedef enum {
  IN_CMD_UNKNOWN = 0,
  IN_CMD_HELLO,
  IN_CMD_RESET,
  IN_CMD_PAN_PLUS,
  IN_CMD_PAN_MINUS,
  IN_CMD_TILT_PLUS,
  IN_CMD_TILT_MINUS,
  IN_CMD_SATURATION_PLUS,
  IN_CMD_SATURATION_MINUS,
  IN_CMD_CONTRAST_PLUS,
  IN_CMD_CONTRAST_MINUS,
  IN_CMD_BRIGHTNESS_PLUS,
  IN_CMD_BRIGHTNESS_MINUS,
  IN_CMD_GAIN_PLUS,
  IN_CMD_GAIN_MINUS
} in_cmd_type;

/* structure to store variables/functions for input plugin */
typedef struct {
  char *plugin;
  void *handle;
  input_parameter param;

  int (*init)(input_parameter *);
  int (*stop)(void);
  int (*run)(void);
  int (*cmd)(in_cmd_type cmd);
} input;

#endif
