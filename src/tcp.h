/*
 * tcp.h
 *
 *  Created on: Oct 22, 2012
 *      Author: Tim
 */

#ifndef TCP_H_
#define TCP_H_

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>

int connectTcpSocket(const char *hostname, int portno);

#endif /* TCP_H_ */
