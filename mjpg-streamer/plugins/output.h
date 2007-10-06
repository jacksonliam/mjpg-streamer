#ifndef OUTPUT_H
#define OUTPUT_H

#define OUTPUT_PLUGIN_PREFIX " o: "
#define OPRINT(...) fprintf(stderr, "%s", OUTPUT_PLUGIN_PREFIX); fprintf(stderr, __VA_ARGS__)

/* parameters for output plugin */
typedef struct {
  char *parameter_string;
  void *global;
} output_parameter;

/* commands which can be send to the input plugin */
typedef enum {
  OUT_CMD_UNKNOWN = 0,
  OUT_CMD_HELLO
} out_cmd_type;

/* structure to store variables/functions for output plugin */
typedef struct {
  char *plugin;
  void *handle;
  output_parameter param;

  int (*init)(output_parameter *);
  int (*stop)(void);
  int (*run)(void);
  int (*cmd)(out_cmd_type cmd);
} output;

int output_init(output_parameter *);
int output_stop(void);
int output_run(void);
int output_cmd(out_cmd_type cmd);

#endif
