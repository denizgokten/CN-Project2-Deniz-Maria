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

#define RETRY 100           // milliseconds

int window_size = 1;	   // window size	
int ssthresh = 64;         // slow start threshhold
int duplicate = 0;         // record duplicate ACKs
float rtt_increase = 0.0;  // increase 1/CWND 
int connection = 0;        // connection has been established
int tripleACK;             // store ACK no of last triple ACked packet 
int restart = 0;           // loss occured
int sockfd;			
struct sockaddr_in serveraddr;
int serverlen;
node sndpkts_head = NULL; // points to first node
node sndpkts_tail = NULL; // points to last node 
struct itimerval timer;
sigset_t sigmask;
struct timeval gettime;   // get current time 

// slide window forward when getting an ACK for higher packet number 
int free_pkts (int last_byte_acked)
{
   // VLOG (INFO, "free packets");
  int packets_freed = 0;   // number of packets freed 
  node cur = sndpkts_head; // pointer to head 
  node next_node;          // pointer to next node 
  
  while (1)
    {
      // if current node sequence number is smaller than last bye ACKed
      if (cur && cur->pkt->hdr.seqno < last_byte_acked &&
	  cur->pkt->hdr.data_size)
	{
	  next_node = cur->next; // assign next cur node to next_node
	  free (cur->pkt); // deallocate data memory
	  free (cur);      // deallocate node memory 
	  packets_freed++; // increment number of packets freed 
	  cur = next_node; // assign the next node that we have stored to  current node 
	}
	   // otherwise break loop
      else
	{
	  break;
	}
    }
  sndpkts_head = cur; // assign current node to head 
  if (!sndpkts_head)  // if head is null 
    {
      sndpkts_tail = sndpkts_head; // assign value of head to tail
    }
  return packets_freed; // return number of packets freed 
}

void
resend_packets (int sig)
{
  if ((sig == SIGALRM) || (sig == 3)) // if timeout or dup ACKs 
    {
      gettimeofday(&gettime, NULL);
      //Resend all packets range between sendBase and nextSeqNum
      if (sig == SIGALRM) { 
      	VLOG (INFO, "Timout happend");} 
      else {
        VLOG (INFO, "Triple ACK");}

      VLOG(INFO, "Re-sending packet %d", sndpkts_head->pkt->hdr.ackno); 

      if (sendto (sockfd, sndpkts_head->pkt,
		  TCP_HDR_SIZE + get_data_size (sndpkts_head->pkt), 0,
		  (const struct sockaddr *) &serveraddr, serverlen) < 0)
	{
	  error ("sendto");
	}
       
       ssthresh = max(window_size/2, 2); // set ssthresh value half of window size
       // printf("ssthresh after retransmit:%d\n",ssthresh);

       // if window size is not equal to 1
       if (window_size != 1) {
      	 window_size = 1;  // reset window size to 1 
         restart = 1;
          // VLOG (INFO, "reset window size");
       }
    }
}

void
start_timer ()
{
  // VLOG (INFO, "start timer");
  sigprocmask (SIG_UNBLOCK, &sigmask, NULL);
  setitimer (ITIMER_REAL, &timer, NULL);
}

void
stop_timer ()
{
  // VLOG (INFO, "stop timer");
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
  FILE *fp;   // file to read data from 
  FILE *cwnd; // file showing window size over time

  cwnd = fopen("cwnd.csv", "w"); // open file to record window size 

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
  init_timer (RETRY, resend_packets); // resends packet if timeout 
  // VLOG (INFO, "out of resend");
  int next_seqno = 0;      // sequence number of next packet 
  int send_base = 0;       // next packet to be sent 
  int last_byte_acked = 0; // largest inorder ACK
  int num_pkts_sent = 0;   // number of packets sent 
  int done = 0;            // done reading file 

  while (1)
    {
      VLOG (DEBUG, "Packets: %d CWND:  %d Ssthresh: %d", num_pkts_sent, window_size, ssthresh);

      /* Send packets within window size */
      while (!done && num_pkts_sent < window_size)
	{
	  int len = fread (buffer, 1, DATA_SIZE, fp); // read data from fp and store in buffer 
          // VLOG (INFO, "len is %d", len);

	  if (len <= 0)
	    {
	      VLOG (INFO, "End Of File has been reached");
	      sndpkts_tail->next = create_node (make_packet (0)); // create node packet 0 length 
	      sndpkts_tail = sndpkts_tail->next; // assign to tail 
	      done = 1;        // done reading file 
	      num_pkts_sent++; // increment number of packets sent 
	      break;
	    }

	  send_base = next_seqno;       // send packet with next sequence number 
	  next_seqno = send_base + len; // assign next packet to next_sequno 

	  VLOG (DEBUG, "Making packet %d", send_base);
	  tcp_packet *pkt = make_packet (len); // make packet
	  memcpy (pkt->data, buffer, len);     // copy packet data from buffer to pkt->data 
	  pkt->hdr.data_size = len;            // assign len to packet data size 
	  pkt->hdr.seqno = send_base;          // assign sequence number to packet seqno
	  pkt->hdr.ackno = send_base + len;    // assign next packet sequence number to packet ackno 
	  pkt->hdr.ctr_flags = DATA;           // set flag to DATA 

	  if (!sndpkts_head) // if head is null (empty list)
	    {
	      sndpkts_head = create_node (pkt); // create node and assign to head  
	      sndpkts_tail = sndpkts_head;      // assign head to tail
	    }
	  else
	    {
	      sndpkts_tail->next = create_node (pkt); // create packet node and store it in next tail node
	      sndpkts_tail = sndpkts_tail->next; // assign sndpkts_tail->next to sndpkts_tail
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
      // VLOG (DEBUG, "ACK: %d, Last inorder: %d\n", recvpkt->hdr.ackno, last_byte_acked);
      
      // program starts with ssthresh = 64 
      if (connection == 0 ) {
          connection = 1;
          ssthresh = 64;
          fprintf(cwnd, "%lu : %d\n", (gettime.tv_sec*1000 + (gettime.tv_usec/1000)), window_size);
      }

      assert (get_data_size (recvpkt) <= DATA_SIZE);

      if (recvpkt->hdr.ackno > last_byte_acked) // recieved ACK is larger than last byte ACKed
	{
          duplicate = 0;  // reset duplicate ACKs count 
	  last_byte_acked = recvpkt->hdr.ackno; // update last byte ACKed  
	  stop_timer ();                        // stop timer 
	  int packets_freed = free_pkts (last_byte_acked); // free ACKed packet (slide window)
	  num_pkts_sent -= packets_freed;       // subtract packets freed prom packets sent
     
          if (restart == 1){
          fprintf(cwnd, "%lu : %d\n", (gettime.tv_sec*1000 + (gettime.tv_usec/1000)), window_size);
              restart = 0;
          } 

          // slow start 
          if ( window_size < ssthresh ) { // if window size smaller than ssthresh 
	  	          window_size += 1;         // increase window size by one 
                gettimeofday(&gettime, NULL);
                fprintf(cwnd, "%lu : %d\n", (gettime.tv_sec*1000 + (gettime.tv_usec/1000)), window_size);
          }

          // congestion avoidance 
          else { 
                rtt_increase += ((1.0)/window_size); // increase window by 1/cwnd 
                // printf("rtt increase %2.2f\n", rtt_increase);

                if (rtt_increase >= 1) { // if value sums up to a whole 
                   window_size = window_size + (int)rtt_increase; // add value to window size 
                   rtt_increase -= 1.0; // decrement rrt_increase
                   gettimeofday(&gettime, NULL); 
                  fprintf(cwnd, "%lu : %d\n", (gettime.tv_sec*1000 + (gettime.tv_usec/1000)), window_size);
                }
          }
          

          start_timer (); // start timer 

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
      fclose(cwnd);  // close file 
		  exit (EXIT_SUCCESS); // exit 
		}
	    }
	}
       // if recieved ACK is equal to last byte ACKed 
       else if (recvpkt->hdr.ackno == last_byte_acked) {
          duplicate += 1;       // increment duplicate ACKs count 

          // if 3 duplicates ACKs are recieved for a packet the first time 
          if ((duplicate == 3) && (recvpkt->hdr.ackno != tripleACK)){  
              // VLOG (INFO, "Packet loss: 3 duplicate acks received.");
              tripleACK = recvpkt->hdr.ackno;  // store packet ACK number 
              resend_packets(duplicate);       // resend packet 
              duplicate = 0;                   // reset dulplicate count 
          }
       }
    }

  return 0;
}
