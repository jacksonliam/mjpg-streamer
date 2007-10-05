
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









