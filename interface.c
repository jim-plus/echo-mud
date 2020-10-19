#define __INTERFACE

#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "interface.h"

#define difftime(x,y) (x-y)
#define NUM_FM 16

static int ibufsiz;
static int sock_fd;
static int fd_lim;
static char *internal_buf;
static struct conn connlist[MAX_CONNS];
static struct {
    int avail;
    char *buffer;
  } fmbuf[NUM_FM];

static char *fastmalloc(int size)
{
  int loop=0;

  if (size>ibufsiz)
    return (char *) malloc(size);
  while (loop<NUM_FM) {
    if (fmbuf[loop].avail) {
      fmbuf[loop].avail=0;
      return fmbuf[loop].buffer;
    }
    loop++;
  }
  return (char *) malloc(size);
}

static void fastfree(char *buf)
{
  int loop=0;

  while (loop<NUM_FM) {
    if (fmbuf[loop].buffer==buf) {
      fmbuf[loop].avail=1;
      return;
    }
    loop++;
  }
  free(buf);
}

CONN get_conn(int refno)
{
  if (refno<0 || refno>=MAX_CONNS) return NULL;
  return (connlist[refno].is_free) ? NULL : &(connlist[refno]);
}

int connctl(CONN cd, int oper, int data)
{
  switch (oper) {
  case CONN_SET_FLAGS:
    cd->flags=data;
    return 1;
  case CONN_GET_FLAGS:
    return cd->flags;
  default:
    return -1;
  }
}

int init_port(int portno, int bufsiz)
{
  int opt=1;
  struct sockaddr_in server;
  int loop;

  sock_fd=socket(AF_INET,SOCK_STREAM,0);
  if (sock_fd<0) return 0;
  server.sin_family=AF_INET;
  server.sin_addr.s_addr=INADDR_ANY;
  server.sin_port=htons(portno);
  if (bind(sock_fd,(struct sockaddr *) &server, sizeof(server)))
    return 0;
  listen(sock_fd,5);
  fd_lim=sock_fd+1;
  ibufsiz=bufsiz;
  for (opt=0;opt<MAX_CONNS;opt++) {
    connlist[opt].is_free=1;
    connlist[opt].partial=(char *) malloc(ibufsiz);
    *(connlist[opt].partial)='\0';
  }
  internal_buf=(char *) malloc(ibufsiz);
  while (loop<NUM_FM) {
    fmbuf[loop].avail=1;
    fmbuf[loop].buffer=(char *) malloc(ibufsiz);
    loop++;
  }
  return 1;
}

void shutdown_conn(CONN cd)
{
  if (cd->is_free) return;
  cd->is_free=1;
  while (cd->inbuf_count) free(cd->inbuf[(cd->inbuf_count)--]);
  shutdown(cd->fd,2);
  close(cd->fd);
  *(cd->partial)='\0';
}

void shutdown_port(void)
{
  int loop;

  for (loop=0;loop<MAX_CONNS;loop++) {
    shutdown_conn(&(connlist[loop]));
    free(connlist[loop].partial);
  }
  close(sock_fd);
  free(internal_buf);
  for (loop=0;loop<NUM_FM;loop++) free(fmbuf[loop].buffer);
}

char *read_conn(CONN cd)
{
  int loop=0;

  if (!(cd->inbuf_count)) return NULL;
  strcpy(internal_buf,cd->inbuf[0]);
  fastfree(cd->inbuf[0]);
  while (loop<cd->inbuf_count) cd->inbuf[loop]=cd->inbuf[++loop];
  --(cd->inbuf_count);
  return internal_buf;
}

CONN make_new_conn(void)
{
  int new_fd,new_cd=(-1),size=sizeof(struct sockaddr_in),loop=0;
  struct sockaddr_in tmpaddr;

  loop=sizeof(struct sockaddr);
  new_fd=accept(sock_fd,(struct sockaddr *) &tmpaddr,
		&size);
  if (new_fd<0) return NULL;
  while (loop<MAX_CONNS)
    if (connlist[loop++].is_free) {
      new_cd=loop-1;
      break;
    }
  if (new_cd==-1) {
    shutdown(new_fd,2);
    close(new_fd);
    return NULL;
  }
  if (new_fd>=fd_lim) fd_lim=new_fd+1;
  connlist[new_cd].fd=new_fd;
  connlist[new_cd].is_free=0;
  connlist[new_cd].address=tmpaddr;
  connlist[new_cd].inbuf_count=0;
  connlist[new_cd].flags=0;
  connlist[new_cd].last_input_time=time(NULL);
  connlist[new_cd].input_this_sec=0;
  return &(connlist[new_cd]);
}

static int add_line(CONN cd,char *buf,int pos,int end,time_t now)
{
  int count=0;
  char *tmp;

  if ((difftime(now,cd->last_input_time)<=1) &&
      ((++(cd->input_this_sec))>MAX_LINE_PER_SEC) &&
      (!(cd->flags & SPAM_OK)))
    return 0;
  if (cd->inbuf_count==MAX_INBUF) return 1;
  if (difftime(now,cd->last_input_time)>1)
    cd->input_this_sec=0;
  cd->last_input_time=now;
  tmp=(char *) fastmalloc(end-pos+1);
  while (pos<end) {
    if (isgraph(buf[pos]) || (buf[pos]==' ')) tmp[count++]=buf[pos];
    pos++;
  }
  tmp[count]='\0';
  cd->inbuf[(cd->inbuf_count)++]=tmp;
  return 1;
}
  

static int process_input(CONN cd)
{
  int len,pos=0,start=0,partial_len;
  time_t now;

  partial_len=strlen(cd->partial);
  if (partial_len>=ibufsiz-1)
    partial_len=0;
  strcpy(internal_buf,cd->partial);
  *(cd->partial)='\0';
  len=read(cd->fd,internal_buf+partial_len,ibufsiz-1-partial_len);
  if (len<=0) return 0;
  len+=partial_len;
  now=time(NULL);
  while (pos<len) {
    if (internal_buf[pos]=='\n') {
      if (!add_line(cd,internal_buf,start,pos,now)) return 0;
      start=pos+1;
    }
    pos++;
  }
  if (pos-start) {
    strncpy(cd->partial,internal_buf+start,pos-start);    
    *(cd->partial+pos-start+1)='\0';
  }
  return 1;
}
    

CONN get_event(void)
{
  fd_set input_set;
  int loop=0;
  CONN new_conn;

  while (1) {
    loop=0;
    while (loop<MAX_CONNS) {
      if ((!(connlist[loop].is_free)) && (connlist[loop].inbuf_count)) {
        connlist[loop].status=INPUT_RECEIVED;
        return &(connlist[loop]);
      }
      loop++;
    }
    loop=0;
    FD_ZERO(&input_set);
    while (loop<MAX_CONNS) {
      if (!(connlist[loop].is_free)) {
        FD_SET(connlist[loop].fd,&input_set);
      }
      loop++;
    }
    FD_SET(sock_fd,&input_set);
    select(fd_lim,&input_set,NULL,NULL,NULL);
    if (FD_ISSET(sock_fd,&input_set)) {
      new_conn=make_new_conn();    
      if (new_conn) {
        new_conn->status=NEW_CONNECTION;
        return new_conn;
      }
    }
    loop=0;
    while (loop<MAX_CONNS) {
      if ((!(connlist[loop].is_free)) && (FD_ISSET(connlist[loop].fd,
						   &input_set)))
        if (!process_input(&(connlist[loop]))) {
          connlist[loop].status=CONNECTION_DEAD;
          return &(connlist[loop]);
	} else {
          connlist[loop].status=INPUT_RECEIVED;
	}
      loop++;
    }
  }
}
