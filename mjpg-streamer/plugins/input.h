#ifndef INPUT_H
#define INPUT_H

#define INPUT_PLUGIN_PREFIX " i: "
#define IPRINT(...) fprintf(stderr, "%s", INPUT_PLUGIN_PREFIX); fprintf(stderr, __VA_ARGS__)

/* parameters for input plugin */
typedef struct {
  char *parameter_string;
  void *global;
} input_parameter;

/* structure to store variables/functions for input plugin */
typedef struct {
  char *plugin;
  void *handle;
  input_parameter param;

  int (*init)(input_parameter *);
  int (*stop)(void);
  int (*run)(void);
} input;

#endif
