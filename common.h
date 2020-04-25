#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include "packet.h"

extern int verbose;


#define NONE    0x0
#define INFO    0x1 
#define WARNING 0x10
#define INFO    0x1 
#define DEBUG   0x100
#define ALL     0x111

struct linked_list {
  tcp_packet *pkt;
  struct linked_list *next;
};

typedef struct linked_list *node;

node create_node(tcp_packet *pkt);

#define VLOG(level, ...)            \
  if (level & verbose) {            \
    fprintf(stderr, ##__VA_ARGS__); \
    fprintf(stderr, "\n");          \
  }

void error(char *msg);
#endif
