
#define IO_BUFFER 20
#define BOUNDARY "arflebarfle"
#define MAX_FRAME_SIZE (256*1024)

typedef enum { A_UNKNOWN, A_SNAPSHOT, A_STREAM, A_COMMAND, A_FILE } answer_t;

typedef struct {
  answer_t type;
  char *parameter;
  char *client;
  char *credentials;
} request;

typedef struct {
  int level;              /* how full the buffer is */
  char buffer[IO_BUFFER]; /* the data */
} iobuffer;

void *server_thread(void *arg);









