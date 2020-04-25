#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"


/*
 * You ar required to change the implementation to support
 * window size greater than one.
 * In the currenlt implemenetation window size is one, hence we have
 * onlyt one send and receive packet
 */


/* add to cache and write from cache functions are taken 
from an online source  with some adjustments and added 
commenting */

node cache_head = NULL;		/* first node in cache */
int expected_seqno = 0;		/* expected packet sequence number */


/* creates deep copies of original data packets and stores them orderly 
in the cache by checking their sequence numbers and ordering them from smallest 
sequence number to largest into cur_node linked list */
void
add_to_cache (tcp_packet * orig_pkt)
{
  tcp_packet *pkt = make_packet (orig_pkt->hdr.data_size);	/* create new packet */
  memcpy (pkt->data, orig_pkt->data, orig_pkt->hdr.data_size);	/* copy data from original packet */
  pkt->hdr.data_size = orig_pkt->hdr.data_size;	/* copy data size */
  pkt->hdr.seqno = orig_pkt->hdr.seqno;	/* copy sequence number */
  pkt->hdr.ackno = orig_pkt->hdr.ackno;	/* copy ACK number */
  pkt->hdr.ctr_flags = orig_pkt->hdr.ctr_flags;	/* copy flags */

  node new_node = create_node (pkt);	/* create new node */

  if (!cache_head)		/* if cache head is null */
    {
      cache_head = new_node;	/* assign new node to cache head */
      return;
    }
/* if new packet sequence number is smaller than cache head sequence number */
  else if (new_node->pkt->hdr.seqno < cache_head->pkt->hdr.seqno)
    {
      new_node->next = cache_head;	/* store cache head in the next node of the linked list */
      cache_head = new_node;	/* assign new node to cache head */
      return;
    }
  node cur_node = cache_head;	/* assign value of cache head to current node */

/* while next current node is not NULL and the sequence number of the next */
/* current node is smaller than the sequence number of the new node */
  while (cur_node->next &&
	 cur_node->next->pkt->hdr.seqno < new_node->pkt->hdr.seqno)
    {
      cur_node = cur_node->next;	/* assign value of next node to current node */
      /* if current node sequence number is equal to new node sequence number break loop */
      if (cur_node->pkt->hdr.seqno == new_node->pkt->hdr.seqno)
	{
	  break;
	}
    }
  node next_node = cur_node->next;	/* assing value of next node to next_node */
  cur_node->next = new_node;	/* assign value of new node to next current node */
  new_node->next = next_node;	/* assign value of next node to next new node */
}


/* writes in order packets from cache into the file and then frees them */
node
write_from_cache (FILE * fp)
{
  node cur_node = cache_head;	/* assign cache head to current node */

  /* while current node is not NULL and current node is equal */
  /* to the expected in order sequence number */
  while (cur_node && cur_node->pkt->hdr.seqno == expected_seqno)
    {
      VLOG (DEBUG, "Writing packet: %d", cur_node->pkt->hdr.seqno);
      fseek (fp, cur_node->pkt->hdr.seqno, SEEK_SET);	/* go to sequence number */
      fwrite (cur_node->pkt->data, 1, cur_node->pkt->hdr.data_size, fp);	/* write to file */
      expected_seqno = cur_node->pkt->hdr.seqno + cur_node->pkt->hdr.data_size;	/* update expected sequence number (current pkt sequence number + current pkt data size); */
      node next_node = cur_node->next;
      free (cur_node);		/* free current node */
      cur_node = next_node;	/* assign value of next node to current node */
    }
  cache_head = cur_node;	/* assign value of current node to cache head */
  return cache_head;
}

int
main (int argc, char **argv)
{
  tcp_packet *recvpkt;
  tcp_packet *sndpkt;
  int sockfd;			/* socket */
  int portno;			/* port to listen on */
  int clientlen;		/* byte size of client's address */
  struct sockaddr_in serveraddr;	/* server's addr */
  struct sockaddr_in clientaddr;	/* client addr */
  FILE *fp;			/* file pointer to write to */
  char buffer[MSS_SIZE];
  struct timeval tp;


  /* 
   * check command line arguments 
   */
  if (argc != 3)
    {
      fprintf (stderr, "usage: %s <port> FILE_RECVD\n", argv[0]);
      exit (1);
    }
  portno = atoi (argv[1]);

  fp = fopen (argv[2], "w");
  if (fp == NULL)
    {
      error (argv[2]);
    }

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket (AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    error ("ERROR opening socket");


  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  int optval = 1;
  setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval,
	      sizeof (int));

  /*
   * build the server's Internet address
   */
  bzero ((char *) &serveraddr, sizeof (serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl (INADDR_ANY);
  serveraddr.sin_port = htons ((unsigned short) portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind (sockfd, (struct sockaddr *) &serveraddr, sizeof (serveraddr)) < 0)
    error ("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  VLOG (DEBUG, "epoch time, bytes received, sequence number");

  clientlen = sizeof (clientaddr);
  while (1)
    {
      /*
       * recvfrom: receive a UDP datagram from a client
       */
      VLOG (DEBUG, "waiting from server \n");
      if (recvfrom
	  (sockfd, buffer, MSS_SIZE, 0, (struct sockaddr *) &clientaddr,
	   (socklen_t *) & clientlen) < 0)
	{
	  error ("ERROR in recvfrom");
	}

      recvpkt = (tcp_packet *) buffer;
      assert (get_data_size (recvpkt) <= DATA_SIZE);
      if (recvpkt->hdr.data_size == 0)
	{
	  VLOG (INFO, "End Of File has been reached");
	  fclose (fp);
	  break;
	}

      /* 
       * sendto: ACK back to the client 
       */
      gettimeofday (&tp, NULL);
      VLOG (INFO, "%lu.%06lu, %d, %d", tp.tv_sec, tp.tv_usec,
	    recvpkt->hdr.data_size, recvpkt->hdr.seqno);

      /* if sequence number is greater than expected sequence packet value in cache */
      if (recvpkt->hdr.seqno > expected_seqno)
	{
	  add_to_cache (recvpkt);
	}
      /* if sequence number equals expected sequence store packet in cache then write 
         from the cache into the file */
      else if (recvpkt->hdr.seqno == expected_seqno)
	{
	  add_to_cache (recvpkt);
	  write_from_cache (fp);
	}

      /* acknowledge recieved packet */
      sndpkt = make_packet (0);
      sndpkt->hdr.ackno = expected_seqno;
      sndpkt->hdr.ctr_flags = ACK;
      if (sendto
	  (sockfd, sndpkt, TCP_HDR_SIZE, 0, (struct sockaddr *) &clientaddr,
	   clientlen) < 0)
	{
	  error ("ERROR in sendto");
	}
    }

  return 0;
}
