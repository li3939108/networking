/*
** TFTP server implementation 
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define BACKLOG 10     // how many pending connections queue will hold

// Defining TFTP frame Opcodes

#define RRQ 	0x01
#define WRQ 	0x02
#define DATA 	0x03
#define ACK 	0x04
#define ERR 	0x05

#define TIMEOUT 2000 /*amount of time to wait for an ACK/Data Packet in 1000microseconds 1000 = 1 second*/
#define RETRIES 3 /* Number of times to resend a data OR ack packet beforing giving up */
#define MAXACKFREQ 16 /* Maximum number of packets before ack */

#define MAXDATASIZE 512

// Function prototypes 
void tftp_sendfile (char *, struct sockaddr_in, char *, int) {

void
tsend (char *pFilename, struct sockaddr_in client, char *pMode, int tid)
{
  int sock, len, client_len, opcode, ssize = 0, n, i, j, bcount = 0;
  unsigned short int count = 0, rcount = 0, acked = 0;
  unsigned char filebuf[MAXDATASIZE + 1];
  unsigned char packetbuf[MAXACKFREQ][MAXDATASIZE + 12],
    recvbuf[MAXDATASIZE + 12];
  char filename[128], mode[12], fullpath[196], *bufindex;
  struct sockaddr_in ack;

  FILE *fp;                     /* pointer to the file we will be sending */

  strcpy (filename, pFilename); //copy the pointer to the filename into a real array
  strcpy (mode, pMode);         //same as above

  if (debug)
    printf ("branched to file send function\n");


  if ((sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)   //startup a socket
    {
      printf ("Server reconnect for sending did not work correctly\n");
      return;
    }
  if (!strncasecmp (mode, "octet", 5) && !strncasecmp (mode, "netascii", 8))    /* these two are the only modes we accept */
    {
      if (!strncasecmp (mode, "mail", 4))
        len = sprintf ((char *) packetbuf[0],
                       "%c%c%c%cThis tftp server will not operate as a mail relay%c",
                       0x00, 0x05, 0x00, 0x04, 0x00);
      else
        len = sprintf ((char *) packetbuf[0],
                       "%c%c%c%cUnrecognized mode (%s)%c",
                       0x00, 0x05, 0x00, 0x04, mode, 0x00);
      if (sendto (sock, packetbuf[0], len, 0, (struct sockaddr *) &client, sizeof (client)) != len)     /* send the data packet */
        {
          printf
            ("Mismatch in number of sent bytes while trying to send mode error packet\n");
        }
      return;
    }
  if (strchr (filename, 0x5C) || strchr (filename, 0x2F))       //look for illegal characters in the filename string these are \ and /
    {
      if (debug)
      printf ("Server requested bad file: forbidden name\n");
      len =
        sprintf ((char *) packetbuf[0],
                 "%c%c%c%cIllegal filename.(%s) You may not attempt to descend or ascend directories.%c",
                 0x00, 0x05, 0x00, 0x00, filename, 0x00);
      if (sendto (sock, packetbuf[0], len, 0, (struct sockaddr *) &client, sizeof (client)) != len)     /* send the data packet */
        {
          printf
            ("Mismatch in number of sent bytes while trying to send error packet\n");
        }
      return;

    }
  strcpy (fullpath, path);
  strncat (fullpath, filename, sizeof (fullpath) - 1);  //build the full file path by appending filename to path
  fp = fopen (fullpath, "r");
  if (fp == NULL)
    {                           //if the pointer is null then the file can't be opened - Bad perms OR no such file
      if (debug)
        printf ("Server requested bad file: file not found (%s)\n", fullpath);
      len =
        sprintf ((char *) packetbuf[0], "%c%c%c%cFile not found (%s)%c", 0x00,
                 0x05, 0x00, 0x01, fullpath, 0x00);
      if (sendto (sock, packetbuf[0], len, 0, (struct sockaddr *) &client, sizeof (client)) != len)     /* send the data packet */
        {
          printf
            ("Mismatch in number of sent bytes while trying to send error packet\n");
        }
      return;
    }
  else
    {
      if (debug)
        printf ("Sending file... (source: %s)\n", fullpath);

    }
  memset (filebuf, 0, sizeof (filebuf));
  while (1)                     /* our break statement will escape us when we are done */
    {
      acked = 0;
      ssize = fread (filebuf, 1, datasize, fp);
 count++;                  /* count number of datasize byte portions we read from the file */
      if (count == 1)           /* we always look for an ack on the FIRST packet */
        bcount = 0;
      else if (count == 2)      /* The second packet will always start our counter at zreo. This special case needs to exist to avoid a DBZ when count = 2 - 2 = 0 */
        bcount = 0;
      else
        bcount = (count - 2) % ackfreq;

      sprintf ((char *) packetbuf[bcount], "%c%c%c%c", 0x00, 0x03, 0x00, 0x00); /* build data packet but write out the count as zero */
      memcpy ((char *) packetbuf[bcount] + 4, filebuf, ssize);
      len = 4 + ssize;
      packetbuf[bcount][2] = (count & 0xFF00) >> 8;     //fill in the count (top number first)
      packetbuf[bcount][3] = (count & 0x00FF);  //fill in the lower part of the count
      if (debug)
        printf ("Sending packet # %04d (length: %d file chunk: %d)\n",
                count, len, ssize);

      if (sendto (sock, packetbuf[bcount], len, 0, (struct sockaddr *) &client, sizeof (client)) != len)        /* send the data packet */
        {
          if (debug)
            printf ("Mismatch in number of sent bytes\n");
          return;
        }


      if ((count - 1) == 0 || ((count - 1) % ackfreq) == 0
          || ssize != datasize)
        {
/* The following 'for' loop is used to recieve/timeout ACKs */
          for (j = 0; j < RETRIES; j++)
            {
              client_len = sizeof (ack);
              errno = EAGAIN;
              n = -1;
              for (i = 0; errno == EAGAIN && i <= TIMEOUT && n < 0; i++)
                {
                  n =
                    recvfrom (sock, recvbuf, sizeof (recvbuf), MSG_DONTWAIT,
                              (struct sockaddr *) &ack,
                              (socklen_t *) & client_len);

                  usleep (1000);
                }
              if (n < 0 && errno != EAGAIN)
                {

                {
                  if (debug)
                    printf
                      ("The server could not receive from the client (errno: %d n: %d)\n",
                       errno, n);
                  //resend packet
                }
              else if (n < 0 && errno == EAGAIN)
                {
                  if (debug)
                    printf ("Timeout waiting for ack (errno: %d n: %d)\n",
                            errno, n);
                  //resend packet

                }
              else
                {

                  if (client.sin_addr.s_addr != ack.sin_addr.s_addr)    /* checks to ensure send to ip is same from ACK IP */
                    {
                      if (debug)
                        printf
                          ("Error recieving ACK (ACK from invalid address)\n");
                      j--;      /* in this case someone else connected to our port. Ignore this fact and retry getting the ack */
                      continue;
                    }
                  if (tid != ntohs (client.sin_port))   /* checks to ensure get from the correct TID */
                    {
                      if (debug)
                        printf
                          ("Error recieving file (data from invalid tid)\n");
                      len =
                        sprintf ((char *) recvbuf,
                                 "%c%c%c%cBad/Unknown TID%c", 0x00, 0x05,
                                 0x00, 0x05, 0x00);
                      if (sendto (sock, recvbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len)  /* send the data packet */
                        {
                          printf
                            ("Mismatch in number of sent bytes while trying to send mode error packet\n");
                        }
                      j--;

                      continue; /* we aren't going to let another connection spoil our first connection */
                    }

/* this formatting code is just like the code in the main function */
                  bufindex = (char *) recvbuf;  //start our pointer going
                  if (bufindex++[0] != 0x00)
                    printf ("bad first nullbyte!\n");
                  opcode = *bufindex++;

                  rcount = *bufindex++ << 8;
                  rcount &= 0xff00;
                  rcount += (*bufindex++ & 0x00ff);
                  if (opcode != 4 || rcount != count)   /* ack packet should have code 4 (ack) and should be acking the packet we just sent */
                    {
                      if (debug)
                        printf
                          ("Remote host failed to ACK proper data packet # %d (got OP: %d Block: %d)\n",
                           count, opcode, rcount);
/* sending error message */
                      if (opcode > 5)
                        {
                          len = sprintf ((char *) recvbuf,
                                         "%c%c%c%cIllegal operation%c",
                                         0x00, 0x05, 0x00, 0x04, 0x00);
                          if (sendto (sock, recvbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len)      /* send the data packet */
                            {
                              printf
                                ("Mismatch in number of sent bytes while trying to send mode error packet\n");
                            }
                        }
                      /* from here we will loop back and resend */
                    }
                  else
                    {
                      if (debug)
                        printf ("Remote host successfully ACK'd (#%d)\n",
                                rcount);
                      break;
                    }
                }
              for (i = 0; i <= bcount; i++)
                {
                  if (sendto (sock, packetbuf[i], len, 0, (struct sockaddr *) &client, sizeof (client)) != len) /* resend the data packet */
                    {
                      if (debug)
                        printf ("Mismatch in number of sent bytes\n");
                      return;
                    }
                  if (debug)
                    printf ("Ack(s) lost. Resending: %d\n",
                            count - bcount + i);
                }
              if (debug)
                printf ("Ack(s) lost. Resending complete.\n");

            }
/* The ack sending 'for' loop ends here */

        }
      else if (debug)
        {
          printf ("Not attempting to recieve ack. Not required. count: %d\n",
                  count);
          n = recvfrom (sock, recvbuf, sizeof (recvbuf), MSG_DONTWAIT, (struct sockaddr *) &ack, (socklen_t *) & client_len);   /* just do a quick check incase the remote host is trying with ackfreq = 1 */

        }

      if (j == RETRIES)
        {
          if (debug)
            printf ("Ack Timeout. Aborting transfer\n");
          fclose (fp);

          return;
        }
      if (ssize != datasize)
        break;

      memset (filebuf, 0, sizeof (filebuf));    /* fill the filebuf with zeros so that when the fread fills it, it is a null terminated string */
    }

  fclose (fp);
  if (debug)
    printf ("File sent successfully\n");

  return;
}



//void tftp_getfile (char *, struct sockaddr_in, char *, int);

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int sockfd, sockmax, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in client_addr; // connector's address information
    socklen_t client_size;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv,i,j,tid;
    int numbytes,opcode;
    char *index,buf[MAXDATASIZE],mode[12],filename[256];
 
    //Checking specification of command line options
    if (argc != 3) {
        fprintf(stderr,"usage: server server_ip server_port \n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM; //Datagram packet for UDP sender
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure

    // FD_SET variables for select() 
    fd_set master,read_fds;

    // clear the master and temp sets
    FD_ZERO(&master);    
    FD_ZERO(&read_fds);

    // add the socket descriptor to the master set
    FD_SET(sockfd, &master);

    // keep track of the largest socket descriptor to use with select()
    sockmax = sockfd; // so far, it's this one

    printf("TFTP server started....\n");

    while(1) 
    {  // main accept() loop
	read_fds = master;
	if(select(sockmax+1,&read_fds,NULL,NULL,NULL) == -1) {
	        printf("Error with select \n");
        	perror("select");
	        exit(1);
    	}

	//Looping through all incoming socket connections 
	//for(i=0; i<=sockmax; i++) {
	i=sockfd;
	if(FD_ISSET(i, &read_fds)) {
		if(i == sockfd) {
			//Accept the new connection
			client_size = sizeof client_addr;
			/*clear the buffer */
			memset (buf, 0, sizeof buf); 
			 
			if((numbytes = recvfrom (sockfd, buf, sizeof buf, MSG_DONTWAIT, (struct sockaddr *) &client_addr, (socklen_t *) & client_size)) < 0)
			{
				perror("Server could not receive from client");
				continue;
			}
			else {
				//printf("server: Adding to master %d\n",new_fd);
				//FD_SET(new_fd, &master); //Adding to master set
				//if(new_fd > sockmax) {
				//	sockmax = new_fd;
				//}
				printf("server: got connection from %s , port: %d \n",inet_ntoa (client_addr.sin_addr), ntohs (client_addr.sin_port));	

				index = buf;          
				if (index++[0] != 0x00)
			        {       //first TFTP packet byte needs to be null. Once the value is taken increment it.
				        printf ("Malformed tftp packet.\n");
				        return 0;
			        }
			        tid = ntohs (client_addr.sin_port);    
				opcode = *index++;   

				if (opcode == RRQ || opcode == WRQ)   // RRQ/WRQ requests
			        {
					// obtaining filename from the TFTP frame
					strncpy (filename, index, sizeof (filename) - 1);  
					// Moving the index further and parsing 1 null byte after filename
				        index += strlen (filename) + 1;    
					// obtaining mode of TFTP file transfer (Octet/Netascii)
				        strncpy (mode, index, sizeof (mode) - 1);  
					// Moving the index further 
				        index += strlen (mode) + 1; 
					// Print decoded values from the TFTP frame       					
					printf ("opcode: %x filename: %s packet size: %d mode: %s\n", opcode, filename, numbytes, mode);   
			        }			
				else
			        {
					// not read/write request
				        printf ("opcode: %x size: %d \n", opcode, numbytes);      
        			}
			  
				switch (opcode)           
			        {
				        case 1:
            					printf ("READ REQUEST\n");
				        //        tftp_sendfile (filename, client_addr, mode, tid);
				        
          				break;
				        case 2:
						printf ("WRITE REQUEST\n");
				         //       tftp_getfile (filename, client_addr, mode, tid);
					break;
					default:
						printf ("Invalid opcode detected !\n");
					break;
				}				
			}
       		}
	}
   }
   return 0;
}
			
/*		else {
		       	// handle data from a client
		       	if ((numbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
		        // got error or connection closed by client
		        	printf("server: %s LEFT the chat room \n",usrns[i-sockfd-1]);
		                if (numbytes == 0) {
				        // connection closed
				       	printf("server: socket %d hung up! Nothing received\n", i);
		                } 
				else {
					printf("server: some error receiving \n");
			        	perror("recv");
		                }
					//Sending Offline Message to everyone
		                        send_to_everyone(i, sockfd, sockmax, 6, master, usrns);

					free(usrns[i-sockfd-1]);
					usrns[i-sockfd-1]=NULL;
		                	close(i); // bye!
		                	FD_CLR(i, &master); // remove from master set
		            	} 
				else {
				        // we got some data from a client
					buf[numbytes] = '\0';
					uint16_t *vrs_ty = (uint16_t *)&buf;
					// JOIN request
					if(((ntohs(*vrs_ty))&0x7F) == 2)
					{
						present=1;
						char reason[50];
						//Checking number of clients in room
						for(j=0 ; j < atoi(argv[3]) ;j++) {
							if(usrns[j]==NULL){
								present=0;
								break;
							}
						}

						if(present) {
					                printf("server: MAX CLIENTS of %d reached. Sorry Try again later!\n",atoi(argv[3]));
							strcpy(reason,"MAX CLIENTS reached. Sorry Try again later!"); 
					   	}
				        	else {
						        for(j=0 ; j < atoi(argv[3]) ;j++) {
						                if(usrns[j]!=NULL && strncmp(&buf[8],usrns[j],numbytes-8)==0) {
						                        printf("server: USERNAME(%s) already present!. Try again\n",usrns[j]);
									present=1;
									strcpy(reason,"USERNAME already exists. Try another one!");
									break;
								}
							}
							// No problems and client can join
							if(!present) {				
								usrns[i-sockfd-1]=(char*) malloc(numbytes-8);
								strncpy(usrns[i-sockfd-1],&buf[8],numbytes-8);
								printf("server: %s JOINED the chat room\n",usrns[i-sockfd-1]);	
								//ACK send 
								send_ack (i, usrns, atoi(argv[3]));	
								//Sending Online Message to everyone
								send_to_everyone(i, sockfd, sockmax, 8, master, usrns);
							}
						}
						//NAK send
						if(present) {
						    struct SBCP msg;
				                    msg.vrsn_type = (3<<7)|(5);
				                    msg.at[0].attrib_type = 1;
				                    strcpy(msg.at[0].payload,reason); //Reason you it can't join
				                    msg.at[0].attrib_len = strlen(msg.at[0].payload)+4;
				                    msg.frame_len = msg.at[0].attrib_len + 4;

						    htons_struct(&msg);
						    //Sending NAK reason
				                    if (send(i, (char *)&msg, ntohs(msg.frame_len), 0) == -1){
						            printf("Error sending\n");
						            perror("send");
				                    }
		   				    close(i); // bye!
				                    FD_CLR(i, &master); // remove from master set
						}				
				
					}
					// Chat Message request	
					else if(((ntohs(*vrs_ty))&0x7F) == 4) {
						printf("%s: %s \n",usrns[i-sockfd-1],&buf[8]);

						for(j = 0; j <= sockmax; j++) {
						    	// send to everyone!
							if (FD_ISSET(j, &master)) {
								// except the listener and ourselves
								if ((j > sockfd) && (j != i)) {
								    struct SBCP msg;
								    msg.vrsn_type = (3<<7)|(3);
								    msg.at[0].attrib_type = 2;
								    strcpy(msg.at[0].payload,usrns[i-sockfd-1]); //username initially to join
								    msg.at[0].attrib_len = strlen(msg.at[0].payload)+4;
								    msg.frame_len = msg.at[0].attrib_len + 4;

								    msg.at[1].attrib_type = 4;
								    strcpy(msg.at[1].payload,&buf[8]);	
								    msg.at[1].attrib_len = strlen(msg.at[1].payload)+4;
								    msg.frame_len += msg.at[1].attrib_len ;
				
								    htons_struct(&msg);
	
								    //Sending Chat text and username (FWD)
								    if (send(j, (char *)&msg, sizeof(msg), 0) == -1){
									    printf("Error sending\n");
									    perror("send");
							    	    }
				    				}
							}
						}
					}
				}
			}
		}
	}
    }	
    return 0;
}*/
