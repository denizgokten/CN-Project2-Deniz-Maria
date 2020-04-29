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
  tcp_packet *pkt;          // tcp_packet data field 
  struct linked_list *next; // pointer to next node 
};

typedef struct linked_list *node; // define linked list node

node create_node(tcp_packet *pkt); // create node and return pointer to it 

#define VLOG(level, ...)            \
  if (level & verbose) {            \
    fprintf(stderr, ##__VA_ARGS__); \
    fprintf(stderr, "\n");          \
  }

void error(char *msg);
#endif
