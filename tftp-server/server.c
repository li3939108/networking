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
#define ACK_TIMEOUT 0.1 /* 100 ms */
#define MAXDATASIZE 512

char cwd[100];

// Send file 
void tftp_sendfile (char *file_name, struct sockaddr_in client, char *transfer_mode, int tid)
{
	int sock, len, client_len, res, opcode, ssize = 0, n, j;
	struct timeval tv;
	unsigned short int count = 0, rcount = 0, resend =0;
	time_t curr_time, time_now;
	char filebuf[MAXDATASIZE], sendbuf[MAXDATASIZE + 4], recvbuf[MAXDATASIZE];
	char filename[100], mode[10], file_path[150], *index, *ptr;
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
	
	memset (filebuf, 0, sizeof (filebuf));
	// FD_SET variables for select() 
	fd_set read_fds;
	// clear the temp sets
	FD_ZERO(&read_fds);
	// add the socket descriptor to the master set
	FD_SET(sock, &read_fds);

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

		// 5 seconds timeout
		tv.tv_sec = 5; 
		res = select(sock+1, &read_fds, NULL, NULL, &tv);	
		// Loop to recieve/timeout ACKs			
		for(j=0; j < RETRIES; j++) {
			client_len = sizeof (ack);
			resend = 0;		
			if(res == 0) {
			        printf("server: TIMED OUT Client closed the connection.............. \n");
			        close(sock);
			        return;
			}

			else if (res < 0) {
			        printf("server: Error with select \n");
				close(sock);
			        return;
			}
			// Obtain current time
			time_now = time(NULL);
			do {
				if((n = recvfrom (sock, recvbuf, sizeof (recvbuf), MSG_DONTWAIT, (struct sockaddr *) &ack, (socklen_t *) & client_len)) > 0)
					break;
				// Continue to check current time to ensure ACKs don't timeout
				curr_time = time(NULL);

			}while (difftime(curr_time,time_now)<ACK_TIMEOUT);

			if (n < 0 ) {
				printf ("server: The server could not receive from the client (n: %d)\n", n);
				resend = 1;
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
					// Send error packet
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

			if(resend == 1) {
				// resending data packet
				if (sendto (sock, sendbuf, len, 0, (struct sockaddr *) &client, sizeof (client)) != len) {
					printf ("server: Error resending data packet\n");
					return;
	    			}
	    			printf ("server: Ack(s) lost. Resending: %d\n", count);
				continue;					
			}		 
		}        
		if (j == RETRIES) {
			printf ("server: Ack Timeout. Aborting transfer\n");
			fclose (fp);
			close(sock);
			return;
		}

	      	if (ssize != MAXDATASIZE)
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
			printf("server: got connection from %s , port: %d \n",inet_ntoa (client_addr.sin_addr), ntohs (client_addr.sin_port));	
			index = buf;          
			if (index++[0] != 0x00)
		        {       //first TFTP packet byte needs to be null. Once the value is taken increment it.
			        printf ("server: Errorenous tftp packet.\n");
			        return 0;
		        }
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
			        	tftp_sendfile (filename, client_addr, mode, ntohs (client_addr.sin_port));      
  				break;
			        case 2:
					printf ("server: WRITE REQUEST - Currently not supported\n");
				break;
				default:
					printf ("server: Invalid opcode detected !\n");
				break;
			}				
		}
	}
   }
   return 0;
}
