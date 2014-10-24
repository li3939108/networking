/*
** proxy server.c -- a stream socket server demo
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

#define MAXDATASIZE 1024
#define HTTP "HTTP/1.0"

// Getting Month
int getMonth(const char *strMonth) {
    int i;
    static char *months[] = { 
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    for (i = 0; i < 12; ++i) {
        if (!strncmp(strMonth, months[i], 3)) return i;
    }
    return -1;
}
 
// Getting Day of week
int getDay(const char *strDay) {
    int i;
    static char *days[] = { 
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    for (i = 0; i < 7; ++i) {
        if (!strncmp(strDay, days[i], 3)) return i;
    }
    return -1;
}

// Convert to tm
struct tm strToTm(const char *strDate) {
    struct tm tmDate = { 0 };
    int year = 0;
    char strMonth[4] = {0},strDay[4] = { 0 };
 
    sscanf(strDate, "%3s, %d %3s %d %d:%d:%d %*s", strDay, &(tmDate.tm_mday), 
        strMonth, &year, &(tmDate.tm_hour), &(tmDate.tm_min), &(tmDate.tm_sec));
    tmDate.tm_wday = getDay(strDay);
    tmDate.tm_mon = getMonth(strMonth);
    tmDate.tm_year = year - 1900;
    tmDate.tm_isdst = -1;
    return tmDate;
}
 
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
int check_cache(char *proxy[], char *entry, int sock, struct addrinfo *servinfo) {
    int i,j,n,sock_req;
    char *temp,*upd,*host,*path,buf[MAXDATASIZE],hbuf[MAXDATASIZE];
    char *e = "Expires: ";
    char *l = "Last-Modified: ";
    //Obtaining Current directory
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
	perror("server: getcwd() error\n");
	return 0;
    }
    struct tm exp, up, *now_gm;
    time_t now;
    FILE *fp;
    for(i=0; i<CACHE_SIZE; i++) {
	if(proxy[i] != NULL) {
		//Finding in the list, and replacing in LRU fashion if found
		if(!strcmp(proxy[i],entry)) {
			sprintf(cwd,"%s/%s",cwd,entry);
                        fp=fopen(cwd,"r");
			n = fread(buf, sizeof(char), sizeof(buf), fp);
			// Checking Expires time
			if((temp = strstr(buf,e)) != NULL) {
				exp=strToTm(temp+strlen(e));
				printf("server: Expires: %s", asctime(&exp));
				now = time(NULL);				
				now_gm = gmtime(&now);
				printf("server: Current: %s", asctime(now_gm));
			}
			// If not expired, then send from proxy cache
			if(difftime(timegm(&exp),now)>0 && (temp != NULL)) {
				send_cache:
					rewind(fp);
				        while(!feof(fp)) {
				                n = fread(buf, sizeof(char), sizeof(buf), fp);
				                if(!(n<=0))
				                        send(sock,buf,n,0);
				        }
				        fclose(fp);
					for(j=i; j>0; j--) {
						strcpy(proxy[j],proxy[j-1]);
					}	
					strcpy(proxy[0],entry);
					printf("server: Obtained from proxy cache \n\n");
					return 1;
			}
			// If expired in proxy cache, check if modified
			else {
				// Checking Last Updated time
				strcpy(hbuf,entry);
				replace_char(hbuf,'_','/');
				host = strtok((char*)hbuf,"/");
				path = strtok(NULL,"\n");
				// implementing a HEAD request
				sprintf(buf,"HEAD /%s HTTP/1.0\r\nHost: %s\r\n\r\n",path,host);
				sock_req = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
				connect(sock_req, servinfo->ai_addr, servinfo->ai_addrlen);
				
				n = send(sock_req,buf,sizeof(buf),0);
				if(n<0)
					perror("server: Error sending to the socket\n");
				
				n=recv(sock_req,buf,sizeof(buf),0);
				if((upd = strstr(buf,l)) != NULL) {
					up=strToTm(upd+strlen(l));
					printf("server: Last-Modified: %s", asctime(&up));
				}
				close(sock_req);
				//Checking if modified after expiry
				if(difftime(timegm(&exp),timegm(&up))>0 && temp!=NULL && upd!=NULL) 
					goto send_cache;
				
				// Expired and modified after that, so re-obtain from server
				remove(cwd);
				fclose(fp);
				for(j=i; j<CACHE_SIZE-1; j++) {
					if(proxy[j+1] != NULL) 
						strcpy(proxy[j],proxy[j+1]);
					else
						break;
				}
				proxy[j]=NULL;
				return 0;
			}
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
		for(j=i; j>0; j--) {
        		strcpy(proxy[j],proxy[j-1]);
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
    socklen_t sin_size;
    FILE *fp;
    int yes=1;
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
							
											
						if ((getaddrinfo(url, "http", &hints, &servinfo)) != 0) {	
							goto badurl;
						}					
		
						printf("server: HostPath = %s%s\n",url,path);
						   
						sock_req = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
						new_fd=connect(sock_req, servinfo->ai_addr, servinfo->ai_addrlen);
						inet_ntop(servinfo->ai_family, get_in_addr(servinfo->ai_addr), s, sizeof s);
						printf("server: Connected to %s IP - %s\n",url,s);
						if(new_fd<0)
							perror("server: Error in connecting to remote server");
											
						strcpy(tmp,url);
						replace_char(strcat(tmp,path),'/','_');
						
						//Checking the cache in the directory
						if(check_cache(proxy_cache,tmp,i,servinfo)) {
							close(i); // bye!
		                                        FD_CLR(i, &master); // remove from master set
							continue;
						}

						//Obtaining Current directory
						if (getcwd(cwd, sizeof(cwd)) == NULL) {
	               			        	perror("server: getcwd() error\n");
							continue;
						}

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
									printf ("server: file not found (%s)\n", cwd);
								}

								bzero((char*)buf,sizeof(buf));
								n=recv(sock_req,buf,sizeof(buf),0);
								fwrite(buf, n, sizeof(char), fp);
								fclose(fp);
								if(!(n<=0))
									send(i,buf,n,0);

							}while(n>0);
						}
						add_entry(proxy_cache,url);
						printf("\n");
					}
					else
					{
						badurl:
						printf("server: bad url\n");
						send(i,"HTTP/1.0 400 bad request\nOnly HTTP requests allowed \r\n\r\n",100,0);
					}
					close(i); // bye!
                                        FD_CLR(i, &master); // remove from master set
				}
			}
		}
	}
    }	
    return 0;
}
