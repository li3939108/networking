/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define BACKLOG 10     	// how many pending connections queue will hold
#define CACHE_SIZE 10	// Number of pages to be stored in the cache 

#define MAXDATASIZE 512
#define HTTP "HTTP/1.0"

char cwd[MAXDATASIZE]; //path of current working directory


//Search and replace a character in a string
void replace_char (char *s, char find, char replace) {
    while (*s != 0) {
    	if (*s == find)
		*s = replace;
	s++;
    }
}

// Checking and replacing in cache 
int check_cache(char *proxy[], char *entry) {
    int i,j;
    for(i=0; i<CACHE_SIZE; i++) {
	if(proxy[i] != NULL) {
		//Finding in the list, and replacing in LRU fashion if found
		if(!strcmp(proxy[i],entry)) {
			for(j=0; j<i; j++) {
				strcpy(proxy[j+1],proxy[j]);
			}
			strcpy(proxy[0],entry);
			return 1;
		}
	}
    }
    return 0;
}

void add_entry(char *proxy[], char *entry) {
    int i,j;
    for(i=0; i<CACHE_SIZE; i++) {
	if(proxy[i] == NULL) {
		proxy[i]=(char*) malloc(MAXDATASIZE);
		//Adding at the front
		for(j=0; j<i; j++) {
        		strcpy(proxy[j+1],proxy[j]);
        	}	
		strcpy(proxy[0],entry);
		return;
	}
    }
    
    // Cache full, so discarding the LRU entry 
    for(j=0; j<CACHE_SIZE-1; j++) {
	strcpy(proxy[j+1],proxy[j]);
    }
    strcpy(proxy[0],entry);
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
    int sockfd, sockmax, sock_req, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    struct sockaddr_in host_addr;
    socklen_t sin_size;
    struct hostent *host;
    FILE *fp;
    int yes=1, port;
    char s[INET6_ADDRSTRLEN];
    int rv,i,n,numbytes;
    char buf[MAXDATASIZE], url[MAXDATASIZE], tmp[MAXDATASIZE], prot[10], http[MAXDATASIZE],path[MAXDATASIZE];
    char *proxy_cache[CACHE_SIZE]; // Store entries in LRU fashion

    for(i=0; i<CACHE_SIZE; i++) {
	proxy_cache[i]=NULL;
    }
    //Checking specification of command line options
    if (argc != 3) {
        fprintf(stderr,"usage: server server_ip server_port \n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
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

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
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

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    // FD_SET variables for select() 
    fd_set master,read_fds;

    // clear the master and temp sets
    FD_ZERO(&master);    
    FD_ZERO(&read_fds);

    // add the socket descriptor to the master set
    FD_SET(sockfd, &master);

    // keep track of the largest socket descriptor to use with select()
    sockmax = sockfd; // so far, it's this one

    printf("server: waiting for connections...\n");

    while(1) 
    {  // main accept() loop
	read_fds = master;
	if(select(sockmax+1,&read_fds,NULL,NULL,NULL) == -1) {
	        printf("Error with select \n");
        	perror("select");
	        exit(1);
    	}

	//Looping through all incoming socket connections 
	for(i=0; i<=sockmax; i++) {
		if(FD_ISSET(i, &read_fds)) {
			if(i == sockfd) {
				//Accept the new connection
				sin_size = sizeof their_addr;
				new_fd =accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
				if(new_fd == -1) {
					perror("accept");
					continue;
				}
				else {
					printf("server: Adding to master %d\n",new_fd);
					FD_SET(new_fd, &master); //Adding to master set
					if(new_fd > sockmax) {
						sockmax = new_fd;
					}

				inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
				printf("server: %d got connection from %s\n", sockfd,s);	
				}
           		}
			
			else {
		            	// handle data from a client
		            	if ((numbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
		                        // got error or connection closed by client
		                	if (numbytes == 0) {
					        // connection closed
				        	printf("server: socket %d hung up! Nothing received\n", i);
		                	} 
					else {
						printf("server: some error receiving \n");
				            	perror("recv");
		                	}

		                	close(i); // bye!
		                	FD_CLR(i, &master); // remove from master set
		            	} 
				else {
				        // we got some data from a client
					buf[numbytes] = '\0';
					sscanf(buf,"%s %s %s\r\nHost: %s\r\n\r\n",http,path,prot,url);			
					printf("server: %s %s %s %s\n",http,path,prot,url);
					if(((strncmp(http,"GET",3)==0))&&(strncmp(prot,HTTP,8)==0)) {
												
						port=80;
					
						//Obtaining Current directory
						if (getcwd(cwd, sizeof(cwd)) == NULL) {
	               			        	perror("client: getcwd() error\n");
							continue;
						}
						
						strcpy(tmp,url);
						replace_char(strcat(tmp,path),'/','_');
						
						//Checking the cache in the directory
						if(check_cache(proxy_cache,tmp)) {
							sprintf(cwd,"%s/%s",cwd,tmp);
							fp=fopen(cwd,"r");
							printf("server: Obtained from proxy cache \n\n");
							while(!feof(fp)) {
								n = fread(buf, sizeof(char), sizeof(buf), fp);
								if(!(n<=0))
									send(i,buf,n,0);
							}
							fclose(fp);
							continue;
						}

						host=gethostbyname(url);
                                                if(host==NULL)
                                                        goto badurl;
						                                                   
						printf("server: HostPath = %s%s\tPort = %d\n",url,path,port);
						   
						bzero((char*)&host_addr,sizeof(host_addr));
						host_addr.sin_port=htons(port);
						host_addr.sin_family=AF_INET;
						bcopy((char*)host->h_addr,(char*)&host_addr.sin_addr.s_addr,host->h_length);
						   
						sock_req = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
						new_fd=connect(sock_req,(struct sockaddr*)&host_addr,sizeof(struct sockaddr));
						printf("server: Connected to %s IP - %s\n\n",url,inet_ntoa(host_addr.sin_addr));
						if(new_fd<0)
							perror("server: Error in connecting to remote server");
						
						//Appending the filename to the path
                                                replace_char(strcat(url,path),'/','_');
                                                sprintf(cwd,"%s/%s",cwd,url);

						n=send(sock_req,buf,strlen(buf),0);
						if(n<0)
							perror("Error writing to socket");
						else{
							do
							{
								fp = fopen (cwd, "a+");
								if (fp == NULL) {
									printf ("client: file not found (%s)\n", cwd);
								}

								bzero((char*)buf,sizeof(buf));
								n=recv(sock_req,buf,sizeof(buf),0);
								fputs(buf,fp);
								fclose(fp);
								if(!(n<=0))
									send(i,buf,n,0);

							}while(n>0);
						}
						add_entry(proxy_cache,url);
					}
					else
					{
						badurl:
						send(i,"HTTP/1.0 400 : BAD REQUEST,ONLY HTTP REQUESTS ALLOWED",100,0);
					}
				}
			}
		}
	}
    }	
    return 0;
}
