ECEN602 HW2 Programming Assignment
----------------------------------
Team Number: 30
Member 1 # Raghavan S V (UIN: 722009006)
Member 2 # Hari Kishan Srikonda (UIN: 924002529)
Member 3 # Jiaju Shi (UIN: 823001803)
---------------------------------------
Description/Comments:
--------------------
The package contains the implementation of the TFTP (Trivial File Transfer Protocol) server. Only READ requests can be issued to this TFTP server.

Initially, the socket structure for the TFTP server is set up and its parameters are initialized. After the bind function, the server is set up and ready to receive READ request from the tftp client.

The server essentially has the main following functions: 
  
  a. RECEIVE READ requests from the tftp client. Creates a new UDP port for DATA transfer to client
  
  b. SEND DATA packets (containing the file data) or ERROR packet (say, in case the file does not exist).

  c. Maintains a window of packets and has a 100ms timeout for each packet. 

  d. Receive ACK from the tftp client for each DATA packet (512 bytes) and advance the window. 

  e. In case of timeout for a particular packet, retransmit that particular packet alone.

  f. It also handles concurrent client transfers.


Unix command for starting server:
------------------------------------------
./server SERVER_IP SERVER_PORT 

