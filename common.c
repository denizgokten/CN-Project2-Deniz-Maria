#include <stdio.h>
#include <stdlib.h>

#include "common.h"


int verbose = ALL;


/* create empty an empty linked list to use as buffer for storing packets */
node create_node(tcp_packet *pkt) {
  node temp = (node)malloc(sizeof(struct linked_list)); // allocate memory for new node
  temp->pkt = pkt;   // insert pkt into new node 
  temp->next = NULL; // assign NULL to next node
  return temp; // return pointer to new node 
}

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
}
