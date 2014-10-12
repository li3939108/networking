/* 
ECEN602: Computer Networks
HW3 programming assignment: HTTP1.0 proxy server
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
	/* write HTTP1.0 proxy server code */
	
	int sockfd, newfd;  // listen on sock_fd, new connection on newfd
	struct addrinfo hints, *servinfo, *p;
	//struct sockaddr_storage their_addr; // connector's address information
	//socklen_t sin_size;
	//struct sigaction sa;
	int yes=1;
	//char s[INET6_ADDRSTRLEN];
	int rv;

	fd_set master;    // master file descriptor list
	fd_set read_fds;  // temp file descriptor list for select()
	int fdmax;        // maximum file descriptor number

	//int listener;     // listening socket descriptor
	struct sockaddr_storage remoteaddr;
	socklen_t addrlen;	
	int i, j;
	char remoteIP[INET6_ADDRSTRLEN];
	char buf[256];    // buffer for client data
    	int nbytes;

	//Checking specification of command line options
	if (argc != 3) {
		fprintf(stderr,"usage: proxy <ip to bind> <port to bind>\n");
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
	
	FD_ZERO(&master);    // clear the master and temp sets
    	FD_ZERO(&read_fds);
 	// add sockfd to the master set
	FD_SET(sockfd, &master);

	// keep track of the biggest file descriptor
	fdmax = sockfd; // so far, it's this one
	// main loop
    	for(;;) {
        	read_fds = master; // copy it
        	if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            		perror("select");
            		exit(4);
        	}
	        // run through the existing connections looking for data to read
		for(i = 0; i <= fdmax; i++) {
		    if (FD_ISSET(i, &read_fds)) { // we got one!!
		        if (i == sockfd) {
		            // handle new connections
		            addrlen = sizeof remoteaddr;
		            newfd = accept(sockfd,
		                (struct sockaddr *)&remoteaddr,
		                &addrlen);

		            if (newfd == -1) {
		                perror("accept");
		            } else {
		                FD_SET(newfd, &master); // add to master set
		                if (newfd > fdmax) {    // keep track of the max
		                    fdmax = newfd;
		                }
		                printf("selectserver: new connection from %s on "
		                    "socket %d\n",
		                    inet_ntop(remoteaddr.ss_family,
		                        get_in_addr((struct sockaddr*)&remoteaddr),
		                        remoteIP, INET6_ADDRSTRLEN),
		                    newfd);
		            }
		        } else {
		            // handle data from a client
		            if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
		                // got error or connection closed by client
		                if (nbytes == 0) {
		                    // connection closed
		                    printf("selectserver: socket %d hung up\n", i);
		                } else {
		                    perror("recv");
		                }
		                close(i); // bye!
		                FD_CLR(i, &master); // remove from master set
		            } else {
		                // we got some data from a client
		                for(j = 0; j <= fdmax; j++) {
		                    // send to everyone!
		                    if (FD_ISSET(j, &master)) {
		                        // except the listener and ourselves
		                        if (j != sockfd && j != i) {
		                            if (send(j, buf, nbytes, 0) == -1) {
		                                perror("send");
		                            }
		                        }
		                    }
		                }
		            }
		        } // END handle data from client
		    } // END got new incoming connection
		} // END looping through file descriptors
	} // END for(;;)--and you thought it would never end!

/*	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
		    perror("accept");
		    continue;
        	}

		inet_ntop(their_addr.ss_family,
		    get_in_addr((struct sockaddr *)&their_addr),
		    s, sizeof s);
		printf("server: got connection from %s\n", s);
		//if (!fork()) { // this is the child process
		    close(sockfd); // child doesn't need the listener
		    if (send(new_fd, "Hello, world!", 13, 0) == -1)
		        perror("send");
		    close(new_fd);
		    exit(0);
		//}
		//close(new_fd);  // parent doesn't need this
	}*/

	return 0;
}
