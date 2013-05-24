/** \file caca_socket.h
 *  \author Carlos Jaramillo <cjaramillo@gc.cuny.edu>
 *  \brief The caca_socket public header based on sockets usef by \cacatalk .
 *
 Course         : CS 82010 UNIX Application Development (Spring 2013)
 Created on     : May 24, 2013
 Description    : A very simple video and text chat interface (peer-to-peer (non centralized)) based on libcaca
 License        : ​Do What The Fuck You Want To Public License (WTFPL)
 */

#ifndef CACA_SOCKET_H_
#define CACA_SOCKET_H_
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#define _GNU_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define SOCKADDR       (struct sockaddr*)
#define SIZE           sizeof(struct sockaddr_in)
#define DEFAULT_HOST   "eniac.geo.hunter.cuny.edu"
#define PORT_CHAT           25666
#define PORT_VIDEO          25667
#define  ERROR_EXIT( _mssg, _num)  perror(_mssg);exit(_num);
#define  MAXLINE       4096
#define LISTEN_QUEUE_SIZE   5
#define  MAXFD( _x, _y)  ((_x)>(_y)?(_x):(_y))

void set_non_block(int fd);
/** TODO: description
 *
 * @param peer_hostname The IP address or hostname as a human readable character string
 * @param server The socket address structure to be realized based on the connection to the peer_hostname
 * @param port  The port number to connect to (assumes the other side is listening)
 *
 * @return socket file descriptor number. Or -1 if not connected.
 */
int connect_to_peer_socket(const char* peer_hostname, struct sockaddr_in * server, in_port_t port);

/** @brief Create socket and connect it to the address specified by
 *      'host' + 'service'/'type'.
 *@note Function based on Michael Kerrisk's source code for his book "The Linux Programming Interface"
 *
 *@param host           NULL for loopback IP address, or a host name or numeric IP address
 *@param service        either a name or a port number
 *@param type           either SOCK_STREAM or SOCK_DGRAM
 *
 *@return socket descriptor on success, or -1 on error
 */
int inet_connect(const char *host, const char *service, int type);

/** @brief Resolves all the network interfaces and prints addresses to standard output (if desired)
 *         In fact, it provides the IPv4 presentation address for the local host.
 *
 * @param presentation_addr The presentation address in IPv4 format (dotted decimal)
 * @param print         1: prints all addresses to standard output. Or not (0: default)
 */
void get_IP_addresses(char *presentation_addr, int print);

/** @brief The following typedef simplifies the function definition after it
 *
 */
typedef void Sigfunc(int); // for signal handlers

/** @brief override existing signal function to handle non-BSD systems
 *
 */
Sigfunc* Signal(int signo, Sigfunc *func);

// Signal handlers
void on_sigchld(int signo);

#endif // CACA_SOCKET_H_
