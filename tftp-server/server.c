/*
** TFTP server implementation 
*/
#include <sys/select.h>
#include <sys/time.h>
#include <stdio.h>
#include <time.h>
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
#include <pthread.h>

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

#define RETRIES 50 /* 5 sec to terminate and 100 ms ACK timeout (5000/100 = 50) */ 
#define ACK_TIMEOUT 100000 /* 100 ms = 100000 usec */
#define MAXDATASIZE 512

char cwd[100];

struct read_args {
	char file[100];
	struct sockaddr_in c;
	char mode[20];
};
// Send file 
void tftp_sendfile (void * send_arguments) {
	
	struct read_args *args = send_arguments;
	struct sockaddr_in client = args->c;
	int sock, len, client_len, opcode, ssize = 0, n, j;
	struct timeval curr_time, time_now;
	unsigned short int count = 0, rcount = 0, resend =0;
	char filebuf[MAXDATASIZE], sendbuf[MAXDATASIZE + 4], recvbuf[MAXDATASIZE], filename[100], mode[10], file_path[150], *index, *ptr;
	struct sockaddr_in ack;

	// Obtaining Current directory
	if (getcwd(cwd, sizeof(cwd)) != NULL)
        	printf("server: Current working directory: %s\n", cwd);
        else
        	perror("server: getcwd() error");
	
	// pointer to the file we will be sending
	FILE *fp;                     	
	strcpy (filename, args->file); 
	strcpy (mode, args->mode);        
	
	// Opening a new socket sending the file
	if ((sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)   
    	{
		printf ("server: Error in creating socket for sending file \n");
	      	return;
	}
	
	// Error handling for mode
	if (strncasecmp (mode, "octet", 5))    
	{
		if (strncasecmp (mode, "mail", 4) || strncasecmp (mode, "netascii", 8)) {
	  		printf ("server: %s mode not supported \n ",mode);
			len = sprintf ((char *) sendbuf, "%c%c%c%c(%s) mode not supported.%c", 0x00, ERR, 0x00, error_code[0], mode, 0x00);		     
		}
	        else {
                        printf ("server: %s mode unrecognized \n ",mode);
                        len = sprintf ((char *) sendbuf, "%c%c%c%c(%s) mode unrecognized.%c", 0x00, ERR, 0x00, error_code[0], mode, 0x00);                    
		}
		
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
	
	while (1)   
    	{
		ssize = fread (filebuf, 1, MAXDATASIZE, fp);
		count++; 
		count = htons(count); 
		ptr = (char *) &count;
		sprintf ((char *) sendbuf, "%c%c", 0x00, DATA);  
		memcpy ((char *) sendbuf + 2, ptr, 2); 
		memcpy ((char *) sendbuf + 4, filebuf, ssize);
		len = 4 + ssize;
		count = ntohs(count);
		printf ("server: Sending packet # %d (length: %d file chunk: %d)\n", count, len, ssize);

		// Sending the data packet
		if (sendto (sock, sendbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len) {
			printf("server: data packet not sent correctly\n");
			return;
		}
			
		// Loop to recieve/timeout ACKs			
		for(j=0; j < RETRIES; j++) {
			client_len = sizeof (ack);
			resend = 0;		
			gettimeofday(&time_now, NULL);
			do {
				if((n = recvfrom (sock, recvbuf, sizeof (recvbuf), MSG_DONTWAIT, (struct sockaddr *) &ack, (socklen_t *) & client_len)) > 0)
					break;
				// Continue to check current time to ensure ACKs don't timeout
				gettimeofday(&curr_time, NULL);

			}while (((curr_time.tv_sec - time_now.tv_sec)*1000000 + curr_time.tv_usec - time_now.tv_usec) < ACK_TIMEOUT);

			if (n < 0 )
				resend = 1;

			else {
				// Maybe someone connected to us
	  			if (client.sin_addr.s_addr != ack.sin_addr.s_addr) {
					printf ("server: Error recieving ACK (ACK from invalid address)\n");
					j--;      
					continue;
	    			}
				// Decode received message correctly
				index = (char *) recvbuf;  
				index++; // moving past first null byte
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

			if(resend == 1) {
				// resending data packet
				if (sendto (sock, sendbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len) {
					printf ("server: Error resending data packet\n");
					return;
	    			}
	    			printf ("server: No ACK received. Retry: %d. Packet: #%d \n",j+1,count);
				continue;					
			}		 
		}    
		// Obtain current time
		if (j == RETRIES) {
			printf ("server: ACK Timeout. Client did not respond for 5 seconds!! Aborting transfer\n");	
			fclose (fp);
			close(sock);
			return;
		}

	      	if (feof(fp))
			break;

		memset (filebuf, 0, sizeof (filebuf));    
    	}
  	fclose (fp);
	close (sock);
	printf ("server: File sent successfully\n");
	return;
}



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
    int sockfd, rv;  
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in client_addr; // connector's address information
    socklen_t client_size;
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
    fd_set read_fds;

    // clear the temp sets
    FD_ZERO(&read_fds);

    // add the socket descriptor to the master set
    FD_SET(sockfd, &read_fds);

    printf("TFTP server started....\n");

    while(1) 
    {  
	// main accept() loop
	if(select(sockfd+1,&read_fds,NULL,NULL,NULL) == -1) {
	        printf("Error with select \n");
        	perror("select");
	        exit(1);
    	}
	
	if(FD_ISSET(sockfd, &read_fds)) {
		//Accept the new connection
		client_size = sizeof client_addr;
		/*clear the buffer */
		memset (buf, 0, sizeof buf); 
		 
		if((numbytes = recvfrom (sockfd, buf, sizeof buf, MSG_DONTWAIT, (struct sockaddr *) &client_addr, (socklen_t *) & client_size)) < 0)
		{
			perror("server: Could not receive from client");
			continue;
		}
		else {
			printf("server: got connection from %s \n",inet_ntoa (client_addr.sin_addr));	
			index = buf;          
			index++; // moving past the first NULL packet
			opcode = *index++;   

			if (opcode == RRQ )   // RRQ requests
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
    				printf ("server: READ REQUEST\n");
				struct read_args send_args;
				strcpy(send_args.file,filename);
			        send_args.c = client_addr;
			        strcpy(send_args.mode, mode);
				pthread_t client_t;
			        pthread_create(&client_t, NULL, (void*(*)(void*)) tftp_sendfile, (void*)&send_args);      
				
		        }			
			else
		        {
				// not read/write request
			        printf ("server: Invalid opcode: %x size: %d detected\n", opcode, numbytes);      
			}
		}
	}
   }
   return 0;
}
