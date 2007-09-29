#ifndef OUTPUT_H
#define OUTPUT_H

#define OUTPUT_PLUGIN_PREFIX " o: "
#define OPRINT(...) fprintf(stderr, "%s", OUTPUT_PLUGIN_PREFIX); fprintf(stderr, __VA_ARGS__)

/* parameters for output plugin */
typedef struct {
  char *parameter_string;
  void *global;
} output_parameter;

/* structure to store variables/functions for output plugin */
typedef struct {
  char *plugin;
  void *handle;
  output_parameter param;

  int (*init)(output_parameter *);
  int (*stop)(void);
  int (*run)(void);
} output;

#endif
