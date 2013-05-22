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
#include <sys/param.h>
#include <ifaddrs.h>
#include <netdb.h>

#include "caca_socket.h"

#define  MAXFD( _x, _y)  ((_x)>(_y)?(_x):(_y))

void print_IP_addresses()
{
  // This only produces the localhost address
  //printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *) client_as_host->h_addr_list[0])));
  // Solution to obtain interface address(es):
  struct ifaddrs * if_addr_struct = NULL;
  struct ifaddrs * if_addr_ptr = NULL;
  void * tmp_addr_ptr = NULL;
  getifaddrs(&if_addr_struct);
  for (if_addr_ptr = if_addr_struct; if_addr_ptr != NULL ; if_addr_ptr = if_addr_ptr->ifa_next)
  {
    if (if_addr_ptr->ifa_addr->sa_family == AF_INET) // check it is IP4
    { // is a valid IP4 Address
      tmp_addr_ptr = &((struct sockaddr_in *)if_addr_ptr->ifa_addr)->sin_addr;
      char address_buffer_v4[INET_ADDRSTRLEN];
      // Convert IP address from network (binary) to textual form (Presentation (eg. dotted decimal))
      inet_ntop(AF_INET, tmp_addr_ptr, address_buffer_v4, INET_ADDRSTRLEN);
      printf("%s IPv4 Address = %s\n", if_addr_ptr->ifa_name, address_buffer_v4);
    }
    else if (if_addr_ptr->ifa_addr->sa_family == AF_INET6) // check it is IP6
    { // is a valid IP6 Address
      tmp_addr_ptr = &((struct sockaddr_in6 *)if_addr_ptr->ifa_addr)->sin6_addr;
      char address_buffer_v6[INET6_ADDRSTRLEN];
      // Convert IP address from network (binary) to textual form (Presentation (eg. dotted decimal))
      inet_ntop(AF_INET6, tmp_addr_ptr, address_buffer_v6, INET6_ADDRSTRLEN);
      printf("%s IPv6 Address = %s\n", if_addr_ptr->ifa_name, address_buffer_v6);
    }
  }
  // Free memory
  if (if_addr_struct != NULL )
    freeifaddrs(if_addr_struct);

  char dummychar = getc(stdin); // TODO: Pause for debugging socket
}

/** TODO: description
 *
 * @return socket file descriptor
 */
int connect_to_peer_socket(const char* peer_hostname, struct sockaddr_in * server, in_port_t port)
{
  int sockfd;
  char ip_name[256] = "";
  char my_host_name[MAXHOSTNAMELEN] = "";
//  struct sockaddr_in server;
  struct hostent *client_as_host;
  struct hostent *server_as_host;

  strcpy(ip_name, peer_hostname);

  if(gethostname(my_host_name, MAXHOSTNAMELEN) == 0 )
  {
    if ((client_as_host = ( struct hostent * ) gethostbyname(my_host_name)) == NULL )
    {
      ERROR_EXIT("gethostbyname", 1);
    }
    else
    {
      printf ( "hostname (self): %s\n" , client_as_host->h_name );
      // This only produces the localhost address
      //printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *) client_as_host->h_addr_list[0])));

    }
  }

  if ((server_as_host = gethostbyname(ip_name)) == NULL )
  {
    ERROR_EXIT("gethostbyname", 1);
  }

  memset(server, 0, sizeof(*server));
  memcpy(&(server->sin_addr), SOCKADDR *server_as_host->h_addr_list, SIZE);
  server->sin_family = AF_INET;
//  server->sin_family = AF_UNSPEC; // TODO: allow IPv4 or IPv6
  server->sin_port = port;

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    ERROR_EXIT("socketcall failed", 1)
  }

  if (connect(sockfd, SOCKADDR server, sizeof(*server)) == -1)
  {
    char err_msg[MAXLINE];
    sprintf(err_msg, "connect call to %s:%d failed", peer_hostname, (int) port);
    ERROR_EXIT(err_msg, 1);
  }

  return sockfd;
}

/** TODO: description
 *
 * @return number of bytes sent through socket
 */
int send_receive_data_through_socket(int sockfd, char* sendline, char * recvline, int send_bytes)
{
  /*
  // Clear receive buffer
  memset(recvline, '\0', MAXLINE);

  int maxfd, n;
  fd_set readset;
  fd_set writeset;
//  int stdin_eof = 0;
  struct timeval tv;

  // do socket initialization etc.

  tv.tv_sec = 1;
  tv.tv_usec = 500000;

  // tv now represents 1.5 seconds

  maxfd = MAXFD(fileno(stdin), sockfd) + 1;

  FD_ZERO(&readset);
  FD_ZERO(&writeset);

  if (stdin_eof == 0)
    FD_SET(fileno(stdin), &readset);

  FD_SET(sockfd, &readset);
  FD_SET(sockfd, &writeset);

  */

  int send_status = send(sockfd, sendline, send_bytes, 0); // TODO: use this instead to write data

//  if (select(maxfd, &readset, NULL, NULL, NULL ) == 0)
//  if (select(maxfd, &readset, NULL, NULL, &tv ) == 0)
//  if (select(maxfd, &readset, &writeset, NULL, 0 ) == 0)
//  if (select(maxfd, NULL, &writeset, NULL, NULL ) == 0)
  {
    // Receive data
    /*
    if (FD_ISSET(sockfd, &readset))
    {
      if ((n = recv(sockfd, recvline, MAXLINE - 1, 0)) == 0)
      {
        recvline[n] = '\0';
      }
    }
    */

    // Send data
    /*
    if ( FD_ISSET(fileno(stdin), &readset))
//    if ( FD_ISSET(sockfd, &readset))
    {
          int send_status = send(sockfd, sendline, strlen(sendline), 0); // TODO: use this instead to write data
          printf("sent %d bytes\n", send_status);
    }
    */
/*
    //      if (FD_ISSET(sockfd, &writeset))
      {
        if (sendline != NULL)
        {
          //        write(sockfd, sendline, strlen(sendline));
          int send_status = send(sockfd, sendline, strlen(sendline), 0); // TODO: use this instead to write data
          if(send_status >= 0)
          {
            return send_status;
          }
          else // -1
          {
            //          stdin_eof = 1;
            shutdown(sockfd, SHUT_WR);
            FD_CLR(sockfd, &writeset);
            return send_status;
          }
        }
      }
      */
  }
//  else
//    return -1; // -1 implies failure

  return send_status; // 0 Implies success
}


void str_receive(int sockfd)
{
  ssize_t n;
  char recvline[MAXLINE];

  for (;;) // FIXME: don't put a loop like this
  {
//    if ((n = recv(sockfd, recvline, MAXLINE - 1, 0)) == 0)
    if ((n = recv(sockfd, recvline, MAXLINE - 1, 0)) == 0)
      return; /* connection closed by other end */

    recvline[n] = '\0';
    fwrite(recvline, n, 1, stdout);
    //fputs(recvline, stdout);

    //write(sockfd, recvline, n); // TODO:send back stuff
  }
}


void set_non_block(int fd )
{
    int flagset;

    flagset   = fcntl(fd, F_GETFL);
    flagset   |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flagset);
}

/* if a SIGCHLD is received: */
void on_sigchld(int signo)
{
  pid_t pid;
  int status;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    ;
  return;
}

Sigfunc* Signal(int signo, Sigfunc *func)
{
  struct sigaction act, oact;

  act.sa_handler = func;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  if (SIGALRM != signo)
  {
    act.sa_flags |= SA_RESTART;
  }
  if (sigaction(signo, &act, &oact) < 0)
    return (SIG_ERR );
  return (oact.sa_handler);
}

