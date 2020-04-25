# CN-Project2-Deniz-Maria

This is an implementation of reliable data transfer between two UDP sockets using a stop-and-wait protocol. 

## Task I: Simplified TCP sender/receiver (Reliable data transfer)
Implement the following functionalities:
  • sending packets to the network based on a fixed sending window size (e.g. WND of 10 packets).
  • sending acknowledgments back from the receiver and handling what to do when re- ceiving ACKs at the sender.
  • a Timeout mechanism to deal with packet loss.
  
## Task II: TCP Congestion Control
Implement congestion control with the following features:
  1. slow start
  2. congestion avoidance
  3. fast retransmit (No fast recovery)
