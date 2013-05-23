/*
 * grabber.cpp
 *
 *  Created on: May 7, 2013
 *      Author: carlos
 */

/* V4L2 video picture grabber
 Copyright (C) 2009 Mauro Carvalho Chehab <mchehab@infradead.org>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/select.h>

#include "cacatalk.h"
#include "common_image.h"
#include "caca_socket.h"

//TODO: move to header file
#define CLEAR(x) memset(&(x), 0, sizeof(x))
// ******** Socket handlers  **********
void * send_chat(void * arguments);
void * receive_chat(void * arguments);
// ----------------------------
int chat(caca_canvas_t *cv, caca_display_t *dp, int sendfd, int recv_vid_fd, Window *win, char * peer_hostname);
int grab(caca_canvas_t *cv, caca_display_t *dp, video_params * vid_params, Window *win, int sockfd);
static void grab_messy(caca_canvas_t *cv, caca_display_t *dp, char *dev_name, int img_width, int img_height, int sockfd);
void set_window(int fd, unsigned short video_lines, Window *win);

int main(int argc, char **argv)
{
  Window win; // To store rows and colums of the terminal
  void (*demo)(void * arg1, ...) = NULL;
  int quit = 0;
  caca_canvas_t *cv;
  caca_display_t *dp;
  int img_width = 640; // TODO: parametrize
  int img_height = 480;
  char * dev_name;
  int is_connected = 0;
//  pthread_t threads[NUM_THREADS];

  options *arg_opts;
  arg_opts = (options *)malloc(sizeof(options));

  if (get_options(argc, argv, arg_opts) == -1)
  {
    fprintf(stderr, "%s: unable to get option arguments\n", argv[0]);
    return 1;
  }

  // --------------- Video related ---------------------------------
  unsigned int lines_of_video = 10; // Arbitrary number of lines // TODO: parametrize
  video_params *vid_params;
  vid_params = (video_params *)malloc(sizeof(video_params));
  dev_name = arg_opts->video_device_name;
  // ---------------------------------------------------

  set_window(STDIN_FILENO, lines_of_video, &win); // Create a window object in order to obtain dimensions

  // ----------------------------------------------------------------------
  // -------   Socket related:  -------------------------------------------
  int listenfd;
  int connfd = -1; // Initial value for non-existing connection
  int recvfd = -1; // Socket for receiving.
  struct sockaddr_in client_addr_accepted, server_addr_that_listens;
  static struct sockaddr_in server_addr_to_connect;
  socklen_t clilen = sizeof(client_addr_accepted);
  char address_buffer_v4[INET_ADDRSTRLEN] = "";

  // Solution to obtain interface address(es):
  print_IP_addresses();

  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    ERROR_EXIT("socket call failed", 1)
  }

  bzero(&server_addr_that_listens, sizeof(server_addr_that_listens));
  server_addr_that_listens.sin_family = AF_INET;
  server_addr_that_listens.sin_addr.s_addr = htonl(INADDR_ANY );
  server_addr_that_listens.sin_port = PORT;

  set_non_block(listenfd); // Set it as nonblocking descriptor (Not working)

  if (bind(listenfd, SOCKADDR &server_addr_that_listens, sizeof(server_addr_that_listens)) == -1)
  {
    ERROR_EXIT(" bind call failed", 1)
  }

  if (-1 == listen(listenfd, LISTEN_QUEUE_SIZE))
  {
    ERROR_EXIT(" listen call failed", 1)
  }

  Signal(SIGCHLD, on_sigchld); // TODO: Since we don't have children, handle your own signals with house keeping
  // ----------------------------------------------------------------------

//  cv = caca_create_canvas(80, 24);
  cv = caca_create_canvas(0, 0);
  if (!cv)
  {
    fprintf(stderr, "%s: unable to initialise libcaca\n", argv[0]);
    return 1;
  }

  const char * driver = "ncurses";
  dp = caca_create_display_with_driver(cv, driver);
//    dp = caca_create_display(cv);

  if (dp == NULL )
  {
    printf("Failed to create display\n");
    return 1;
  }

  int usec = 40000; // The refresh delay in microseconds.
  caca_set_display_time(dp, usec);
  caca_set_mouse(dp, 0); // Disable cursor by default
  caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY);
  caca_clear_canvas(cv);

  // Main menu //TODO:
  // v: allow turning video on,off
  // a: set address (or name) of peer for connecting to
  // c: enter chat room
//  display_menu();

  caca_refresh_display(dp);

  char label_name_of_peer[MAXHOSTNAMELEN] = ""; // Temp debug label

  // Go (main loop)
  while (!quit)
  {
    caca_event_t ev;
    int key_choice; // The key pressed to be switched upon
//    int menu = 0, mouse = 0, xmouse = 0, ymouse = 0; // TODO

    while (caca_get_event(dp, CACA_EVENT_ANY, &ev, 0))
    {
      if (demo && (caca_get_event_type(&ev) & CACA_EVENT_KEY_PRESS))
      {
        //menu = 1;
        demo = NULL;
      }
      else if (caca_get_event_type(&ev) & CACA_EVENT_KEY_PRESS)
      {
        key_choice = caca_get_event_key_ch(&ev);
        switch (key_choice)
        {
          case 'q':
          case 'Q':
          case CACA_KEY_ESCAPE:
            demo = NULL;
            quit = 1;
            break;
          case 'v':
          case 'V':
            key_choice = 'v';
            set_video(vid_params, dev_name, &win, img_width, img_height); //FIXME: arbitrary canvas height for video
            demo = grab; // TODO: just to test for now
//            demo = grab_messy; // TODO: just to test for now
            break;
          case 'c':
          case 'C':
            key_choice = 'c';
            demo = chat;
            break;
          case 'i':
          case 'I':
            key_choice = 'i';
            if (connfd < 0) // Only attempt a connection when server mode hasn't accepted one already
            {
              // Initialize connection
              // Check if there is a host name on command line; if not use default
              if ((strlen(arg_opts->peer_name) > 0) && (is_connected == 0))
              {
                // Attention: needs to have static memory for server
                connfd = connect_to_peer_socket(arg_opts->peer_name, &server_addr_to_connect, PORT);
                // TODO: use this instead:
                //connfd = inet_connect(arg_opts->peer_name, PORT, SOCK_STREAM);

                if (connfd == -1)
                  perror("main: Could not connect to server socket");
                if (connfd > 0)
                {
                  recvfd = dup(connfd);

                  void * peer_addr_ptr = NULL;
                  if (server_addr_to_connect.sin_family == AF_INET) // check it is IP4
                  { // is a valid IP4 Address
                    peer_addr_ptr = (struct sockaddr_in *)&(server_addr_to_connect.sin_addr);
                    // Convert IP address from network (binary) to textual form (Presentation (eg. dotted decimal))
                    inet_ntop(AF_INET, peer_addr_ptr, address_buffer_v4, INET_ADDRSTRLEN);

                    sprintf(label_name_of_peer, "%s@%s", "host", address_buffer_v4); // TODO:temp
                  }
                  is_connected = 1; // To indicate not longer trying to connect
                }
              }
            }
            break;
        }
      }

      /*// TODO: mouse events
       else if(caca_get_event_type(&ev) & CACA_EVENT_MOUSE_MOTION)
       {
       mouse = 1;
       xmouse = caca_get_event_mouse_x(&ev);
       ymouse = caca_get_event_mouse_y(&ev);
       }
       else if(caca_get_event_type(&ev) & CACA_EVENT_RESIZE)
       {
       mouse = 1; // old hack
       }
       */
    }

    // Acting in server mode by accepting connections at listening socket
    if ((is_connected == 0))
    {
      // Recall: non-blocking accept has been set for the listenfd
//      if ((recvfd = accept(listenfd, SOCKADDR &client_addr_accepted, &clilen)) < 0)
      if ((connfd = accept(listenfd, SOCKADDR &client_addr_accepted, &clilen)) < 0)
      {
        if (EINTR == errno)
          continue; // back to loop
        // Don't use if non-blocking:
        //else
        //ERROR_EXIT("accept error", 1);
      }
      else
      { // A connection has been accepted

//          connfd = dup(recvfd);
        recvfd = dup(connfd);

        void * peer_addr_ptr = NULL;
        if (client_addr_accepted.sin_family == AF_INET) // check it is IP4
        { // is a valid IP4 Address
          peer_addr_ptr = (struct sockaddr_in *)&(client_addr_accepted.sin_addr);
          // Convert IP address from network (binary) to textual form (Presentation (eg. dotted decimal))
          inet_ntop(AF_INET, peer_addr_ptr, address_buffer_v4, INET_ADDRSTRLEN);

          sprintf(label_name_of_peer, "%s@%s", "server", address_buffer_v4); // TODO:temp
        }

        is_connected = 1; // To indicate not longer trying to connect

        // TODO: Receive message behavior
//          key_choice = 'c';
//          demo = chat;
//          str_receive(connfd); // process the request // TODO:
//          exit(0);
        // Try now to connect for sending if not done so already (as by initial request)
        /*
         if(connfd < 0)
         {

         void * peer_addr_ptr = NULL;
         if (client_addr_accepted.sin_family == AF_INET) // check it is IP4
         { // is a valid IP4 Address
         peer_addr_ptr = (struct sockaddr_in *) &(client_addr_accepted.sin_addr);
         // Convert IP address from network (binary) to textual form (Presentation (eg. dotted decimal))
         inet_ntop(AF_INET, peer_addr_ptr, address_buffer_v4, INET_ADDRSTRLEN);
         //printf("%s IPv4 Address = %s\n", client_addr_accepted->sin_addr, address_buffer_v4);
         connfd = connect_to_peer_socket(address_buffer_v4, &server_addr_to_connect, PORT_LISTEN); // FIXME: assuming the port is thisx
         if(connfd > 0) // set nonblocking
         set_non_block(connfd); // FIXME: check that it's working

         }
         // TODO: suppor IPv6
         //else if (client_addr_accepted.sin_family == AF_INET6) // check it is IP6
         { // is a valid IP6 Address
         peer_addr_ptr = (struct sockaddr_in6 *) &(client_addr_accepted.sin6_addr);
         char address_buffer_v6[INET6_ADDRSTRLEN];
         // Convert IP address from network (binary) to textual form (Presentation (eg. dotted decimal))
         inet_ntop(AF_INET6, peer_addr_ptr, address_buffer_v6, INET6_ADDRSTRLEN);
         //printf("%s IPv6 Address = %s\n", if_addr_ptr->ifa_name, address_buffer_v6);
         connfd = connect_to_peer_socket(address_buffer_v6, &server_addr_to_connect);
         }

         }
         */
      }
    }

    if (demo)
    {
      //          caca_set_color_ansi(cv, CACA_DEFAULT, CACA_TRANSPARENT);
      //          caca_set_color_ansi(cv, CACA_LIGHTGRAY, CACA_BLACK);
      caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY);
      caca_clear_canvas(cv);

      if (key_choice == 'v')
        demo(cv, dp, vid_params, &win, connfd); // FIXME: using same socket
//        demo(cv, dp, vid_params, &win, recvfd);
//        demo(cv, dp, dev_name, img_width, img_height, connfd); // Using grab_messy

      if (key_choice == 'c')
//        demo(cv, dp, connfd, recvfd, &win, address_buffer_v4);
        demo(cv, dp, connfd, connfd, &win, address_buffer_v4); // Using same socket

      caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY);

      caca_clear_canvas(cv);
      caca_set_cursor(dp, 0); // Disable cursor
      caca_refresh_display(dp);
      demo = NULL;
      // TODO: add functionality
      /*
       caca_set_color_ansi(cv, CACA_LIGHTGRAY, CACA_BLACK);
       caca_draw_thin_box(cv, 1, 1, caca_get_canvas_width(cv) - 2, caca_get_canvas_height(cv) - 2);
       caca_printf(cv, 4, 1, "[%i.%i FPS]----", 1000000 / caca_get_display_time(dp),
       (10000000 / caca_get_display_time(dp)) % 10);
       caca_refresh_display(dp);
       */
    }
    /*
     else
     {
     // TODO: only temp debugging print label
     caca_clear_canvas(cv);
     caca_put_str(cv, 0, 0, label_name_of_peer);
     caca_refresh_display(dp);
     }
     */
  }

  // Clean up caca
  caca_free_display(dp);
  caca_free_canvas(cv);

  close_video_stream(vid_params);
  // Close sockets:
  close(connfd); // close socket
  close(recvfd); // close socket
  close(listenfd);

  return 0;
}

int chat(caca_canvas_t *cv, caca_display_t *dp, int sendfd, int recv_vid_fd, Window *win, char * peer_hostname)
{
//  int video_area = 2 * win->cols * win->rows; // A safe value for the receiving video buffer
  int video_area = MAXLINE; // FIXME: with less than this?

  char *video_in_buffer = NULL;
  video_in_buffer = malloc(video_area);
  unsigned int new_recv_video_bytes = 0; // Indicates when the size of a new video frame
  int area_of_interest = win->cols * win->video_lines;

  fd_set readset, writeset;
  int maxfd;
  char label_recv[MAXHOSTNAMELEN];
  int text_buffer_size = (MIN(win->cols, BUFFER_SIZE)) - 2; // Leave padding (margin)
  unsigned int row_offset_recv = 1 + win->video_lines;
  unsigned int row_offset_self = row_offset_recv + TEXT_ENTRIES + 1;
  unsigned int col_offset = 1;
  char * peer_username = "test_peer"; // TODO obtain the info
  caca_set_cursor(dp, 1); // Enable cursor

  struct timeval timeout;
  struct timeval *pto;

  // Timeout for select() is specified in argv[1]

  /*// NULL for blocking
   if (strcmp(argv[1], "-") == 0)
   {
   pto = NULL;                     // Infinite timeout
   }
   else
   */
  {
    pto = &timeout;
    timeout.tv_sec = 0; // seconds
    timeout.tv_usec = 5000; // FIXME microseconds
  }

  // ------------------ Fill argument structures  --------------------------------
  // Sender arguments
  struct thread_arg_struct chat_send_args;
  chat_send_args.socketfd = sendfd;
  chat_send_args.text_buffer_size = text_buffer_size;
  chat_send_args.row_offset = row_offset_self;
  chat_send_args.col_offset = col_offset;
  chat_send_args.cv = cv;
  chat_send_args.dp = dp;
  char sender_label[MAX_INPUT] = "Text chat (self) - press: Enter to send || Escape to stop\0";
  strcpy(chat_send_args.label, sender_label);

  // Receiver arguments
  struct thread_arg_struct chat_recv_args;
  chat_recv_args.socketfd = recv_vid_fd;
  chat_recv_args.text_buffer_size = text_buffer_size;
  chat_recv_args.row_offset = row_offset_recv;
  chat_recv_args.col_offset = col_offset;
  chat_recv_args.cv = cv;
  chat_recv_args.dp = dp;
  sprintf(label_recv, "%s@%s", peer_username, peer_hostname);
  strcpy(chat_recv_args.label, label_recv);
  // --------------------------------------------------------------------------------

  textentry entries_self[TEXT_ENTRIES];
  char * sendline = NULL; // Buffer to send through socket
  unsigned int i, e = TEXT_ENTRIES - 1, running = 1;
  unsigned int j, start, size;
  unsigned int newline_entered = 1; // Indicates when the return key has been pressed
  unsigned int send_succesful = 1; // 1: Indicates when the sending through socket succeeded

  char recline[MAXLINE] = ""; // Buffer to receive from socket
  textentry entries_recv[TEXT_ENTRIES];
  unsigned int new_recv_entry_bytes = 0; // Indicates when the size of a new peer line entry
  unsigned int first_time = 1; // Indicates when we are here for the first time

  caca_clear_canvas(cv);
  caca_set_color_ansi(cv, CACA_WHITE, CACA_BLUE);
  caca_fill_box(cv, col_offset, row_offset_recv, text_buffer_size, 1, ' ');
  caca_put_str(cv, col_offset, row_offset_recv, label_recv);
  caca_fill_box(cv, col_offset, row_offset_self, text_buffer_size, 1, ' ');
  caca_put_str(cv, col_offset, row_offset_self, sender_label);

  // NOTE: memory allocation should not happen inside switch statement!
  sendline = malloc(MAXLINE);

  row_offset_self++;
  row_offset_recv++;

  for (i = 0; i < TEXT_ENTRIES; i++)
  {
    entries_self[i].buffer[0] = 0;
    entries_self[i].size = 0;
    entries_self[i].cursor = 0;
    entries_self[i].changed = 1;

    entries_recv[i].buffer[0] = 0;
    entries_recv[i].size = 0;
    entries_recv[i].cursor = 0;
    entries_recv[i].changed = 1;
  }

  maxfd = MAXFD(fileno(stdin), sendfd) + 1; // FIXME: here is the problem? chicken vs egg case
//      maxfd = MAXFD(fileno(stdin), recv_vid_fd) +1;

  caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY); // Reset background color

  while (running)
  {
    caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY); // Reset background color

    if (sendfd > 0)
    {
      // Important:
      // Since these structures are modified by the call,
      // we must ensure that we reinitialize them if we are repeatedly calling select() from within a loop.)
      FD_ZERO(&readset);
//      FD_ZERO(&writeset);
      FD_SET(recv_vid_fd, &readset);
//      FD_SET(sendfd, &readset);
//      FD_SET(sendfd, &writeset); // FIXME??? should I use 2 sockets?

//      if ( select( maxfd, &readset, &writeset, NULL, pto ) > 0 )
      if (select(maxfd, &readset, NULL, NULL, pto) > 0)
      {
        /*
         if ( FD_ISSET(sendfd, &readset))
         {
         memset(recline, '\0', MAXLINE);
         new_recv_entry_bytes = recv(sendfd, recline, MAXLINE, 0);
         }
         */

        if (FD_ISSET(recv_vid_fd, &readset))
        {
          bzero(video_in_buffer, video_area);
          new_recv_video_bytes = recv(recv_vid_fd, video_in_buffer, video_area, 0);
        }
        else
          new_recv_video_bytes = 0;

        /*
         if ( FD_ISSET(sendfd, &writeset))
         {
         if (entries_self[e].size > 0 && newline_entered == 1 && send_succesful == 0)
         {
         int sent_bytes = send(sendfd, sendline, entries_self[e].size, 0);
         if(sent_bytes >= 0)
         {
         send_succesful = 1;
         }
         }
         }
         */
      }
#ifdef DEVELOP
      else
      {
        char msg_select_ready[MAXLINE] = "Select is NOT ready";
        caca_put_str(cv, 0, row_offset_self + TEXT_ENTRIES + 1, msg_select_ready);
      }
#endif
    }

    // Update peer (received) video
    if (new_recv_video_bytes > 0)
    {
      ssize_t imported_bytes = caca_import_area_from_memory(cv, 0, 0, video_in_buffer, new_recv_video_bytes, win->caca_format); // TODO: pass format from window (or struct)
      caca_fill_box(cv, col_offset, row_offset_recv - 1, text_buffer_size, 1, ' ');
      char import_label[100] = "";
      sprintf(import_label, "Received= %d bytes, Imported %d bytes of caca video (AOI:%d x %d)", new_recv_video_bytes,
              imported_bytes, win->cols, win->video_lines);
      caca_put_str(cv, col_offset, row_offset_recv - 1, import_label);
    }
    new_recv_video_bytes = 0; // always reset (because we must have handled the buffer already)

    // Update peer (received) entries
    if (new_recv_entry_bytes > 0 || first_time == 1)
    {
      first_time = 0; // Reset flag
      for (i = 0; i < TEXT_ENTRIES - 1; i++)
      {
        caca_set_color_ansi(cv, CACA_YELLOW, CACA_MAGENTA);
        caca_fill_box(cv, col_offset, i + row_offset_recv, text_buffer_size, 1, ' ');

        // Clear top line and put contents from line below:
        memset(entries_recv[i].buffer, '\0', entries_recv[i].size * 4); // *4 because using uint32_t
        memmove(entries_recv[i].buffer, entries_recv[i + 1].buffer, (entries_recv[i + 1].size) * 4);
        entries_recv[i].size = entries_recv[i + 1].size;
        entries_recv[i].cursor = 0;

        start = 0;
        size = entries_recv[i].size;

        for (j = 0; j < size; j++)
        {
          caca_put_char(cv, col_offset + j, i + row_offset_recv, entries_recv[i].buffer[start + j]);
        }
        entries_recv[i].changed = 0;
      }
      caca_fill_box(cv, col_offset, e + row_offset_recv, text_buffer_size, 1, ' ');
      if (new_recv_entry_bytes > 0)
        caca_set_attr(cv, CACA_BLINK); // TODO: not working yet
      start = 0;
      entries_recv[e].size = new_recv_entry_bytes;
      entries_recv[e].cursor = 0;
      size = entries_recv[e].size;
      memset(entries_recv[e].buffer, '\0', size * 4); // *4 because using uint32_t
      for (j = 0; j < size; j++)
      {
        entries_recv[e].buffer[start + j] = (uint32_t)recline[start + j];
        caca_put_char(cv, col_offset + j, e + row_offset_recv, entries_recv[e].buffer[start + j]);
      }
      caca_unset_attr(cv, CACA_BLINK); // TODO: not working yet
    }
    new_recv_entry_bytes = 0; // always reset (because we must have handled the entry already)

    // Update self (sender) entries
    if (newline_entered == 1)
    {
      for (i = 0; i < TEXT_ENTRIES - 1; i++)
      {

        caca_set_color_ansi(cv, CACA_BLACK, CACA_CONIO_CYAN);
        caca_fill_box(cv, col_offset, i + row_offset_self, text_buffer_size, 1, ' ');

        // Clear top line and put contents from line below:
        memset(entries_self[i].buffer, '\0', entries_self[i].size * 4); // *4 because using uint32_t
        memmove(entries_self[i].buffer, entries_self[i + 1].buffer, (entries_self[i + 1].size) * 4);
        entries_self[i].size = entries_self[i + 1].size;
        entries_self[i].cursor = 0;

        start = 0;
        size = entries_self[i].size;

        for (j = 0; j < size; j++)
        {
          caca_put_char(cv, col_offset + j, i + row_offset_self, entries_self[i].buffer[start + j]);
        }
        entries_self[i].changed = 0;
      }
      // Reset editable textbox (last line)
      caca_set_color_ansi(cv, CACA_WHITE, CACA_DARKGRAY);
      caca_fill_box(cv, col_offset, i + row_offset_self, text_buffer_size, 1, ' ');
      memset(entries_self[i].buffer, '\0', BUFFER_SIZE);
      entries_self[i].size = 0;
      entries_self[i].cursor = 0;
    }
    else // Only update last line if changed
    {
      if (entries_self[e].changed == 1)
      {
        caca_set_color_ansi(cv, CACA_WHITE, CACA_DARKGRAY);
        caca_fill_box(cv, col_offset, e + row_offset_self, text_buffer_size, 1, ' ');

        start = 0;
        size = entries_self[e].size;
        for (j = 0; j < size; j++)
        {
          caca_put_char(cv, col_offset + j, e + row_offset_self, entries_self[e].buffer[start + j]);
        }
      }
    }
    // always reset flags after updating entries
    entries_self[e].changed = 0;
    newline_entered = 0;

    // Put the cursor on the active textentry
    caca_gotoxy(cv, col_offset + entries_self[e].cursor, e + row_offset_self);

    caca_refresh_display(dp);

    caca_event_t ev;

    //  timeout A timeout value in microseconds, -1 for blocking behaviour
    int timeout = 1000;
    if (caca_get_event(dp, CACA_EVENT_KEY_PRESS, &ev, timeout) == 0)
      continue;

    switch (caca_get_event_key_ch(&ev))
    {
      case CACA_KEY_ESCAPE:
        running = 0;
        // Free string buffers
        break;
        //case CACA_KEY_TAB:
      case CACA_KEY_RETURN:
      {
        if (send_succesful == 1 && entries_self[e].size > 0) // Check if we can accept a new entry
        {
          // Send line through socket
          memset(sendline, '\0', entries_self[e].size + 1);

          int j;
          for (j = 0; j < entries_self[e].size; j++)
          {
            sendline[j] = (char)entries_self[e].buffer[j];
          }

          // Set flags
          newline_entered = 1;
          entries_self[e].changed = 1;
          send_succesful = 0;
        }
        break;
      }
      case CACA_KEY_HOME:
        entries_self[e].cursor = 0;
        break;
      case CACA_KEY_END:
        entries_self[e].cursor = entries_self[e].size;
        break;
      case CACA_KEY_LEFT:
        if (entries_self[e].cursor)
          entries_self[e].cursor--;
        break;
      case CACA_KEY_RIGHT:
        if (entries_self[e].cursor < entries_self[e].size)
          entries_self[e].cursor++;
        break;
      case CACA_KEY_DELETE:
        if (entries_self[e].cursor < entries_self[e].size)
        {
          memmove(entries_self[e].buffer + entries_self[e].cursor, entries_self[e].buffer + entries_self[e].cursor + 1,
                  (entries_self[e].size - entries_self[e].cursor + 1) * 4);
          entries_self[e].size--;
          entries_self[e].changed = 1;
        }
        break;
      case CACA_KEY_BACKSPACE:
        if (entries_self[e].cursor)
        {
          memmove(entries_self[e].buffer + entries_self[e].cursor - 1, entries_self[e].buffer + entries_self[e].cursor,
                  (entries_self[e].size - entries_self[e].cursor) * 4);
          entries_self[e].size--;
          entries_self[e].cursor--;
          entries_self[e].changed = 1;
        }
        break;
      default:
        if (entries_self[e].size < BUFFER_SIZE)
        {
          memmove(entries_self[e].buffer + entries_self[e].cursor + 1, entries_self[e].buffer + entries_self[e].cursor,
                  (entries_self[e].size - entries_self[e].cursor) * 4);
          entries_self[e].buffer[entries_self[e].cursor] = caca_get_event_key_utf32(&ev);
          entries_self[e].size++;
          entries_self[e].cursor++;
          entries_self[e].changed = 1;
        }
        break;
    }

  }

  free(sendline);
  free(video_in_buffer);

  return 0;
}

int grab(caca_canvas_t *cv, caca_display_t *dp, video_params * vid_params, Window *win, int sockfd)
{
  fd_set fds;
  fd_set sock_writeset;
  int sock_maxfd;
  struct timeval tv;
  struct timeval socket_timeout;
  struct timeval *pto;
  unsigned int r, rows, cols;
  void *export;
  size_t exported_bytes;
  struct image *im; // Image struct to use in caca
  int quit = 0;

  pto = &socket_timeout;
  socket_timeout.tv_sec = 0; // seconds
  socket_timeout.tv_usec = 90000; // microseconds FIXME

  // Allowable exportable region dimensions (so export doesn't fail)
  if (win->cols > vid_params->cv_cols)
    cols = vid_params->cv_cols;
  else
    cols = win->cols;

  if (win->rows > vid_params->cv_rows)
    rows = vid_params->cv_rows;
  else
    rows = win->rows;

  // Go (main loop)
  while (!quit)
  {
    caca_event_t ev;

    while (caca_get_event(dp, CACA_EVENT_ANY, &ev, 0))
    {
      if (caca_get_event_type(&ev) & CACA_EVENT_KEY_PRESS)
      {
        switch (caca_get_event_key_ch(&ev))
        {
          case 'q':
          case 'Q':
          case CACA_KEY_ESCAPE:
            quit = 1;
            break;
        }

      }
    }

    if (!quit)
    {
      do
      {
        FD_ZERO(&fds);
        FD_SET(vid_params->v4l_fd, &fds);

        // Timeout.
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select(vid_params->v4l_fd + 1, &fds, NULL, NULL, &tv);
      } while ((r == -1 && (errno = EINTR)));

      if (r == -1)
      {
        perror("select");
        return errno;
      }

      CLEAR(vid_params->buf);
      vid_params->buf.type = vid_params->type;
      vid_params->buf.memory = vid_params->memory;
      xioctl(vid_params->v4l_fd, VIDIOC_DQBUF, &(vid_params->buf));

      //-----------------------------------------
      im = load_image_from_V4L_buffer(&(vid_params->fmt), &(vid_params->buffers[vid_params->buf.index]),
                                      vid_params->buf.bytesused);
      if (!im)
      {
        fprintf(stderr, "grab: Unable to load caca from V4L\n");
        caca_free_canvas(cv);
        return 1;
      }

      caca_clear_canvas(cv);
      if (caca_set_dither_algorithm(im->dither, vid_params->caca_dither ? vid_params->caca_dither : "fstein"))
      {
        fprintf(stderr, "grab: Can't dither image with algorithm '%s'\n", vid_params->caca_dither);
        unload_image(im);
        caca_free_canvas(cv);
        return -1;
      }

      if (vid_params->caca_brightness != -1)
        caca_set_dither_brightness(im->dither, vid_params->caca_brightness);
      if (vid_params->caca_contrast != -1)
        caca_set_dither_contrast(im->dither, vid_params->caca_contrast);
      if (vid_params->caca_gamma != -1)
        caca_set_dither_gamma(im->dither, vid_params->caca_gamma);

      caca_dither_bitmap(cv, 0, 0, vid_params->cv_cols, vid_params->cv_rows, im->dither, im->pixels);

      //unload_image(im); // Enable it if using memcpy in the loading

      if (sockfd > 0)
      {
        sock_maxfd = MAXFD(fileno(stdin), sockfd) + 1; // FIXME: here is the problem? chicken vs egg case

        // Important:
        // Since these structures are modified by the call,
        // we must ensure that we reinitialize them if we are repeatedly calling select() from within a loop.)
        FD_ZERO(&sock_writeset);
        FD_SET(sockfd, &sock_writeset);

        if (select(sock_maxfd, NULL, &sock_writeset, NULL, pto) > 0)
        {

          if (FD_ISSET(sockfd, &sock_writeset))
          {
            exported_bytes = 0; // clear number of bytes exported (assuming they all went through)
            // This is what needs to be sent through the socket
            //      export = caca_export_canvas_to_memory(cv, vid_params->caca_format ? format : "ansi", &exported_bytes);
//            export = caca_export_area_to_memory(cv, 0, 0, cols, rows, vid_params->caca_format ? vid_params->caca_format : "ansi", &exported_bytes);
            export = caca_export_area_to_memory(cv, 0, 0, cols, rows,
                                                vid_params->caca_format ? vid_params->caca_format : "caca",
                                                &exported_bytes);
            if (!export)
            {
              fprintf(stderr, "grab: Can't export to format '%s'\n", vid_params->caca_format);
            }
            else
            {
              int sent_bytes = send(sockfd, export, exported_bytes, 0);

              char sent_label[100] = "";
              sprintf(sent_label, "Exported = %d bytes, Sent %d bytes (%dx%d) frame", exported_bytes, sent_bytes, cols,
                      rows);
              caca_put_str(cv, 0, rows + 3, sent_label);
              free(export);
            }
#ifdef DEVELOP
            caca_get_event(dp, CACA_EVENT_KEY_PRESS, NULL, -1);
#endif
          }
        }
      }

      /* TODO: delete, just for testing
      export = caca_export_area_to_memory(cv, 0, 0, cols, rows, vid_params->caca_format ? vid_params->caca_format : "utf8",
                                              &exported_bytes);
      if (!export)
      {
        fprintf(stderr, "grab: Can't export to format '%s'\n", vid_params->caca_format);
      }
      else
      {
        int imported_bytes = caca_import_area_from_memory(cv, 0, rows+10, export, exported_bytes, vid_params->caca_format ? vid_params->caca_format : "caca");

        char exp_imp_label[100] = "";
        sprintf(exp_imp_label, "Exported = %d bytes, Imported %d bytes (%dx%d) frame", exported_bytes, imported_bytes, cols,
                rows);
        caca_put_str(cv, 0, rows + 3, exp_imp_label);

        free(export);
      }
      */

      // TODO: Nice display margin:
      // ------------------------------------------------------------------
//       caca_set_color_ansi(cv, CACA_LIGHTGRAY, CACA_BLACK);
//       caca_draw_thin_box(cv, 1, 1, caca_get_canvas_width(cv) - 2, caca_get_canvas_height(cv) - 2);
//       caca_printf(cv, 4, 1, "[%i.%i FPS]----", 1000000 / caca_get_display_time(dp),
//       (10000000 / caca_get_display_time(dp)) % 10);
      // ------------------------------------------------------------------

      caca_refresh_display(dp);

      xioctl(vid_params->v4l_fd, VIDIOC_QBUF, &(vid_params->buf));
    }
  }

  return 0;
}

static void grab_messy(caca_canvas_t *cv, caca_display_t *dp, char *dev_name, int img_width, int img_height, int sockfd)
{
  struct v4l2_format fmt;
  struct v4l2_buffer buf;
  struct v4l2_requestbuffers req;
  enum v4l2_buf_type type;
  fd_set fds;
  struct timeval tv;
  int r, fd = -1;
  unsigned int i, n_buffers;
  struct buffer *buffers;

  //-----------------------------------------
  // caca test: from img2txt
  void *export;
  char *format = "ansi";
  size_t exported_bytes;
  unsigned int font_width = 6, font_height = 10;
  float aspect_ratio = ((float)img_width / (float)img_height) * ((float)font_height / font_width);
  unsigned int lines = caca_get_canvas_height(cv);
  unsigned int cols = (unsigned int)(aspect_ratio * (float)lines);

  char *dither = "fstein";
  float gamma = -1, brightness = -1, contrast = -1;
  struct image *im; // Image struct to use in caca
  // libcaca context
  //-----------------------------------------

  fd = v4l2_open(dev_name, O_RDWR | O_NONBLOCK, 0);
  if (fd < 0)
  {
    perror("Cannot open device");
    exit(EXIT_FAILURE);
  }

  CLEAR(fmt);
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = img_width;
  fmt.fmt.pix.height = img_height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24; // The simplest 3-color channels 8bpp format
  fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
  xioctl(fd, VIDIOC_S_FMT, &fmt);
  if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24)
  {
    printf("Libv4l didn't accept RGB24 format. Can't proceed.\n");
    exit(EXIT_FAILURE);
  }
  if ((fmt.fmt.pix.width != 640) || (fmt.fmt.pix.height != 480))
    printf("Warning: driver is sending image at %dx%d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);

  // --------------------------------------------------------------------------------
  // Assume a 6×10 font
  if (!cols && !lines)
  {
    cols = font_width * font_height;
//          lines = cols * im->h * font_width / im->w / font_height;
    lines = cols * fmt.fmt.pix.height * font_width / fmt.fmt.pix.width / font_height;
  }
  else if (cols && !lines)
  {
//          lines = cols * im->h * font_width / im->w / font_height;
    lines = cols * fmt.fmt.pix.height * font_width / fmt.fmt.pix.width / font_height;
  }
  else if (!cols && lines)
  {
//          cols = lines * im->w * font_height / im->h / font_width;
    cols = lines * fmt.fmt.pix.width * font_height / fmt.fmt.pix.height / font_width;
  }

  caca_set_canvas_size(cv, cols, lines);
  // --------------------------------------------------------------------------------

  CLEAR(req);
  req.count = 2;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  xioctl(fd, VIDIOC_REQBUFS, &req);

  buffers = calloc(req.count, sizeof(*buffers)); // A double buffer (there is only 2 requests)

  for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
  {
    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = n_buffers;

    xioctl(fd, VIDIOC_QUERYBUF, &buf);

    buffers[n_buffers].length = buf.length;
    buffers[n_buffers].start = v4l2_mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

    if (MAP_FAILED == buffers[n_buffers].start)
    {
      perror("mmap");
      exit(EXIT_FAILURE);
    }
  }

  for (i = 0; i < n_buffers; ++i)
  {
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    xioctl(fd, VIDIOC_QBUF, &buf);
  }
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  xioctl(fd, VIDIOC_STREAMON, &type);

  // Go (main loop)
  int quit = 0;
  while (!quit)
  {
    caca_event_t ev;

    while (caca_get_event(dp, CACA_EVENT_ANY, &ev, 0))
    {
      if (caca_get_event_type(&ev) & CACA_EVENT_KEY_PRESS)
      {
        switch (caca_get_event_key_ch(&ev))
        {
          case 'q':
          case 'Q':
          case CACA_KEY_ESCAPE:
            quit = 1;
            break;
        }

      }
    }

    if (!quit)
    {
      do
      {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        // Timeout.
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select(fd + 1, &fds, NULL, NULL, &tv);
      } while ((r == -1 && (errno = EINTR)));

      if (r == -1)
      {
        perror("select");
//        return errno;
      }

      CLEAR(buf);
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      xioctl(fd, VIDIOC_DQBUF, &buf);

      //-----------------------------------------
      im = load_image_from_V4L_buffer(&fmt, &buffers[buf.index], buf.bytesused);
      if (!im)
      {
        fprintf(stderr, "grab: Unable to load caca from V4L\n");
        caca_free_canvas(cv);
//        return 1;
      }

      caca_clear_canvas(cv);
      if (caca_set_dither_algorithm(im->dither, dither ? dither : "fstein"))
      {
        fprintf(stderr, "grab: Can't dither image with algorithm '%s'\n", dither);
        unload_image(im);
        caca_free_canvas(cv);
//        return -1;
      }

      if (brightness != -1)
        caca_set_dither_brightness(im->dither, brightness);
      if (contrast != -1)
        caca_set_dither_contrast(im->dither, contrast);
      if (gamma != -1)
        caca_set_dither_gamma(im->dither, gamma);

      caca_dither_bitmap(cv, 0, 0, cols, lines, im->dither, im->pixels);

      //unload_image(im); // Enable it if using memcpy in the loading

      // This is what needs to be sent through the socket
//      /*
//      export = caca_export_canvas_to_memory(cv, format ? format : "ansi", &exported_bytes);
      export = caca_export_area_to_memory(cv, 0, 0, cols, lines, format ? format : "ansi", &exported_bytes);
      if (!export)
      {
        fprintf(stderr, "grab: Can't export to format '%s'\n", format);
      }
      else
      {
//        fwrite(export, len, 1, stdout);
        char * recvline;
        int nahhh = send_receive_data_through_socket(sockfd, export, recvline, exported_bytes);

        free(export);
      }
//        /*

      // FIXME: Nice display margin:
      /*
       // ------------------------------------------------------------------
       caca_set_color_ansi(cv, CACA_LIGHTGRAY, CACA_BLACK);
       caca_draw_thin_box(cv, 1, 1, caca_get_canvas_width(cv) - 2, caca_get_canvas_height(cv) - 2);
       caca_printf(cv, 4, 1, "[%i.%i FPS]----", 1000000 / caca_get_display_time(dp),
       (10000000 / caca_get_display_time(dp)) % 10);
       // ------------------------------------------------------------------
       */
      caca_refresh_display(dp);

      xioctl(fd, VIDIOC_QBUF, &buf);

    }
  }
//  caca_free_canvas(cv);

  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  xioctl(fd, VIDIOC_STREAMOFF, &type);
  for (i = 0; i < n_buffers; ++i)
    v4l2_munmap(buffers[i].start, buffers[i].length);
  v4l2_close(fd);

//  return 0;
}

int set_video(video_params *vid_params, char *dev_name, Window *win, int img_width, int img_height)
{
  unsigned int i, n_buffers;

  // We don't know yet if setting the video device will succeed, so set the "not okay" flag initially
  vid_params->is_ok = 0;
  vid_params->is_on = 0; // Streaming is not on yet

  // ------ Arbitrary settings:
  vid_params->caca_format = win->caca_format;
  vid_params->caca_dither = "fstein";
  vid_params->number_of_buffers = 2; // Arbitrary double buffer
  vid_params->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  vid_params->memory = V4L2_MEMORY_MMAP;
  unsigned int font_width = 6, font_height = 10; // FIXME: purely arbitrary based on the usual ansi format for terminals
  // -------------------------------------------------------

  vid_params->img_width = img_width;
  vid_params->img_height = img_height;
  vid_params->aspect_ratio = ((float)img_width / (float)img_height) * ((float)font_height / font_width);
  vid_params->cv_rows = win->video_lines;
  vid_params->cv_cols = (unsigned int)(vid_params->aspect_ratio * (float)vid_params->cv_rows);

  vid_params->caca_gamma = -1;
  vid_params->caca_brightness = -1;
  vid_params->caca_contrast = -1;

  strcpy(vid_params->dev_name, dev_name);
  vid_params->v4l_fd = v4l2_open(vid_params->dev_name, O_RDWR | O_NONBLOCK, 0);
  if (vid_params->v4l_fd < 0)
  {
    perror("set_video: Cannot open video device\n");
    exit(EXIT_FAILURE);
  }

  CLEAR(vid_params->fmt);
  vid_params->fmt.type = vid_params->type;
  vid_params->fmt.fmt.pix.width = img_width;
  vid_params->fmt.fmt.pix.height = img_height;
  vid_params->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24; // The simplest 3-color channels 8bpp format
  vid_params->fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
  xioctl(vid_params->v4l_fd, VIDIOC_S_FMT, &(vid_params->fmt));
  if (vid_params->fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24)
  {
    perror("set_video: Libv4l didn't accept RGB24 format. Can't proceed.\n");
    exit(EXIT_FAILURE);
  }
  if ((vid_params->fmt.fmt.pix.width != img_width) || (vid_params->fmt.fmt.pix.height != img_height))
    printf("Warning: driver is sending image at %dx%d\n", vid_params->fmt.fmt.pix.width,
           vid_params->fmt.fmt.pix.height);

  CLEAR(vid_params->req);
  vid_params->req.count = vid_params->number_of_buffers;
  vid_params->req.type = vid_params->type;
  vid_params->req.memory = vid_params->memory;
  xioctl(vid_params->v4l_fd, VIDIOC_REQBUFS, &(vid_params->req));

  vid_params->buffers = calloc(vid_params->req.count, sizeof(*(vid_params->buffers))); // A double buffer (there is only 2 requests)

  for (n_buffers = 0; n_buffers < vid_params->req.count; ++n_buffers)
  {
    CLEAR(vid_params->buf);

    vid_params->buf.type = vid_params->type;
    vid_params->buf.memory = vid_params->memory;
    vid_params->buf.index = n_buffers;

    xioctl(vid_params->v4l_fd, VIDIOC_QUERYBUF, &(vid_params->buf));

    vid_params->buffers[n_buffers].length = vid_params->buf.length;
    vid_params->buffers[n_buffers].start = v4l2_mmap(NULL, vid_params->buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                     vid_params->v4l_fd, vid_params->buf.m.offset);

    if (MAP_FAILED == vid_params->buffers[n_buffers].start)
    {
      perror("set_video: mmap");
      exit(EXIT_FAILURE);
    }
  }

  for (i = 0; i < n_buffers; ++i)
  {
    CLEAR(vid_params->buf);
    vid_params->buf.type = vid_params->type;
    vid_params->buf.memory = vid_params->memory;
    vid_params->buf.index = i;
    xioctl(vid_params->v4l_fd, VIDIOC_QBUF, &(vid_params->buf));
  }

  // If we got passed these tests and settings, then the video device is on and readily streaming
  vid_params->is_ok = 1;

  // Last, turn the stream on
  turn_video_stream_on(vid_params);

  return vid_params->is_ok;
}

int turn_video_stream_on(video_params *vid_params)
{
  vid_params->is_on = 0; // Reset on/off flag

  if (vid_params->is_ok == 1)
  {
    int status = xioctl(vid_params->v4l_fd, VIDIOC_STREAMON, &(vid_params->type));
    if (status == 0)
      vid_params->is_on = 1; // Set flag indicating streaming is on
  }

  return vid_params->is_on;
}

int turn_video_stream_off(video_params *vid_params)
{
  vid_params->is_on = -1; // Reset on/off flag to the undefined state

  if (vid_params->is_ok == 1)
  {
    int status = xioctl(vid_params->v4l_fd, VIDIOC_STREAMOFF, &(vid_params->type));
    if (status == 0)
      vid_params->is_on = 0; // Set flag indicating streaming is off
  }

  return vid_params->is_on;
}

void close_video_stream(video_params *vid_params)
{
  unsigned int i;

  if (vid_params->is_ok == 1)
  {
    // Turn off stream just in case it slipped through the code
    xioctl(vid_params->v4l_fd, VIDIOC_STREAMOFF, &(vid_params->type));
  }

  for (i = 0; i < vid_params->number_of_buffers; ++i)
    v4l2_munmap(vid_params->buffers[i].start, vid_params->buffers[i].length);

  v4l2_close(vid_params->v4l_fd);

  free(vid_params);
}

int xioctl(int fd, int request, void *arg)
{
  int r;

  do
  {
    r = v4l2_ioctl(fd, request, arg);
  } while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));

  if (r == -1)
  {
    fprintf(stderr, "error %d, %s\n", errno, strerror(errno));
  }
  return r;
}

void set_window(int fd, unsigned short video_lines, Window *win)
{
  struct winsize size;

  if (ioctl(fd, TIOCGWINSZ, &size) < 0)
  {
    perror("TIOCGWINSZ error");
    return;
  }
  win->rows = size.ws_row;
  win->cols = size.ws_col;
  win->video_lines = video_lines;
  win->video_cols = video_lines; // Just a safe square value (temporary)
  win->caca_format = "caca"; // Arbitrary format for caca export/import
}

// ******************** THREADS *************************
void * send_chat(void * arguments)
{
  struct thread_arg_struct *args = (struct thread_arg_struct *)arguments;
  int sendfd = args->socketfd;
  caca_canvas_t *cv = args->cv;
  caca_display_t *dp = args->dp;
  int text_buffer_size = args->text_buffer_size;
  textentry entries_self[TEXT_ENTRIES];
  char * sendline = NULL; // Buffer to send through socket
  unsigned int i, e = TEXT_ENTRIES - 1, running = 1;
  unsigned int j, start, size;
  unsigned int newline_entered = 1; // Indicates when the return key has been pressed
  unsigned int row_offset_self = args->row_offset;
  unsigned int col_offset = 1;

  caca_set_color_ansi(cv, CACA_WHITE, CACA_BLUE);
  caca_fill_box(cv, col_offset, row_offset_self, text_buffer_size, 1, ' ');
  caca_put_str(cv, col_offset, row_offset_self, args->label);

  // NOTE: memory allocation should not happen inside switch statement!
  sendline = malloc(MAXLINE);

  row_offset_self++;

  for (i = 0; i < TEXT_ENTRIES; i++)
  {
    entries_self[i].buffer[0] = 0;
    entries_self[i].size = 0;
    entries_self[i].cursor = 0;
    entries_self[i].changed = 1;
  }

  while (running)
  {
    caca_event_t ev;

    // Update self (sender) entries
    if (newline_entered == 1)
    {
      for (i = 0; i < TEXT_ENTRIES - 1; i++)
      {

        caca_set_color_ansi(cv, CACA_BLACK, CACA_CONIO_CYAN);
        caca_fill_box(cv, col_offset, i + row_offset_self, text_buffer_size, 1, ' ');

        // Clear top line and put contents from line below:
        memset(entries_self[i].buffer, '\0', entries_self[i].size * 4); // *4 because using uint32_t
        memmove(entries_self[i].buffer, entries_self[i + 1].buffer, (entries_self[i + 1].size) * 4);
        entries_self[i].size = entries_self[i + 1].size;
        entries_self[i].cursor = 0;

        start = 0;
        size = entries_self[i].size;

        for (j = 0; j < size; j++)
        {
          caca_put_char(cv, col_offset + j, i + row_offset_self, entries_self[i].buffer[start + j]);
        }
        entries_self[i].changed = 0;
      }
      // Reset editable textbox (last line)
      caca_set_color_ansi(cv, CACA_WHITE, CACA_DARKGRAY);
      caca_fill_box(cv, col_offset, i + row_offset_self, text_buffer_size, 1, ' ');
      memset(entries_self[i].buffer, '\0', BUFFER_SIZE);
      entries_self[i].size = 0;
      entries_self[i].cursor = 0;
    }
    else // Only update last line if changed
    {
      if (entries_self[e].changed == 1)
      {
        caca_set_color_ansi(cv, CACA_WHITE, CACA_DARKGRAY);
        caca_fill_box(cv, col_offset, e + row_offset_self, text_buffer_size, 1, ' ');

        start = 0;
        size = entries_self[e].size;
        for (j = 0; j < size; j++)
        {
          caca_put_char(cv, col_offset + j, e + row_offset_self, entries_self[e].buffer[start + j]);
        }
      }
    }
    entries_self[e].changed = 0;
    newline_entered = 0; // always reset flag

    // Put the cursor on the active textentry
    caca_gotoxy(cv, col_offset + entries_self[e].cursor, e + row_offset_self);

//      int s = pthread_mutex_lock(&mtx);
    caca_refresh_display(dp);
//      s = pthread_mutex_unlock(&mtx);

    if (caca_get_event(dp, CACA_EVENT_KEY_PRESS, &ev, -1) == 0)
      continue;

    switch (caca_get_event_key_ch(&ev))
    {
      case CACA_KEY_ESCAPE:
        running = 0;
        // Free string buffers
        break;
        //case CACA_KEY_TAB:
      case CACA_KEY_RETURN:
      {
        // Send line through socket
        memset(sendline, '\0', entries_self[e].size + 1);

        int j;
        for (j = 0; j < entries_self[e].size; j++)
        {
          sendline[j] = (char)entries_self[e].buffer[j];
        }

        if (entries_self[e].size > 0 && sendline != NULL )
        {
          int send_status = send(sendfd, sendline, strlen(sendline), 0); // FIXME: correct with truth
        }

        newline_entered = 1;
        entries_self[e].changed = 1;
        break;
      }
      case CACA_KEY_HOME:
        entries_self[e].cursor = 0;
        break;
      case CACA_KEY_END:
        entries_self[e].cursor = entries_self[e].size;
        break;
      case CACA_KEY_LEFT:
        if (entries_self[e].cursor)
          entries_self[e].cursor--;
        break;
      case CACA_KEY_RIGHT:
        if (entries_self[e].cursor < entries_self[e].size)
          entries_self[e].cursor++;
        break;
      case CACA_KEY_DELETE:
        if (entries_self[e].cursor < entries_self[e].size)
        {
          memmove(entries_self[e].buffer + entries_self[e].cursor, entries_self[e].buffer + entries_self[e].cursor + 1,
                  (entries_self[e].size - entries_self[e].cursor + 1) * 4);
          entries_self[e].size--;
          entries_self[e].changed = 1;
        }
        break;
      case CACA_KEY_BACKSPACE:
        if (entries_self[e].cursor)
        {
          memmove(entries_self[e].buffer + entries_self[e].cursor - 1, entries_self[e].buffer + entries_self[e].cursor,
                  (entries_self[e].size - entries_self[e].cursor) * 4);
          entries_self[e].size--;
          entries_self[e].cursor--;
          entries_self[e].changed = 1;
        }
        break;
      default:
        if (entries_self[e].size < BUFFER_SIZE)
        {
          memmove(entries_self[e].buffer + entries_self[e].cursor + 1, entries_self[e].buffer + entries_self[e].cursor,
                  (entries_self[e].size - entries_self[e].cursor) * 4);
          entries_self[e].buffer[entries_self[e].cursor] = caca_get_event_key_utf32(&ev);
          entries_self[e].size++;
          entries_self[e].cursor++;
          entries_self[e].changed = 1;
        }
        break;
    }

  }

  // NOTE: memory handling should not happen inside switch statement!
  free(sendline); // Release buffer memory
  pthread_exit(NULL );
  return NULL ;
}

void * receive_chat(void * arguments)
{
  struct thread_arg_struct *args = (struct thread_arg_struct *)arguments;
  int recvfd = args->socketfd;
  caca_canvas_t *cv = args->cv;
  caca_display_t *dp = args->dp;
  int text_buffer_size = args->text_buffer_size;
  char recline[MAXLINE] = ""; // Buffer to receive from socket
  textentry entries_recv[TEXT_ENTRIES];
  unsigned int i, e = TEXT_ENTRIES - 1, running = 1;
  unsigned int j, start, size;
  unsigned int row_offset_recv = args->row_offset;
  unsigned int col_offset = 1;
  unsigned int new_recv_entry_bytes = 0; // Indicates when the size of a new peer line entry
  unsigned int first_time = 1; // Indicates when we are here for the first time

  caca_set_color_ansi(cv, CACA_WHITE, CACA_BLUE);
  caca_fill_box(cv, col_offset, row_offset_recv, text_buffer_size, 1, ' ');
  caca_put_str(cv, col_offset, row_offset_recv, args->label);

  // NOTE: memory allocation should not happen inside switch statement!
//  recline = malloc(MAXLINE); // FIXME?

  row_offset_recv++;

  for (i = 0; i < TEXT_ENTRIES; i++)
  {
    entries_recv[i].buffer[0] = 0;
    entries_recv[i].size = 0;
    entries_recv[i].cursor = 0;
    entries_recv[i].changed = 1;
  }

  while (running)
  {
    caca_event_t ev;

    // Update peer (received) entries
    if (new_recv_entry_bytes > 0 || first_time == 1)
    {
      first_time = 0; // Reset flag
      for (i = 0; i < TEXT_ENTRIES - 1; i++)
      {
        caca_set_color_ansi(cv, CACA_BLACK, CACA_MAGENTA);
        caca_fill_box(cv, col_offset, i + row_offset_recv, text_buffer_size, 1, ' ');

        // Clear top line and put contents from line below:
        memset(entries_recv[i].buffer, '\0', entries_recv[i].size * 4); // *4 because using uint32_t
        memmove(entries_recv[i].buffer, entries_recv[i + 1].buffer, (entries_recv[i + 1].size) * 4);
        entries_recv[i].size = entries_recv[i + 1].size;
        entries_recv[i].cursor = 0;

        start = 0;
        size = entries_recv[i].size;

        for (j = 0; j < size; j++)
        {
          caca_put_char(cv, col_offset + j, i + row_offset_recv, entries_recv[i].buffer[start + j]);
        }
        entries_recv[i].changed = 0;
      }
      caca_fill_box(cv, col_offset, e + row_offset_recv, text_buffer_size, 1, ' ');
      start = 0;
      entries_recv[e].size = new_recv_entry_bytes;
      entries_recv[e].cursor = 0;
      size = entries_recv[e].size;
      memset(entries_recv[e].buffer, '\0', size * 4); // *4 because using uint32_t
      for (j = 0; j < size; j++)
      {
        entries_recv[e].buffer[start + j] = (uint32_t)recline[start + j];
        caca_put_char(cv, col_offset + j, e + row_offset_recv, entries_recv[e].buffer[start + j]);
      }

    }

    new_recv_entry_bytes = 0; // always reset (because we must have handled the entry already)

//    int s = pthread_mutex_lock(&mtx);
    caca_refresh_display(dp); // FIXME: separate the portions of the canvas!!!!
//    s = pthread_mutex_unlock(&mtx);

    if (caca_get_event(dp, CACA_EVENT_KEY_PRESS, &ev, -1) == 0)
      continue;

    switch (caca_get_event_key_ch(&ev))
    {
      case CACA_KEY_ESCAPE:
        running = 0;
        // Free string buffers
        break;
      default:
        break;
    }

    memset(recline, '\0', MAXLINE);
    if (recvfd > 0)
      new_recv_entry_bytes = recv(recvfd, recline, MAXLINE, 0); // FIXME

  }

  // NOTE: memory handling should not happen inside switch statement!
//  free(recline); // Release buffer memory // FIXME

  return NULL ;
}

int get_options(int argc, char **argv, options * opt)
{
  opterr = 0;
  int c;

  // Initialize defaults
  strcpy(opt->video_device_name, "/dev/video0\0");
  memset(opt->peer_name, '\0', MAXHOSTNAMELEN);
  opt->is_server = 0; // Socket client is the default behavior

  while ((c = getopt(argc, argv, "sv:p:")) != -1) // The ":" next to a flag indicates to expect a value
  {
    switch (c)
    {
      case 's':
        opt->is_server = 1;
        break;
      case 'v':
        strcpy(opt->video_device_name, optarg);
        break;
      case 'p':
        strcpy(opt->peer_name, optarg);
        break;
      case '?':
      {
        if (optopt == 'v' || optopt == 'p')
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
        return -1;
      }
      default:
        abort();
        break;
    }
  }

  // Get non-option arguments (usernames, in this case)
  /* None so far
   for (int index = optind; index < argc; index++)
   {
   argv[index];
   }
   */

  return 0;
}
