/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define MAXDATASIZE 512 // max number of bytes we can get at once 
#define MAXATTRIBUTES 2 //max number of attributes that will be sent in our implementation

//Frame(message) contents of the SBCP protocol

struct Attr{
uint16_t attrib_type,attrib_len;
char payload[MAXDATASIZE];
};

struct SBCP{
uint16_t vrsn_type, frame_len;
struct Attr at[MAXATTRIBUTES];
};

//General htons for all variables in struct
void htons_struct(struct SBCP *m)
{
	int k;
	m->vrsn_type = htons (m->vrsn_type);
	for(k=0;k<MAXATTRIBUTES;k++) {
		m->at[k].attrib_type = htons (m->at[k].attrib_type);
	        m->at[k].attrib_len = htons (m->at[k].attrib_len);
	}
        m->frame_len = htons (m->frame_len);
}

//General ntohs for  all struct variables
void ntohs_struct(struct SBCP *m)
{
        int k;
        m->vrsn_type = ntohs (m->vrsn_type);
        for(k=0;k<MAXATTRIBUTES;k++) {
                m->at[k].attrib_type = ntohs (m->at[k].attrib_type);
                m->at[k].attrib_len = ntohs (m->at[k].attrib_len);
        }
        m->frame_len = ntohs (m->frame_len);
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
    int sockfd, numbytes;  
    char buf[MAXDATASIZE * MAXATTRIBUTES];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    //Checking specification of command line options
    if (argc != 4) {
        fprintf(stderr,"usage: client username server_ip server_port\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    //Obtaining address of server to connect
    if ((rv = getaddrinfo(argv[2], argv[3], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    //Obtaining the IPv4/IPv6 addresses in text format
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    // Populating the SBCP message frame
    // Initailizing with username
    struct SBCP msg; 
    msg.vrsn_type = (3<<7)|(2);
    msg.at[0].attrib_type = 2;
    memset(msg.at[0].payload, '\0', sizeof(msg.at[0].payload));
    strcpy(msg.at[0].payload,argv[1]); //username initially to join
    msg.at[0].attrib_len = strlen(msg.at[0].payload)+4;
    msg.frame_len = msg.at[0].attrib_len + 4; 
    
    htons_struct(&msg);
    //JOIN
    if (send(sockfd, (char *)&msg, sizeof (msg), 0) == -1){
        printf("Error sending\n");
        perror("send");
    }

    printf("client: JOIN, now SEND/RECV \n"); 
    // FD_SET tmp variable for select() 
    fd_set tmp;

    while(1)
    {

    FD_ZERO(&tmp);
    FD_SET(0,&tmp);
    FD_SET(sockfd,&tmp);

    if(select(sockfd+1,&tmp,NULL,NULL,NULL) == -1) {
	printf("Error with select \n");
	perror("select");
	exit(1);
    }
    
    if(FD_ISSET(0,&tmp))
    {
	printf("\n%s: ",argv[1]);
	fgets(buf,MAXDATASIZE,stdin);
	msg.vrsn_type = (3<<7) | 4;
	msg.at[0].attrib_type = 4;
	memset(msg.at[0].payload, '\0', sizeof(msg.at[0].payload));
	strcpy(msg.at[0].payload,buf); 
	msg.at[0].attrib_len = strlen(msg.at[0].payload)-1+4;
	msg.frame_len = msg.at[0].attrib_len + 4;

	htons_struct(&msg);
	//SEND
	if (send(sockfd, (char *)&msg, sizeof(msg), 0) == -1){
        printf("Error sending\n");
        perror("send");
    	}

    }
    
    if(FD_ISSET(sockfd,&tmp))
    {
	if ((numbytes = recv(sockfd, buf,sizeof(buf), 0)) == -1) {
            perror("recv");
     	       exit(1);
	}
	buf[numbytes] = '\0';
	struct SBCP *recv_msg=(struct SBCP*) &buf;
	//FWD message
	ntohs_struct(recv_msg);
	if(((recv_msg->vrsn_type)&0x7F) == 3) { 
		printf("\n%s: %s",recv_msg->at[0].payload,recv_msg->at[1].payload);
	}
	// ACK message
        else if(((recv_msg->vrsn_type)&0x7F) == 7) {
                printf("\nACK: NUMBER OF ACTIVE CLIENTS: %d \nCLIENTS: %s \n",recv_msg->at[0].payload[0],&recv_msg->at[0].payload[1]);
        }
	// NAK message
	else if(((recv_msg->vrsn_type)&0x7F) == 5) {
                printf("\nNAK: %s\n",recv_msg->at[0].payload);
        }
	// Online message
	else if(((recv_msg->vrsn_type)&0x7F) == 8) {
                printf("\n%s is now ONLINE \n",recv_msg->at[0].payload);
        }
	// Offline message
	else if(((recv_msg->vrsn_type)&0x7F) == 6) {
                printf("\n%s is now OFFLINE \n",recv_msg->at[0].payload);
        }

    }

    }

    close(sockfd);

    return 0;
}
