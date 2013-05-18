/*
 *  Shared header file for caca_socket client-to-client connection
 *
 *  Copyright (c) 2013 Carlos Jaramillo <cjaramillo@gc.cuny.edu>
 *                All Rights Reserved
 *
 *  This program is free software. It comes without any warranty, to
 *  the extent permitted by applicable law. You can redistribute it
 *  and/or modify it under the terms of the Do What the Fuck You Want
 *  to Public License, Version 2, as published by Sam Hocevar. See
 *  http://www.wtfpl.net/ for more details.
 */

#ifndef CACA_SOCKET_H_
#define CACA_SOCKET_H_
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define SOCKADDR       (struct sockaddr*)
#define SIZE           sizeof(struct sockaddr_in)
#define DEFAULT_HOST   "eniac.geo.hunter.cuny.edu"
#define PORT           25666
#define  ERROR_EXIT( _mssg, _num)  perror(_mssg);exit(_num);
#define  MAXLINE       4096

int connect_to_peer_socket(const char* peer_hostname, struct sockaddr_in * server);
int send_receive_data_through_socket(int sockfd, char* sendline, char * recvline);
void print_IP_addresses();

#endif // CACA_SOCKET_H_
