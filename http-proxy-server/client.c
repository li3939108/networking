/* 
HTTP1.0 client GET request
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

#define MAXDATASIZE 1024 // max number of bytes we can get at once 

char cwd[MAXDATASIZE]; //path of current working directory
                   
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(int argc, char const *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv,i;
	char *tkn,*host,*header,*fname,s[INET6_ADDRSTRLEN],query[MAXDATASIZE]; 
	FILE *fp;

	if (argc != 4) {
	fprintf(stderr,"usage: client <proxy address> <proxy port> <url to retrieve>\n");
	exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
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
	
	while(i<strlen(argv[3])) {
		if(argv[3][i]==':') 
			break;
		i++;
	}		

	if(i==strlen(argv[3])) 
		host=strtok((char*)argv[3],"/");
	else {
		host=strtok((char*)argv[3],"/");		
		host=strtok(NULL,"/");
	}			

	tkn=strtok(NULL,"\n");
	tkn=(tkn==NULL)?"":tkn;
	sprintf(query, "GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n",tkn,host);
	printf("client: %s\n", query);		
	if (send(sockfd, query, sizeof(query), 0) == -1){
		printf("Error sending\n");
		perror("send");
    	}

	//Obtaining Current directory
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
	        perror("client: getcwd() error");
		exit(1);
	}
	
	// Getting file name
	if((!strcmp(tkn,"")) || (!strlen(tkn)))
		fname=host;
	else {
		char *tem=tkn;
		header=strtok(tem,"/\n");
		while(header!=NULL) {
			fname=header;
			header=strtok(NULL,"/\n");
		}
	}
		
	//Appending the filename to the path and storing in the directory
        sprintf(cwd,"%s/%s",cwd,fname);                           
	                                               
	i=0;
	if((fp = fopen (cwd, "r")) != NULL)
		remove(cwd);
	do {
		if ((numbytes = recv(sockfd, buf,sizeof(buf), 0)) == -1) {
                        perror("recv");
                        exit(1);
                }
	
		buf[numbytes] = '\0';
		if(!i) { 
			char temp[MAXDATASIZE];
			header=strstr(buf,"\r\n\r\n");
			if(header)
				header += 4;
			else
				continue;
			strncpy(temp,buf,header-buf);
			
			if(strncmp(buf+9,"200",3) != 0) {
				printf("\nclient:\n%s\n\nPlease try again...\n\n",temp);
				break;
			}
			printf("\nclient:\n%s\n\nDocument at : %s \n",temp,cwd);
			numbytes -= header-buf;
			memmove(&buf,header,numbytes);
			i=1;
		}

		fp = fopen (cwd, "a+");
		if (fp == NULL) {
			printf ("client: file not found (%s)\n", cwd);
		}     
		
		fwrite(buf, numbytes, sizeof(char), fp);
		fclose(fp);

	} while(numbytes > 0);
	
	close(sockfd);

	return 0;
}
