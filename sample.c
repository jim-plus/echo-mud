#include <stdio.h>
#include "interface.h"

extern void *malloc();
extern char *inet_ntoa();

struct cdlist {
  CONN cd;
  struct cdlist *next;
};

void notify_all(struct cdlist *cds, char *msg)
{
  while (cds) {
    write_conn(cds->cd,msg);
    write_conn(cds->cd,"\n");
    cds=cds->next;
  }
}

main() {
  char ourbuf[512],*theirbuf;
  CONN curr_cd;
  struct cdlist *cds,*tmp,*prev;

  cds=NULL;
  if (!init_port(3939,512)) {
    fprintf(stderr,"Failed to open port 3939\n");
    exit(-1);
  }
  while (1) {
    curr_cd=get_event();
    switch (conn_status(curr_cd)) {
    case INPUT_RECEIVED:
      theirbuf=read_conn(curr_cd);
      if (!strcmp(theirbuf,"WHO")) {
	tmp=cds;
	sprintf(ourbuf,"FD\tAddress\n");
	write_conn(curr_cd,ourbuf);
	while (tmp) {
	  sprintf(ourbuf,"%d\t%s\n",
		  (int) get_fd(tmp->cd),inet_ntoa(get_inet_addr(tmp->cd).
						  sin_addr));
	  write_conn(curr_cd,ourbuf);
	  tmp=tmp->next;
	}
	break;
      } else if (!strcmp(theirbuf,"QUIT")) {
	write_conn(curr_cd,"Ciao babe.\n");
	if (cds->cd==curr_cd) {
          tmp=cds;
	  cds=cds->next;
	  free(tmp);
	} else {
	  prev=cds;
	  tmp=prev->next;
	  while (tmp) {
	    if (tmp->cd==curr_cd) {
	      prev->next=tmp->next;
	      free(tmp);
	      break;
	    }
	    prev=tmp;
	    tmp=tmp->next;
	  }
	}
	shutdown_conn(curr_cd);
	break;
      } else if (!strcmp(theirbuf,"@shutdown")) {
	notify_all(cds,"Shutting down, babes.\n");
	shutdown_port();
	exit(0);
      } else
        notify_all(cds,theirbuf);
      break;
    case NEW_CONNECTION:
      tmp=malloc(sizeof(struct cdlist));
      tmp->cd=curr_cd;
      tmp->next=cds;
      cds=tmp;
      write_conn(curr_cd,"Howdy!\n");
      break;
    case CONNECTION_DEAD:
      if (cds->cd==curr_cd) {
	tmp=cds->next;
	shutdown_conn(curr_cd);
	free(cds);
	cds=tmp;
      } else {
	prev=cds;
	tmp=cds->next;
	while (tmp) {
	  if (tmp->cd==curr_cd) {
	    prev->next=tmp->next;
	    shutdown_conn(curr_cd);
	    free(tmp);
	    break;
	  }
	  prev=tmp;
	  tmp=tmp->next;
	}
      }
      break;
    }
  }
}
