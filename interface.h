#define MAX_CONNS 58
#define MAX_INBUF 96
#define MAX_LINE_PER_SEC 10

#ifndef __INTERFACE
#include <sys/types.h>
#include <netinet/in.h>
#endif /* __INTERFACE */

struct conn {
  int fd;
  int is_free;
  struct sockaddr_in address;
  char *inbuf[MAX_INBUF];
  int inbuf_count;
  char *partial;
  int status;
  int flags;
  time_t last_input_time;
  int input_this_sec;
};

typedef struct conn *CONN;

int init_port(int portno, int bufsiz);
void shutdown_port(void);
void shutdown_conn(CONN cd);
#define write_conn(DESCRIPTOR,STRING) \
  write(DESCRIPTOR->fd,STRING,strlen(STRING))
char *read_conn(CONN cd);
CONN get_event(void);
int connctl(CONN cd, int oper, int data);
#define conn_status(DESCRIPTOR) \
  (DESCRIPTOR->status)
CONN get_conn(int refno);
#define get_inet_addr(X) \
  (X->address)
#define get_fd(X) \
  (X->fd) 

/* possible statuses for conn_status to return */
#define INPUT_RECEIVED 0
#define NEW_CONNECTION 1
#define CONNECTION_DEAD 2

/* possible operations with connctl */
#define CONN_SET_FLAGS 1
#define CONN_GET_FLAGS 2

/* possible flags that can be set on the CONN with connctl */
#define SPAM_OK 1


