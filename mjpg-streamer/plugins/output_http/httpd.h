
#include "../input.h"
#include "../output.h"

#define IO_BUFFER 256
#define BOUNDARY "arflebarfle"
#define MAX_FRAME_SIZE (256*1024)

/* only the following filestypes are supported */
static const struct {
  const char *dot_extension;
  const char *mimetype;
} mimetypes[] = {
  { ".html", "text/html" },
  { ".htm",  "text/html" },
  { ".css",  "text/css" },
  { ".js",   "text/javascript" },
  { ".txt",  "text/plain" },
  { ".jpg",  "image/jpeg" },
  { ".jpeg", "image/jpeg" },
  { ".png",  "image/png"},
  { ".ico",  "image/x-icon" },
  { ".swf",  "application/x-shockwave-flash" },
  { ".cab",  "application/x-shockwave-flash" },
  { ".jar",  "application/java-archive" }
};

/* mapping between command string and command type */
static const struct {
  const char *string;
  const in_cmd_type cmd;
} in_cmd_mapping[] = {
  { "hello_input", IN_CMD_HELLO },
  { "reset", IN_CMD_RESET },
  { "pan_plus", IN_CMD_PAN_PLUS },
  { "pan_minus", IN_CMD_PAN_MINUS },
  { "tilt_plus", IN_CMD_TILT_PLUS },
  { "tile_minus", IN_CMD_TILT_MINUS },
  { "saturation_plus", IN_CMD_SATURATION_PLUS },
  { "saturation_minus", IN_CMD_SATURATION_MINUS },
  { "contrast_plus", IN_CMD_CONTRAST_PLUS },
  { "contrast_minus", IN_CMD_CONTRAST_MINUS },
  { "brightness_plus", IN_CMD_BRIGHTNESS_PLUS },
  { "brightness_minus", IN_CMD_BRIGHTNESS_MINUS },
  { "gain_plus", IN_CMD_GAIN_PLUS },
  { "gain_minus", IN_CMD_GAIN_MINUS }
};

/* mapping between command string and command type */
static const struct {
  const char *string;
  const out_cmd_type cmd;
} out_cmd_mapping[] = {
  { "hello_output", OUT_CMD_HELLO }
};

typedef enum { A_UNKNOWN, A_SNAPSHOT, A_STREAM, A_COMMAND, A_FILE } answer_t;

typedef struct {
  answer_t type;
  char *parameter;
  char *client;
  char *credentials;
} request;

typedef struct {
  int level;              /* how full is the buffer */
  char buffer[IO_BUFFER]; /* the data */
} iobuffer;

void *server_thread(void *arg);









