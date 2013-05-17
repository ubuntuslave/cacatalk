/******************************************************************************
 Title          : sockdemo2_client.c
 Author         : Stewart Weiss
 Created on     : May 10, 2008
 Description    : Connection-oriented Internet domain client w/name conversion
 and multiplexed I/O
 Purpose        : To demonstrate principles of connection oriented client
 using host name conversion
 Build with     : gcc -o sockdemo2_client sockdemo2_client.c -lnsl
 Requires sockdemo1.h

 Usage          : sockdemo2_client  [hostname]
 This assumes that the sockdemo2_server is running on the
 specified host and that the firewalls are open for port
 25555.  Sends to eniac by default.

 This reads lines of test from standard input and sends them
 to the server. It uses select() to choose between reading
 standard input and reading the socket.

 Modifications  : Dec. 13, 2011
 Replaced obsolete calls to gethostbyname for the newer
 POSIX-compliant, reentrant getaddrinfo() function.

 Notes:
 This client is designed to communicate with the sockdemo2_server. The
 sockdemo2_server expects lines of text and sends back that text converted to
 uppercase.

 This client uses the getaddrinfo() system call, which given an internet host
 and a "service", allocates and fills a linked list of addrinfo structures.
 The host and service are both character strings. The host can be a numeric
 IPV4 number-dot format such as "192.168.1.1" or if IPV6, a hex string, or
 a network host name such as "cs82010.gc.cuny.edu".
 The service can be either a name, such as that  found in the /etc/services
 file, e.g., "ssh" or "ftp", or a string representing a port number, such
 as "25555". There are other options -- see the man page for details.

 An addrinfo structure looks like
 struct addrinfo {
 int              ai_flags;
 int              ai_family;
 int              ai_socktype;
 int              ai_protocol;
 size_t           ai_addrlen;
 struct sockaddr *ai_addr;
 char            *ai_canonname;
 struct addrinfo *ai_next;
 };
 
 Before the call is made, the structure's ai_family, ai_socktype, ai_flags,
 and ai_protocol members should be initialized. For the address family, we
 can use AF_INET, AF_INET6, or AF_UNSPEC. The last lets the call use either
 IPv4 or IPv6, so we choose to use that here.
 Since our server is a stream-oriented server, we set the socktype to
 SOCK_STREAM.
 There are various flags that can be bitwise-or-ed but for this simple client
 we do not set any of them. The protocol is set to 0 to allow the system to
 choose any protocol for the requested port.

 ******************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "caca_socket.h"

#define  MAXFD( _x, _y)  ((_x)>(_y)?(_x):(_y))

/** TODO: description
 *
 * @return socket file descriptor
 */
int connect_to_peer_socket(const char* peer_hostname, struct sockaddr_in * server)
{
  int sockfd;
  char ip_name[256] = "";
//  struct sockaddr_in server;
  struct hostent *host;

  strcpy(ip_name, peer_hostname);

  if ((host = gethostbyname(ip_name)) == NULL )
  {
    ERROR_EXIT("gethostbyname", 1);
  }

  memset(server, 0, sizeof(*server));
  memcpy(&(server->sin_addr), SOCKADDR *host->h_addr_list, SIZE);
  server->sin_family = AF_INET;
  server->sin_port = PORT;

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    ERROR_EXIT("socketcall failed", 1)
  }

  if (connect(sockfd, SOCKADDR server, sizeof(*server)) == -1)
  {
    ERROR_EXIT("connect call failed", 1);
  }

  return sockfd;
}

/** TODO: description
 *
 * @return number of bytes sent through socket
 */
//int send_receive_data_through_socket(int sockfd, char* sendline, char * recvline)
int send_receive_data_through_socket(int sockfd, char* sendline)
{
  /*
  int maxfd, n;
  fd_set readset;
  int stdin_eof = 0;

  maxfd = MAXFD(fileno(stdin), sockfd) + 1;

  FD_ZERO(&readset);
  if (stdin_eof == 0)
    FD_SET(fileno(stdin), &readset);
  FD_SET(sockfd, &readset);

  if (select(maxfd, &readset, NULL, NULL, NULL ) > 0)
  {
    // Receive data
    if (FD_ISSET(sockfd, &readset))
    {
      if ((n = recv(sockfd, recvline, MAXLINE - 1, 0)) == 0)
      {
        recvline[n] = '\0';
      }
    }

  }
  */
  // Send data
//    if (FD_ISSET(fileno(stdin), &readset))
    if (sendline != NULL)
    {
      // TODO: use flags, see Socket Data Options
//      return send(sockfd, sendline, strlen(sendline), 0); // TODO: use this insteat to send data
      return write(sockfd, sendline, strlen(sendline));
    }

    return 0;
}

