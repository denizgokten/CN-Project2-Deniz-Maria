#include <stdio.h>
#include <stdlib.h>

#include "common.h"


int verbose = ALL;


/* empty node for buffer */
node create_node(tcp_packet *pkt) {
  node temp = (node)malloc(sizeof(struct linked_list));
  temp->pkt = pkt;
  temp->next = NULL;
  return temp;
}

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
}

