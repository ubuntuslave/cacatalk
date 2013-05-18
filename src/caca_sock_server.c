/******************************************************************************
 Title          : sockdemo2_server.c
 Author         : Stewart Weiss, based on code by Richard Stevens
 Created on     : May 10, 2008
 Description    : Connection-oriented upcase server
 Purpose        :
 Build with     : gcc -o sockdemo2_server sockdemo2_server.c
 Requires sockdemo1.h
 Usage          : Run this in the background on eniac or some other machine.
 Make sure that port 25666 is free or change sockdemo1.h
 to some free port. Turn off the firewall or configure it
 to allows connections to that port with TCP.


 ******************************************************************************/

#include "caca_socket.h"
#include "sys/wait.h"  

#define LISTEN_QUEUE_SIZE   5

// The following typedef simplifies the function definition after it
typedef void Sigfunc(int); // for signal handlers

// override existing signal function to handle non-BSD systems
Sigfunc* Signal(int signo, Sigfunc *func);

// Signal handlers
void on_sigchld(int signo);
void str_receive(int sockfd);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  pid_t childpid;
  socklen_t clilen;
  struct sockaddr_in client_addr, server_addr;
  void sig_chld(int);

  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    ERROR_EXIT("socket call failed", 1)
  }

  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY );
  server_addr.sin_port = PORT;

  if (bind(listenfd, SOCKADDR &server_addr, sizeof(server_addr)) == -1)
  {
    ERROR_EXIT(" bind call failed", 1)
  }

  if (-1 == listen(listenfd, LISTEN_QUEUE_SIZE))
  {
    ERROR_EXIT(" listen call failed", 1)
  }

  Signal(SIGCHLD, on_sigchld);

  for (;;)
  {
    clilen = sizeof(client_addr);
    if ((connfd = accept(listenfd, SOCKADDR &client_addr, &clilen)) < 0)
    {
      if (EINTR == errno)
        continue; // back to for()
      else
        ERROR_EXIT("accept error", 1);
    }

    if ((childpid = fork()) == 0)
    { // child process
      close(listenfd); // close listening socket

      // TODO: send back behavior
      str_receive(connfd); // process the request
      exit(0);
    }
    close(connfd); // parent closes connected socket
  }

  return 0;
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

