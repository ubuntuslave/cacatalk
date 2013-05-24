/** \file caca_socket.h
 *  \author Carlos Jaramillo <cjaramillo@gc.cuny.edu>
 *  \brief The caca_socket implementation file for cacatalk connections to a listening peer.
 *
 Course         : CS 82010 UNIX Application Development (Spring 2013)
 Created on     : May 24, 2013
 Description    : A very simple video and text chat interface (peer-to-peer (non centralized)) based on libcaca
 License        : ​Do What The Fuck You Want To Public License (WTFPL)
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <ifaddrs.h>
#include <netdb.h>

#include "caca_socket.h"

void get_IP_addresses(char *presentation_addr, int print)
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
      if (print == 1)
        printf("%s IPv4 Address = %s\n", if_addr_ptr->ifa_name, address_buffer_v4);
      if (strlen(address_buffer_v4) > 4)
        strcpy(presentation_addr, address_buffer_v4);
    }
    else if (if_addr_ptr->ifa_addr->sa_family == AF_INET6) // check it is IP6
    { // is a valid IP6 Address
      tmp_addr_ptr = &((struct sockaddr_in6 *)if_addr_ptr->ifa_addr)->sin6_addr;
      char address_buffer_v6[INET6_ADDRSTRLEN];
      // Convert IP address from network (binary) to textual form (Presentation (eg. dotted decimal))
      inet_ntop(AF_INET6, tmp_addr_ptr, address_buffer_v6, INET6_ADDRSTRLEN);
      if (print == 1)
        printf("%s IPv6 Address = %s\n", if_addr_ptr->ifa_name, address_buffer_v6);
    }
  }
  // Free memory
  if (if_addr_struct != NULL )
    freeifaddrs(if_addr_struct);

}

int connect_to_peer_socket(const char* peer_hostname, struct sockaddr_in * server, in_port_t port)
{
  int sockfd = -1; // Initial invalid file descriptor number
  char ip_name[256] = "";
  char my_host_name[MAXHOSTNAMELEN] = "";
//  struct sockaddr_in server;
  struct hostent *client_as_host;
  struct hostent *server_as_host;

  strcpy(ip_name, peer_hostname);

  if (gethostname(my_host_name, MAXHOSTNAMELEN) == 0)
  {
    if ((client_as_host = (struct hostent *)gethostbyname(my_host_name)) == NULL )
    {
      ERROR_EXIT("gethostbyname", 1);
    }
    else
    {
      printf("hostname (self): %s\n", client_as_host->h_name);
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
    sprintf(err_msg, "connect call to %s:%d failed", peer_hostname, (int)port);
    ERROR_EXIT(err_msg, 1);
  }

  return sockfd;
}

int inet_connect(const char *host, const char *service, int type)
{
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int sfd, s;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;
  hints.ai_family = AF_UNSPEC; // Allows IPv4 or IPv6
  hints.ai_socktype = type;

  s = getaddrinfo(host, service, &hints, &result);
  if (s != 0)
  {
    errno = ENOSYS;
    return -1;
  }

  // Walk through returned list until we find an address structure
  //   that can be used to successfully connect a socket

  for (rp = result; rp != NULL ; rp = rp->ai_next)
  {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1)
      continue; // On error, try next address

    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
      break; // Success

    // Connect failed: close this socket and try next address

    close(sfd);
  }

  freeaddrinfo(result);

  return (rp == NULL ) ? -1 : sfd;
}

void set_non_block(int fd)
{
  int flagset;

  flagset = fcntl(fd, F_GETFL);
  flagset |= O_NONBLOCK;
  fcntl(fd, F_SETFL, flagset);
}

// TODO: not used
/* if a SIGCHLD is received: */
/*
void on_sigchld(int signo)
{
  pid_t pid;
  int status;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    ;
  return;
}
*/

/*
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
*/
