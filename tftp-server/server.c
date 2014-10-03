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

// Defining Error Codes 

char error_code []={	0 /*Not defined, see error message (if any)"*/,
		     	1 /*File not found*/,
		  	2 /*Access violation*/,
			3 /*Disk full or allocation exceeded*/,
			4 /*Illegal TFTP operation*/,
			5 /*Unknown transfer ID*/,
			6 /*File already exists*/,
			7 /*No such user */ };

#define TIMEOUT 2000 /*amount of time to wait for an ACK/Data Packet in 1000microseconds 1000 = 1 second*/
#define RETRIES 3 /* Number of times to resend a data OR ack packet beforing giving up */


#define MAXDATASIZE 512

char cwd[100];

// Function prototypes 
void tftp_sendfile (char *file_name, struct sockaddr_in client, char *transfer_mode, int tid)
{
	int sock, len, client_len, opcode, ssize = 0, n, i, j;
	unsigned short int count = 0, rcount = 0, bcount =0;
	char filebuf[MAXDATASIZE + 1];
	char sendbuf[MAXDATASIZE + 12],recvbuf[MAXDATASIZE + 12];
	char filename[128], mode[12], file_path[196], *index, *ptr;
	struct sockaddr_in ack;

	// Obtaining Current directory
	if (getcwd(cwd, sizeof(cwd)) != NULL)
        	printf("server: Current working directory: %s\n", cwd);
        else
        	perror("server: getcwd() error");
	
	// pointer to the file we will be sending
	FILE *fp;                     	
	strcpy (filename, file_name); 
	strcpy (mode, transfer_mode);        
	
	// Opening a new socket sending the file
	if ((sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)   
    	{
		printf ("server: Error in reconnect for sending file \n");
	      	return;
	}
	
	// Error handling for mode
	if (strncasecmp (mode, "octet", 5))    
	{
		if (strncasecmp (mode, "mail", 4) || strncasecmp (mode, "netascii", 8)) {
	  		printf ("server: %s mode not supported \n ",mode);
			len = sprintf ((char *) sendbuf, "%c%c%c%c(%s) mode not supported.%c", 0x00, ERR, 0x00, error_code[0], mode, 0x00);		     }
	        else {
                        printf ("server: %s mode unrecognized \n ",mode);
                        len = sprintf ((char *) sendbuf, "%c%c%c%c(%s) mode unrecognized.%c", 0x00, ERR, 0x00, error_code[0], mode, 0x00);                    }
		
		//Sending Error PCKT to client
		if (sendto (sock, sendbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len)     
        		printf("server: send mode error packet not sent correctly\n");
	        
		return;
    	}

	//Appending the filename to the path
	sprintf(file_path,"%s/",cwd);
	strncat (file_path, filename, sizeof (file_path) - 1);  
	fp = fopen (file_path, "r");
	if (fp == NULL) {              
        	printf ("server: file not found (%s)\n", file_path);
	        len = sprintf ((char *) sendbuf, "%c%c%c%cFile not found (%s)%c", 0x00, ERR, 0x00, error_code[1], file_path, 0x00);
		//Sending Error PCKT to client
                if (sendto (sock, sendbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len)
                	printf("server: file not found mode error packet not sent correctly\n");
		
                return;
	}
  	
	else {
      	        printf ("server: Sending file... (source: %s)\n", file_path);
    	}
	
	memset (filebuf, 0, sizeof (filebuf));
	while (1)   
    	{
	        ssize = fread (filebuf, 1, MAXDATASIZE, fp);
		count++; 
		count = htons(count); 
		ptr = &count;
		sprintf ((char *) sendbuf, "%c%c", 0x00, DATA);  
	        memcpy ((char *) sendbuf + 2, ptr, 2); 
		memcpy ((char *) sendbuf + 4, filebuf, ssize);
	        len = 4 + ssize;
		count = ntohs(count);
        	printf ("server: Sending packet # %04d (length: %d file chunk: %d)\n", count, len, ssize);

		// Sending the data packet
		if (sendto (sock, sendbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len) {
			printf("server: data packet not sent correctly\n");
		        return;
	        }

		if ((count - 1) >= 0 || ssize != MAXDATASIZE) {
        		// Loop to recieve/timeout ACKs
			for (j = 0; j < RETRIES; j++) {
				client_len = sizeof (ack);
              			n = -1;
              			for (i = 0; i <= TIMEOUT && n < 0; i++) {
			                n = recvfrom (sock, recvbuf, sizeof (recvbuf), MSG_DONTWAIT, (struct sockaddr *) &ack, (socklen_t *) & client_len);
                		}
			        if (n < 0 ) {
                			printf ("server: The server could not receive from the client (n: %d)\n", n);
			                //resend packet
                		}	
               			else {
					// Maybe someone connected to us
	          			if (client.sin_addr.s_addr != ack.sin_addr.s_addr) {
			                	printf ("server: Error recieving ACK (ACK from invalid address)\n");
			                        j--;      
			                        continue;
                    			}
		      			// Obtain the right port
		                        if (tid != ntohs (client.sin_port)) {
                        			printf ("Error recieving file (data from invalid tid)\n");
			                        len = sprintf ((char *) recvbuf, "%c%c%c%cBad/Unknown TID%c", 0x00, ERR, 0x00, error_code[5], 0x00);
						// Send erro packet
			                        if (sendto (sock, recvbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len) 
							printf ("server: Port error packet not sent correctly\n");
                        		
                      				j--;
			                        continue; 
                    			}
					// Decode received message correctly
			                index = (char *) recvbuf;  
			                if (index++[0] != 0x00)
			                	printf ("server: Bad first nullbyte!\n");
			
			                opcode = *index++;
			                rcount = *index++ << 8;
 		                        rcount &= 0xff00;
			                rcount |= (*index++ & 0x00ff);
					// Checking the opcode and the block number of the ACK for the sent packet
		                        if (opcode != ACK || rcount != count) {
                        			printf ("server: Remote host failed to ACK proper data packet # %d (got OP: %d Block: %d)\n", count, opcode, rcount);
						// Send Error
			                        if (opcode > ERR) {						
				                        len = sprintf ((char *) recvbuf, "%c%c%c%cIllegal operation%c", 0x00, ERR, 0x00, error_code[4], 0x00);
							if (sendto (sock, recvbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len)      
			   	                        	printf ("server: Error in ACK packet not sent correctly\n");
                                                }
                                        }
                  			else {
                                        	printf ("server: ACK successfully received for block (#%d)\n", rcount);
                      				break;
                    			}
                		}
				
				for (i = 0; i < bcount; i++) {
					// resending data packet
					if (sendto (sock, sendbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len) {
						printf ("server: Error resending data packet\n");
			                        return;
                    			}
                    			printf ("server: Ack(s) lost. Resending: %d\n", count - bcount + i);
                		}
              	                //printf ("server: Ack(s) lost. Resending complete.\n");

            		}
        	}
		
		//n = recvfrom (sock, recvbuf, sizeof (recvbuf), MSG_DONTWAIT, (struct sockaddr *) &ack, (socklen_t *) & client_len);   
        
		if (j == RETRIES) {
        		printf ("server: Ack Timeout. Aborting transfer\n");
		        fclose (fp);
		        return;
	        }

	      	if (ssize != MAXDATASIZE)
        		break;
      		memset (filebuf, 0, sizeof (filebuf));    
    	}

  	fclose (fp);
	printf ("server: File sent successfully\n");
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
            					printf ("server: READ REQUEST\n");
				        	tftp_sendfile (filename, client_addr, mode, tid);
				        
          				break;
				        case 2:
						printf ("server: WRITE REQUEST\n");
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
