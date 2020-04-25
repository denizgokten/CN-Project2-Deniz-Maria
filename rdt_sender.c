#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

#define RETRY 500		// milliseconds

int window_size = 5;		
int sockfd;			
struct sockaddr_in serveraddr;
int serverlen;
node sndpkts_head = NULL;
node sndpkts_tail = NULL;
struct itimerval timer;
sigset_t sigmask;


int free_pkts (int last_byte_acked)
{
  int packets_freed = 0;
  node cur = sndpkts_head;
  node next_node;
  while (1)
    {
      if (cur && cur->pkt->hdr.seqno < last_byte_acked &&
	  cur->pkt->hdr.data_size)
	{
	  next_node = cur->next;
	  free (cur->pkt);
	  free (cur);
	  packets_freed++;
	  cur = next_node;
	}
      else
	{
	  break;
	}
    }
  sndpkts_head = cur;
  if (!sndpkts_head)
    {
      sndpkts_tail = sndpkts_head;
    }
  return packets_freed;
}

void
resend_packets (int sig)
{
  if (sig == SIGALRM)
    {
      //Resend all packets range between 
      //sendBase and nextSeqNum
      VLOG (INFO, "Timout happend");
      if (sendto (sockfd, sndpkts_head->pkt,
		  TCP_HDR_SIZE + get_data_size (sndpkts_head->pkt), 0,
		  (const struct sockaddr *) &serveraddr, serverlen) < 0)
	{
	  error ("sendto");
	}
    }
}

void
start_timer ()
{
  VLOG (DEBUG, "Start timer");
  sigprocmask (SIG_UNBLOCK, &sigmask, NULL);
  setitimer (ITIMER_REAL, &timer, NULL);
}

void
stop_timer ()
{
  sigprocmask (SIG_BLOCK, &sigmask, NULL);
}

/*
 * init_timer: Initialize timeer
 * delay: delay in milli seconds
 * sig_handler: signal handler function for resending unacknoledge packets
 */
void
init_timer (int delay, void (*sig_handler) (int))
{
  signal (SIGALRM, resend_packets);
  timer.it_interval.tv_sec = delay / 1000;	// sets an interval of the timer
  timer.it_interval.tv_usec = (delay % 1000) * 1000;
  timer.it_value.tv_sec = delay / 1000;	// sets an initial value
  timer.it_value.tv_usec = (delay % 1000) * 1000;

  sigemptyset (&sigmask);
  sigaddset (&sigmask, SIGALRM);
}

int
main (int argc, char **argv)
{
  int portno;
  char *hostname;
  char buffer[DATA_SIZE];
  FILE *fp;

  /* check command line arguments */
  if (argc != 4)
    {
      fprintf (stderr, "usage: %s <hostname> <port> <FILE>\n", argv[0]);
      exit (EXIT_FAILURE);
    }
  hostname = argv[1];
  portno = atoi (argv[2]);
  fp = fopen (argv[3], "r");
  if (fp == NULL)
    {
      error (argv[3]);
    }


  /* socket: create the socket */
  sockfd = socket (AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    error ("ERROR opening socket");

  /* initialize server server details */
  bzero ((char *) &serveraddr, sizeof (serveraddr));
  serverlen = sizeof (serveraddr);

  /* covert host into network byte order */
  if (inet_aton (hostname, &serveraddr.sin_addr) == 0)
    {
      fprintf (stderr, "ERROR, invalid host %s\n", hostname);
      exit (EXIT_FAILURE);
    }

  /* build the server's Internet address */
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons (portno);

  // Stop and wait protocol
  init_timer (RETRY, resend_packets);
  int next_seqno = 0;
  int send_base = 0;
  int last_byte_acked = 0;
  int num_pkts_sent = 0;
  int done = 0;

  while (1)
    {

      VLOG (DEBUG, "Packets in flight: %d, CWND: %d", num_pkts_sent,
	    window_size);

      /* Send packets within window size */
      while (!done && num_pkts_sent < window_size)
	{
	  int len = fread (buffer, 1, DATA_SIZE, fp);

	  if (len <= 0)
	    {
	      VLOG (INFO, "End Of File has been reached");
	      sndpkts_tail->next = create_node (make_packet (0)); /* keep sent packets in sndpkts buffer */
	      sndpkts_tail = sndpkts_tail->next;
	      done = 1;
	      num_pkts_sent++;
	      break;
	    }
	  send_base = next_seqno;
	  next_seqno = send_base + len;

	  VLOG (DEBUG, "Making packet %d", send_base);
	  tcp_packet *pkt = make_packet (len);
	  memcpy (pkt->data, buffer, len);
	  pkt->hdr.data_size = len;
	  pkt->hdr.seqno = send_base;
	  pkt->hdr.ackno = send_base + len;
	  pkt->hdr.ctr_flags = DATA;
	  if (!sndpkts_head)
	    {
	      sndpkts_head = create_node (pkt);
	      sndpkts_tail = sndpkts_head;
	    }
	  else
	    {
	      sndpkts_tail->next = create_node (pkt);
	      sndpkts_tail = sndpkts_tail->next;
	    }

	  VLOG (INFO, "Sending packet %d to %s", send_base,
		inet_ntoa (serveraddr.sin_addr));

	  /*
	   * If the sendto is called for the first time, the system will
	   * will assign a random port number so that server can send its
	   * response to the src port.
	   */
	  if (sendto (sockfd, sndpkts_tail->pkt,
		      TCP_HDR_SIZE + get_data_size (sndpkts_tail->pkt), 0,
		      (const struct sockaddr *) &serveraddr, serverlen) < 0)
	    {
	      error ("sendto");
	    }
	  num_pkts_sent++;

	  if (num_pkts_sent == 1)
	    {
	      start_timer ();
	    }
	}

      if (recvfrom
	  (sockfd, buffer, MSS_SIZE, 0, (struct sockaddr *) &serveraddr,
	   (socklen_t *) & serverlen) < 0)
	{
	  error ("recvfrom");
	}
      tcp_packet *recvpkt;
      recvpkt = (tcp_packet *) buffer;
      VLOG (DEBUG, "Recieved ACK: %d, Last inorder ACK: %d\n",
	    recvpkt->hdr.ackno, last_byte_acked);
      assert (get_data_size (recvpkt) <= DATA_SIZE);

      if (recvpkt->hdr.ackno > last_byte_acked)
	{
	  last_byte_acked = recvpkt->hdr.ackno;
	  stop_timer ();
	  int packets_freed = free_pkts (last_byte_acked);
	  num_pkts_sent -= packets_freed;

	  start_timer ();

	  /* send last packet */
	  if (done)
	    {
	      if (num_pkts_sent == 1)
		{
		  if (sendto (sockfd, sndpkts_head->pkt,
			      TCP_HDR_SIZE +
			      get_data_size (sndpkts_head->pkt), 0,
			      (const struct sockaddr *) &serveraddr,
			      serverlen) < 0)
		    {
		      error ("sendto");
		    }
		  exit (EXIT_SUCCESS);
		}
	    }
	}
    }

  return 0;
}



