/*
 * tcp.c
 *
 *  Created on: Oct 22, 2012
 *      Author: Tim
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <netdb.h>
#include <strings.h>
#include "log.h"

int connectTcpSocket(const char *hostname, int portno)
{
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int sockfd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
    	logError("failed to open local socket\n");
    	return sockfd;
    }

    server = gethostbyname(hostname);
    if (server == NULL) {
    	logError("failed to resolve hostname\n");
    	close(sockfd);
    	return -1;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
            (char *)&serv_addr.sin_addr.s_addr,
            server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
    	logError("failed to open remote socket\n");
    	close(sockfd);
    	return -2;
    }

	return sockfd;
}
