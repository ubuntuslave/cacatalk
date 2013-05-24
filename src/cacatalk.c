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
// ******** Threads  **********
void * send_video_thread(void * arguments);
// ----------------------------
int chat(caca_canvas_t *cv, caca_display_t *dp, int text_fd, int vid_fd, Window *win, char * peer_hostname);

// GLOBALS:
video_out_args g_video_out;

int main(int argc, char **argv)
{
  void (*demo)(void * arg1, ...) = NULL;

  caca_canvas_t *cv;
  caca_display_t *dp;
  cv = caca_create_canvas(0, 0);
  if (!cv)
  {
    fprintf(stderr, "%s: unable to initialise libcaca\n", argv[0]);
    return 1;
  }

  Window * win; // To store rows and columns of the terminal and other dimensional information
  win = (Window *)malloc(sizeof(Window));
  int img_width = 640; // TODO: parametrize
  int img_height = 480;
  unsigned int lines_of_video = 16; // Arbitrary number of lines // TODO: parametrize
  char * dev_name;
  char label_name_of_peer[MAXHOSTNAMELEN] = "";
  int quit = 0; // Indicates main loop to quit
  int is_connected = 0;
  long t = 0;
  int rc;
  pthread_t threads[NUM_THREADS];

  options *arg_opts;
  arg_opts = (options *)malloc(sizeof(options));

  if (get_options(argc, argv, arg_opts) == -1)
  {
    fprintf(stderr, "%s: unable to get option arguments\n", argv[0]);
    return 1;
  }

  // ---------------------------------------------------
  set_window(STDIN_FILENO, lines_of_video, win, cv); // Create a window object in order to obtain dimensions

  // --------------- Video related ---------------------------------
  video_params *vid_params;
  vid_params = (video_params *)malloc(sizeof(video_params));
  dev_name = arg_opts->video_device_name;
  g_video_out.socketfd = -1; // We don't have a socket connected for video transmission yet
  g_video_out.quit = quit; // Don't quit just yet
  g_video_out.win = win; // Set to window's pointer struct
  // TODO: don't always set video (in case it doesn't exist)
  set_video(vid_params, dev_name, win, img_width, img_height); //FIXME: arbitrary canvas height for video
  g_video_out.vid_params = vid_params;
  turn_video_stream_off(vid_params); // We don't want video streaming initially
  // Start the video thread
  rc = pthread_create(&threads[t], NULL, send_video_thread, (void *)&g_video_out);
  if (rc)
  {
    ERROR_EXIT("pthread: video out thread failure", 1)
  }
  else
    t++;

  // ----------------------------------------------------------------------
  // -------   Socket related:  -------------------------------------------
  int listen_chat_fd, listen_video_fd; // Listening socket filedescriptors for chat and video stream
  int sock_chat_fd = -1; // Initial value for non-existing connection
  int sock_vid_fd = -1; // Socket for receiving and sending video stream.
  struct sockaddr_in client_addr_chat, client_addr_video;
  struct sockaddr_in server_addr_chat, server_addr_video;
  static struct sockaddr_in server_addr_to_connect;
  socklen_t clilen = sizeof(client_addr_chat);
  char address_buffer_v4[INET_ADDRSTRLEN] = "";

  if ((listen_chat_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    ERROR_EXIT(" chat: server socket call failed", 1)
  }
  if ((listen_video_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    ERROR_EXIT(" video: server socket call failed", 1)
  }

  bzero(&server_addr_chat, sizeof(server_addr_chat));
  bzero(&server_addr_video, sizeof(server_addr_video));
  server_addr_chat.sin_family = AF_INET;
  server_addr_video.sin_family = AF_INET;
  server_addr_chat.sin_addr.s_addr = htonl(INADDR_ANY );
  server_addr_video.sin_addr.s_addr = htonl(INADDR_ANY );
  server_addr_chat.sin_port = PORT_CHAT;
  server_addr_video.sin_port = PORT_VIDEO;

  set_non_block(listen_chat_fd); // Set it as nonblocking descriptor
  // Let the listening to video socket be blocking
  // set_non_block(listen_video_fd); // Set it as nonblocking descriptor

  if (bind(listen_chat_fd, SOCKADDR &server_addr_chat, sizeof(server_addr_chat)) == -1)
  {
    ERROR_EXIT(" chat: server socket binding call failed", 1)
  }
  if (bind(listen_video_fd, SOCKADDR &server_addr_video, sizeof(server_addr_video)) == -1)
  {
    ERROR_EXIT(" video: server socket binding call failed", 1)
  }

  // We will only listen on the chat socket (the video socket will get connected immediately after accepting a connnection)
  if (-1 == listen(listen_chat_fd, LISTEN_QUEUE_SIZE))
  {
    ERROR_EXIT(" listen call failed", 1)
  }

  Signal(SIGCHLD, on_sigchld); // TODO: Since we don't have children, handle your own signals with house keeping
  // ----------------------------------------------------------------------

  dp = caca_create_display_with_driver(cv, arg_opts->driver_options[arg_opts->driver_choice - 1]);
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

  // Main menu:
  // c: Initiates socket connection to peer address
  // a: set IP address (or DNS name) of peer to connect to
  // d | Enter: enter chat room (for demo)
  // q | Esc: to quit
  display_menu(cv, arg_opts);

  caca_refresh_display(dp);

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
            g_video_out.quit = quit; // Tell the thread to quit now
            break;
          case 'a':
          case 'A':
            key_choice = 'a';
            demo = set_peer_address;
            break;
          case CACA_KEY_RETURN:
          case 'd':
          case 'D':
            key_choice = 'd';
            demo = chat;
            break;
          case 'c':
          case 'C':
            key_choice = 'c';
            if (sock_chat_fd < 0) // Only attempt a connection when server mode hasn't accepted one already
            {
              // Initialize connection
              // Check if there is a host name on command line; if not use default
              if ((strlen(arg_opts->peer_name) > 0) && (is_connected == 0))
              {
                // Attention: needs to have static memory for server
                sock_chat_fd = connect_to_peer_socket(arg_opts->peer_name, &server_addr_to_connect, PORT_CHAT);
                // TODO: use this instead:
                //connfd = inet_connect(arg_opts->peer_name, PORT, SOCK_STREAM);

                if (sock_chat_fd == -1)
                  perror("main: Could not connect to server socket");
                if (sock_chat_fd > 0)
                {

                  // We will only listen on the chat socket (the video socket will get connected immediately after accepting a connnection)
                  if (-1 == listen(listen_video_fd, LISTEN_QUEUE_SIZE))
                  {
                    ERROR_EXIT(" video: listen (as client) call failed", 1)
                  }

                  if ((sock_vid_fd = accept(listen_video_fd, SOCKADDR &client_addr_video, &clilen)) < 0)
                  {
                    if (EINTR == errno)
                      continue; // back to loop
                    // If blocking:
                    else
                      ERROR_EXIT(" video: accept error (as client)", 1);
                  }
                  else
                  {
                    g_video_out.socketfd = sock_vid_fd; // Assign socket for video transmission
                    is_connected = 1; // To indicate not longer trying to connect
                  }

                  void * peer_addr_ptr = NULL;
                  if (client_addr_video.sin_family == AF_INET) // check it is IP4
                  { // is a valid IP4 Address
                    peer_addr_ptr = (struct sockaddr_in *)&(client_addr_video.sin_addr);
                    // Convert IP address from network (binary) to textual form (Presentation (eg. dotted decimal))
                    inet_ntop(AF_INET, peer_addr_ptr, address_buffer_v4, INET_ADDRSTRLEN);

                    sprintf(label_name_of_peer, "%s@%s:%d", "initiator", address_buffer_v4, client_addr_video.sin_port);
                  }
                }
              }
            }
            break;
          case 'v':
          case 'V':
            key_choice = 'v';
            demo = change_video_device;
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
      if ((sock_chat_fd = accept(listen_chat_fd, SOCKADDR &client_addr_chat, &clilen)) < 0)
      {
        if (EINTR == errno)
          continue; // back to loop
        // Don't use if non-blocking:
        //else
        //ERROR_EXIT("accept error", 1);
      }
      else
      { // A connection has been accepted
        void * peer_addr_ptr = NULL;
        if (client_addr_chat.sin_family == AF_INET) // check it is IP4 // TODO: support for IPv6
        { // is a valid IP4 Address
          peer_addr_ptr = (struct sockaddr_in *)&(client_addr_chat.sin_addr);
          // Convert IP address from network (binary) to textual form (Presentation (eg. dotted decimal))
          inet_ntop(AF_INET, peer_addr_ptr, address_buffer_v4, INET_ADDRSTRLEN);

          sprintf(label_name_of_peer, "%s@%s:%d", "peer", address_buffer_v4, client_addr_chat.sin_port); // TODO: find username
        }

        client_addr_chat.sin_port = PORT_VIDEO; // Just modify the port number for video streaming (FIXME: may be wrong)

        while ((sock_vid_fd = connect_to_peer_socket(address_buffer_v4, &server_addr_video, PORT_VIDEO)) == -1) // Keep trying
        {
          continue; // back to loop
        }

        g_video_out.socketfd = sock_vid_fd; // Assign socket for video transmission
        is_connected = 1; // To indicate not longer trying to connect

      }
    }

    if (demo)
    {
      caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY);
      caca_clear_canvas(cv);

      if (key_choice == 'a')
        demo(cv, dp, arg_opts);
      if (key_choice == 'd')
        demo(cv, dp, sock_chat_fd, sock_vid_fd, win, label_name_of_peer);
      if (key_choice == 'v')
        demo(cv, dp, arg_opts, vid_params, win, img_width, img_height);

      caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY);

      caca_clear_canvas(cv);
      caca_set_cursor(dp, 0); // Disable cursor
      display_menu(cv, arg_opts);
      caca_refresh_display(dp);
      demo = NULL;
    }
  }

  // Close sockets:
  close(sock_chat_fd);
  close(sock_vid_fd);
  close(listen_chat_fd);
  close(listen_video_fd);

  // Clean up caca
  caca_free_display(dp);
  caca_free_canvas(cv);

  close_video_stream(vid_params);

  pthread_exit(NULL ); // FIXME: perhaps it should join a thread, for instance
  // return pthread_join(some_thread, NULL); // Wait until thread is finished
  // return 0;

}

void display_menu(caca_canvas_t *cv, options * arg_opts)
{
  int xo = caca_get_canvas_width(cv) - 2;
  int yo = caca_get_canvas_height(cv) - 2;

  caca_set_color_ansi(cv, CACA_LIGHTGRAY, CACA_BLACK);
  caca_clear_canvas(cv);
  caca_draw_thin_box(cv, 1, 1, xo, yo);

  caca_put_str(cv, (xo - strlen("cacatalk demo")) / 2, 3, "cacatalk demo");
  caca_put_str(cv, (xo - strlen("==================")) / 2, 4, "==================");
  caca_put_str(cv, (xo - strlen(arg_opts->host_IPv4)) / 2, 6, arg_opts->host_IPv4);

  // a: set IP address (or DNS name) of peer to connect to
  // c: Connect to peer address
  // d | Enter: enter chat room (for demo)
  // q | Esc: to quit
  caca_put_str(cv, 4, 8, "Options:");
  char addr_label[MAXPATHLEN] = "";
  sprintf(addr_label, "'a'        : Set IP Address (or DNS name) of peer [%s]", arg_opts->peer_name);
  caca_put_str(cv, 4, 10, addr_label);
  caca_put_str(cv, 4, 11, "'b'        : blank");

  char conn_label[MAXPATHLEN] = "";
  if (g_video_out.socketfd != -1)
    sprintf(conn_label, "'c'        : Connected to %s", arg_opts->peer_name);
  else
    sprintf(conn_label, "'c'        : Connect to peer");
  caca_put_str(cv, 4, 12, conn_label);

  char dev_label[MAXPATHLEN] = "";
  if (g_video_out.vid_params->is_ok == 1)
    sprintf(dev_label, "'v'        : Video device working [%s]", arg_opts->video_device_name);
  else
    sprintf(dev_label, "'v'        : Set video device");
  caca_put_str(cv, 4, 13, dev_label);

  caca_put_str(cv, 4, yo - 2, "'q' | Esc  : quit");

}

/* TODO:
 int set_peer_address(caca_canvas_t *cv, caca_display_t *dp, char * peer_hostname)
 {
 caca_set_color_ansi(cv, CACA_LIGHTGRAY, CACA_BLACK);
 caca_clear_canvas(cv);
 caca_draw_thin_box(cv, 1, 1, xo, yo);

 char *video_in_buffer = NULL;
 video_in_buffer = malloc(video_area);
 unsigned int new_recv_video_bytes = 0; // Indicates when the size of a new video frame
 }
 */

int chat(caca_canvas_t *cv, caca_display_t *dp, int text_fd, int vid_fd, Window *win, char * peer_hostname)
{
  int xo = caca_get_canvas_width(cv) - 2;
  int yo = caca_get_canvas_height(cv) - 2;
  int video_area = 16 * win->video_cols * win->video_lines; // A safe value for the receiving video buffer

  turn_video_stream_on(g_video_out.vid_params);
  char *video_in_buffer = NULL;
  video_in_buffer = malloc(video_area);
  unsigned int new_recv_video_bytes = 0; // Indicates when the size of a new video frame

  fd_set readset, writeset;
  int maxfd;
  char label_recv[MAXHOSTNAMELEN];
  unsigned int row_offset_recv = 1 + win->video_lines;
  unsigned int row_offset_self = row_offset_recv + TEXT_ENTRIES + 1;
  unsigned int col_offset = 2;
  int text_buffer_size = (MIN(caca_get_canvas_width(cv), BUFFER_SIZE)) - 2 * col_offset; // Leave padding (margin)
  char peer_username[20] = ""; // TODO obtain the info
  caca_set_cursor(dp, 1); // Enable cursor

  struct timeval timeout;
  struct timeval *pto;

  // Timeout for select() is specified in argv[1]

  /*// NULL for blocking (Purely arbitrary time for now)
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

  // Labels
  char sender_label[MAX_INPUT] = "";
  char sender_label1[MAX_INPUT] = "";
  char sender_label2[MAX_INPUT] = " Press: Ctr+V to toggle video || Escape to exit\0";
  if (g_video_out.vid_params->is_on == 1)
    sprintf(sender_label1, "[Video ON]");
  else
    sprintf(sender_label1, "[Video OFF]");
  sprintf(sender_label, "%s %s", sender_label1, sender_label2);
  sprintf(label_recv, "%s %s", peer_hostname, peer_username);

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

  caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY);
  caca_clear_canvas(cv);
  caca_draw_thin_box(cv, 1, 1, xo, yo);

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

  maxfd = MAXFD(fileno(stdin), vid_fd) + 1;

  caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY); // Reset background color

  // FIXME: as a hack, I'm redirecting stderr to /dev/null to get rid of annoying v4l2 error messages
  if (close(fileno(stderr)) == 0)
    open("/dev/null", O_RDWR);

  while (running)
  {
    if (g_video_out.vid_params->is_on == 1)
      sprintf(sender_label1, "[Video ON]");
    else
      sprintf(sender_label1, "[Video OFF]");
    sprintf(sender_label, "%s %s", sender_label1, sender_label2);
    caca_set_color_ansi(cv, CACA_WHITE, CACA_BLUE);
    caca_fill_box(cv, col_offset, row_offset_self - 1, text_buffer_size, 1, ' ');
    caca_put_str(cv, col_offset, row_offset_self - 1, sender_label);

    caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY); // Reset background color
    // Always reset the cursor on the active textentry
    caca_gotoxy(cv, col_offset + entries_self[e].cursor, e + row_offset_self);

    if (text_fd > 0)
    {
      // Important:
      // Since these structures are modified by the call,
      // we must ensure that we reinitialize them if we are repeatedly calling select() from within a loop.)
      FD_ZERO(&readset);
      FD_ZERO(&writeset);
      FD_SET(vid_fd, &readset);
      FD_SET(text_fd, &readset);
      FD_SET(text_fd, &writeset);

      if (select(maxfd, &readset, &writeset, NULL, pto) > 0)
      {
        if (FD_ISSET(text_fd, &readset))
        {
          memset(recline, '\0', MAXLINE);
          new_recv_entry_bytes = recv(text_fd, recline, MAXLINE, 0);
        }

        if (FD_ISSET(vid_fd, &readset))
        {
          bzero(video_in_buffer, video_area);
          new_recv_video_bytes = recv(vid_fd, video_in_buffer, video_area, 0);
        }
        else
          new_recv_video_bytes = 0;

        if (FD_ISSET(text_fd, &writeset))
        {
          if (entries_self[e].size > 0 && newline_entered == 1 && send_succesful == 0)
          {
            int sent_bytes = send(text_fd, sendline, entries_self[e].size, 0);
            if (sent_bytes >= 0)
            {
              send_succesful = 1;
            }
          }
        }
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
      // TODO: try to center window
      ssize_t imported_bytes = caca_import_area_from_memory(cv, col_offset, 0, video_in_buffer, new_recv_video_bytes,
                                                            win->caca_format);
      //      ssize_t imported_bytes = caca_import_area_from_memory(cv, caca_get_canvas_width(cv)/2-win->cols/2, 0, video_in_buffer, new_recv_video_bytes, win->caca_format);
      caca_fill_box(cv, col_offset, win->video_lines, text_buffer_size, 1, ' ');
      char import_label[100] = "";
      sprintf(import_label, "Received= %d bytes, Imported %d bytes of caca video (%d x %d)", new_recv_video_bytes,
              imported_bytes, win->video_cols, win->video_lines);
      caca_put_str(cv, col_offset, win->video_lines, import_label);
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

    // Always reset the cursor on the active textentry (to hide the v4l2 errors)
    caca_gotoxy(cv, col_offset + entries_self[e].cursor, e + row_offset_self);

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
    else
    { // Update last entry if changed
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

    // Always reset the cursor on the active textentry (to hide the v4l2 errors)
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
      case CACA_KEY_CTRL_V:
        // Toggle the state of video streaming
        if ((g_video_out.vid_params->is_on != 1) && (g_video_out.vid_params->is_ok == 1))
          turn_video_stream_on(g_video_out.vid_params);
        else
          turn_video_stream_off(g_video_out.vid_params);
        break;
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

  turn_video_stream_off(g_video_out.vid_params);
  free(sendline);
  free(video_in_buffer);

  return 0;
}

int set_video(video_params *vid_params, char *dev_name, Window *win, int img_width, int img_height)
{
  unsigned int i, n_buffers;

  // We don't know yet if setting the video device will succeed, so set the "not okay" flag initially
  vid_params->is_ok = 0;
  vid_params->is_on = 0; // Streaming is not "on" yet

  if(strlen(dev_name) > 0)
  {
    // ------ Arbitrary settings:
    vid_params->caca_format = win->caca_format;
    vid_params->caca_dither = "fstein";
    vid_params->number_of_buffers = 2;// Arbitrary double buffer
    vid_params->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vid_params->memory = V4L2_MEMORY_MMAP;
    unsigned int font_width = 6, font_height = 10;// FIXME: purely arbitrary based on the usual ansi format for terminals
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
    if(vid_params->v4l_fd > 0)
    {
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
      // DON'T: turn_video_stream_on(vid_params);
    }
  }
  else
  {
    perror("set_video: Cannot open video device\n");
    //exit(EXIT_FAILURE);
  }

  return vid_params->is_ok;
}

void set_peer_address(caca_canvas_t *cv, caca_display_t *dp, options *opts)
{
  int xo = caca_get_canvas_width(cv) - 2;
  int yo = caca_get_canvas_height(cv) - 2;
  unsigned int row_offset = 6;
  unsigned int col_offset = 2;
  int text_buffer_size = (MIN(caca_get_canvas_width(cv), BUFFER_SIZE)) - 2 * col_offset; // Leave padding (margin)
  textentry addr_entry;
  unsigned int j, size;
  int running = 1;
  int newline_entered = 0;

  caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY);
  caca_clear_canvas(cv);
  caca_draw_thin_box(cv, 1, 1, xo, yo);

  caca_put_str(cv, (xo - strlen("cacatalk demo")) / 2, 3, "cacatalk demo");
  caca_put_str(cv, (xo - strlen("==================")) / 2, 4, "==================");

  caca_set_cursor(dp, 1); // Enable cursor

  caca_set_color_ansi(cv, CACA_WHITE, CACA_BLUE);
  char *prompt = "Enter IP address or name: ";
  caca_fill_box(cv, col_offset, row_offset, strlen(prompt), 1, ' ');
  caca_put_str(cv, col_offset, row_offset, prompt);

  col_offset += strlen(prompt);

  addr_entry.buffer[0] = 0;
  addr_entry.size = 0;
  addr_entry.cursor = 0;
  addr_entry.changed = 1;

  caca_refresh_display(dp);

  while (running)
  {
    // Always reset the cursor on the active textentry (to hide the v4l2 errors)
    caca_gotoxy(cv, col_offset + addr_entry.cursor, row_offset);

    // Update self (sender) entries
    if (newline_entered == 1)
    {
      size = addr_entry.size;
      bzero(opts->peer_name, strlen(opts->peer_name));
      for (j = 0; j < size; j++)
        opts->peer_name[j] = addr_entry.buffer[j];

      running = 0; // and quit
      continue;
    }
    else // Only update the entry if changed
    {
      if (addr_entry.changed == 1)
      {
        caca_set_color_ansi(cv, CACA_BLUE, CACA_WHITE);
        caca_fill_box(cv, col_offset, row_offset, text_buffer_size, 1, ' ');

        size = addr_entry.size;
        for (j = 0; j < size; j++)
        {
          caca_put_char(cv, col_offset + j, row_offset, addr_entry.buffer[j]);
        }
      }
    }
    // always reset flags after updating entries
    addr_entry.changed = 0;
    newline_entered = 0;

    // Always reset the cursor on the active textentry (to hide the v4l2 errors)
    caca_gotoxy(cv, col_offset + addr_entry.cursor, row_offset);
    caca_refresh_display(dp);

    caca_event_t ev;

    //  timeout value in microseconds, -1 for blocking behaviour
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
        // Set flags
        newline_entered = 1;
        addr_entry.changed = 1;
        break;
      }
      case CACA_KEY_HOME:
        addr_entry.cursor = 0;
        break;
      case CACA_KEY_END:
        addr_entry.cursor = addr_entry.size;
        break;
      case CACA_KEY_LEFT:
        if (addr_entry.cursor)
          addr_entry.cursor--;
        break;
      case CACA_KEY_RIGHT:
        if (addr_entry.cursor < addr_entry.size)
          addr_entry.cursor++;
        break;
      case CACA_KEY_DELETE:
        if (addr_entry.cursor < addr_entry.size)
        {
          memmove(addr_entry.buffer + addr_entry.cursor, addr_entry.buffer + addr_entry.cursor + 1,
                  (addr_entry.size - addr_entry.cursor + 1) * 4);
          addr_entry.size--;
          addr_entry.changed = 1;
        }
        break;
      case CACA_KEY_BACKSPACE:
        if (addr_entry.cursor)
        {
          memmove(addr_entry.buffer + addr_entry.cursor - 1, addr_entry.buffer + addr_entry.cursor,
                  (addr_entry.size - addr_entry.cursor) * 4);
          addr_entry.size--;
          addr_entry.cursor--;
          addr_entry.changed = 1;
        }
        break;
      default:
        if (addr_entry.size < BUFFER_SIZE)
        {
          memmove(addr_entry.buffer + addr_entry.cursor + 1, addr_entry.buffer + addr_entry.cursor,
                  (addr_entry.size - addr_entry.cursor) * 4);
          addr_entry.buffer[addr_entry.cursor] = caca_get_event_key_utf32(&ev);
          addr_entry.size++;
          addr_entry.cursor++;
          addr_entry.changed = 1;
        }
        break;
    }

  }
}

void change_video_device(caca_canvas_t *cv, caca_display_t *dp, options *opts, video_params *vid_params, Window *win,
                         int img_width, int img_height)
{
  int xo = caca_get_canvas_width(cv) - 2;
  int yo = caca_get_canvas_height(cv) - 2;
  unsigned int row_offset = 6;
  unsigned int col_offset = 2;
  int text_buffer_size = (MIN(caca_get_canvas_width(cv), BUFFER_SIZE)) - 2 * col_offset; // Leave padding (margin)
  textentry addr_entry;
  unsigned int j, size;
  int running = 1;
  int newline_entered = 0;

  caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY);
  caca_clear_canvas(cv);
  caca_draw_thin_box(cv, 1, 1, xo, yo);

  caca_put_str(cv, (xo - strlen("cacatalk demo")) / 2, 3, "cacatalk demo");
  caca_put_str(cv, (xo - strlen("==================")) / 2, 4, "==================");

  caca_set_cursor(dp, 1); // Enable cursor

  caca_set_color_ansi(cv, CACA_WHITE, CACA_BLUE);
  char dev_label[MAXPATHLEN] = "";
  if (vid_params->is_ok == 1)
    sprintf(dev_label, "Current video device [%s] is working", opts->video_device_name);
  else
    sprintf(dev_label, "Please set the path to a video device.");
  caca_put_str(cv, col_offset, row_offset, dev_label);

  row_offset += 2;
  char *prompt = "Enter video device name: ";
  caca_fill_box(cv, col_offset, row_offset, strlen(prompt), 1, ' ');
  caca_put_str(cv, col_offset, row_offset, prompt);

  col_offset += strlen(prompt);

  addr_entry.buffer[0] = 0;
  addr_entry.size = 0;
  addr_entry.cursor = 0;
  addr_entry.changed = 1;

  caca_refresh_display(dp);

  while (running)
  {
    // Always reset the cursor on the active textentry (to hide the v4l2 errors)
    caca_gotoxy(cv, col_offset + addr_entry.cursor, row_offset);

    // Update self (sender) entries
    if (newline_entered == 1)
    {
      size = addr_entry.size;
      bzero(opts->video_device_name, strlen(opts->video_device_name));
      for (j = 0; j < size; j++)
        opts->video_device_name[j] = addr_entry.buffer[j];

      // Try to set video to see if its working
      if (set_video(vid_params, opts->video_device_name, win, img_width, img_height) > 0)
      {
        // Update prompt
        if (vid_params->is_ok == 1)
          sprintf(dev_label, "Current video device [%s] is working", opts->video_device_name);
        else
          sprintf(dev_label, "Please set the path to a video device.");
        caca_put_str(cv, col_offset - strlen(prompt), row_offset - 2, dev_label);
        running = 0; // and quit
      }
      else // Not successful video set up (Try again)
      {
        bzero(opts->video_device_name, strlen(opts->video_device_name));
        addr_entry.buffer[0] = 0;
        addr_entry.size = 0;
        addr_entry.cursor = 0;
        addr_entry.changed = 1;
        newline_entered = 0; // To try again
      }

      continue;
    }
    else // Only update the entry if changed
    {
      if (addr_entry.changed == 1)
      {
        caca_set_color_ansi(cv, CACA_BLUE, CACA_WHITE);
        caca_fill_box(cv, col_offset, row_offset, text_buffer_size, 1, ' ');

        size = addr_entry.size;
        for (j = 0; j < size; j++)
        {
          caca_put_char(cv, col_offset + j, row_offset, addr_entry.buffer[j]);
        }
      }
    }
    // always reset flags after updating entries
    addr_entry.changed = 0;
    newline_entered = 0;

    // Always reset the cursor on the active textentry (to hide the v4l2 errors)
    caca_gotoxy(cv, col_offset + addr_entry.cursor, row_offset);
    caca_refresh_display(dp);

    caca_event_t ev;

    //  timeout value in microseconds, -1 for blocking behaviour
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
        // Set flags
        newline_entered = 1;
        addr_entry.changed = 1;
        break;
      }
      case CACA_KEY_HOME:
        addr_entry.cursor = 0;
        break;
      case CACA_KEY_END:
        addr_entry.cursor = addr_entry.size;
        break;
      case CACA_KEY_LEFT:
        if (addr_entry.cursor)
          addr_entry.cursor--;
        break;
      case CACA_KEY_RIGHT:
        if (addr_entry.cursor < addr_entry.size)
          addr_entry.cursor++;
        break;
      case CACA_KEY_DELETE:
        if (addr_entry.cursor < addr_entry.size)
        {
          memmove(addr_entry.buffer + addr_entry.cursor, addr_entry.buffer + addr_entry.cursor + 1,
                  (addr_entry.size - addr_entry.cursor + 1) * 4);
          addr_entry.size--;
          addr_entry.changed = 1;
        }
        break;
      case CACA_KEY_BACKSPACE:
        if (addr_entry.cursor)
        {
          memmove(addr_entry.buffer + addr_entry.cursor - 1, addr_entry.buffer + addr_entry.cursor,
                  (addr_entry.size - addr_entry.cursor) * 4);
          addr_entry.size--;
          addr_entry.cursor--;
          addr_entry.changed = 1;
        }
        break;
      default:
        if (addr_entry.size < BUFFER_SIZE)
        {
          memmove(addr_entry.buffer + addr_entry.cursor + 1, addr_entry.buffer + addr_entry.cursor,
                  (addr_entry.size - addr_entry.cursor) * 4);
          addr_entry.buffer[addr_entry.cursor] = caca_get_event_key_utf32(&ev);
          addr_entry.size++;
          addr_entry.cursor++;
          addr_entry.changed = 1;
        }
        break;
    }

  }
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

  turn_video_stream_off(vid_params);

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
    // FIXME: let's be silent on errors for now
    // I was getting the following one while trying to turn the stream on/off
    // libv4l2 error dequeuing buf invalid argument
    //fprintf(stderr, "error %d, %s\n", errno, strerror(errno));
  }
  return r;
}

void set_window(int fd, unsigned short video_lines, Window *win, caca_canvas_t *cv)
{
  win->cols = caca_get_canvas_width(cv);
  win->rows = caca_get_canvas_height(cv);

  /* Only good for terminals:
   struct winsize size;
   if (ioctl(fd, TIOCGWINSZ, &size) < 0)
   {
   perror("TIOCGWINSZ error");
   return;
   }
   win->rows = size.ws_row;
   win->cols = size.ws_col;
   */
  win->video_lines = video_lines;
  // Default assuming a 4:3 aspect ratio and 10:6 font
  float aspect_ratio = (float)4 / 3 * (float)10 / 6;
  win->video_cols = (unsigned int)(aspect_ratio * (float)video_lines);
  win->caca_format = "caca"; // Arbitrary format for caca export/import
}

int get_options(int argc, char **argv, options * opt)
{
  opterr = 0;
  int c;

  // Initialize defaults
  opt->video_device_name[0] = '\0';
//  strcpy(opt->video_device_name, "/dev/video0\0");
  memset(opt->peer_name, '\0', MAXHOSTNAMELEN);
  get_IP_addresses(opt->host_IPv4, 0);
  opt->driver_choice = 2; // Default to ncurses terminal
  int user_driver_choice = -1;
  // Initialize hard-coded caca display choices for the device where to draw the caca canvas
  opt->driver_options[0] = NULL; // Selects default caca display driver (usually an X window)
  opt->driver_options[1] = "ncurses";
  opt->driver_options[2] = "conio"; // Fails TODO: install conio
  opt->driver_options[3] = "gl";
  opt->driver_options[4] = "raw"; // Fails
  opt->driver_options[5] = "vga"; // Fails
  opt->driver_options[6] = "slang";

  while ((c = getopt(argc, argv, "d:v:p:")) != -1) // The ":" next to a flag indicates to expect a value
  {
    switch (c)
    {
      case 'd':
        user_driver_choice = atoi(optarg);
        if (user_driver_choice > 0) // Be safe with values above zero
          opt->driver_choice = user_driver_choice;
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

// ******************** THREADS *************************
void * send_video_thread(void * arguments)
{
  // No need now of using the passed arguments
  // struct video_out_args_s *args = (struct video_out_args_s *)arguments;

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
  caca_canvas_t *cv;

  pto = &socket_timeout;
  socket_timeout.tv_sec = 0; // seconds
  socket_timeout.tv_usec = 5000; // microseconds FIXME: wait is arbitrary for now

  // Allowable exportable region dimensions (so export doesn't fail)
  if (g_video_out.win->cols > g_video_out.vid_params->cv_cols)
    cols = g_video_out.vid_params->cv_cols;
  else
    cols = g_video_out.win->cols;

  if (g_video_out.win->rows > g_video_out.vid_params->cv_rows)
    rows = g_video_out.vid_params->cv_rows;
  else
    rows = g_video_out.win->rows;

  cv = caca_create_canvas(cols, rows);
  if (!cv)
  {
    perror(" send_video_thread: unable to initialise libcaca canvas\n");
    exit(EXIT_FAILURE);
  }

  while (!g_video_out.quit)
  {
    while ((g_video_out.socketfd != -1) && (g_video_out.vid_params->is_on == 1) && (g_video_out.vid_params->is_ok == 1))
    {
      do
      {
        FD_ZERO(&fds);
        FD_SET(g_video_out.vid_params->v4l_fd, &fds);

        // Timeout.
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select(g_video_out.vid_params->v4l_fd + 1, &fds, NULL, NULL, &tv);
      } while ((r == -1 && (errno = EINTR)));

      if (r == -1)
      {
        perror("select");
        // return errno; // TODO: don't quit, but handle error gracefully
        continue;
      }

      CLEAR(g_video_out.vid_params->buf);
      g_video_out.vid_params->buf.type = g_video_out.vid_params->type;
      g_video_out.vid_params->buf.memory = g_video_out.vid_params->memory;
      xioctl(g_video_out.vid_params->v4l_fd, VIDIOC_DQBUF, &(g_video_out.vid_params->buf));

      //-----------------------------------------
      // Persistent loading
      while ((im = load_image_from_V4L_buffer(&(g_video_out.vid_params->fmt),
                                              &(g_video_out.vid_params->buffers[g_video_out.vid_params->buf.index]),
                                              g_video_out.vid_params->buf.bytesused)) == NULL )
      {
        fprintf(stderr, "grab: Unable to load caca from V4L\n");
      }

      caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY);
      caca_clear_canvas(cv);
      if (caca_set_dither_algorithm(
          im->dither, g_video_out.vid_params->caca_dither ? g_video_out.vid_params->caca_dither : "fstein"))
      {
        fprintf(stderr, "grab: Can't dither image with algorithm '%s'\n", g_video_out.vid_params->caca_dither);
        unload_image(im);
        caca_free_canvas(cv);
        //return -1;      // TODO: don't quit, but handle error gracefully
        continue;
      }

      if (g_video_out.vid_params->caca_brightness != -1)
        caca_set_dither_brightness(im->dither, g_video_out.vid_params->caca_brightness);
      if (g_video_out.vid_params->caca_contrast != -1)
        caca_set_dither_contrast(im->dither, g_video_out.vid_params->caca_contrast);
      if (g_video_out.vid_params->caca_gamma != -1)
        caca_set_dither_gamma(im->dither, g_video_out.vid_params->caca_gamma);

      caca_dither_bitmap(cv, 0, 0, g_video_out.vid_params->cv_cols, g_video_out.vid_params->cv_rows, im->dither,
                         im->pixels);

      //unload_image(im); // Enable it if using memcpy in the loading

      if (g_video_out.socketfd > 0)
      {
        sock_maxfd = MAXFD(fileno(stdin), g_video_out.socketfd) + 1; // FIXME: here is the problem? chicken vs egg case

        // Important:
        // Since these structures are modified by the call,
        // we must ensure that we reinitialize them if we are repeatedly calling select() from within a loop.)
        FD_ZERO(&sock_writeset);
        FD_SET(g_video_out.socketfd, &sock_writeset);

        if (select(sock_maxfd, NULL, &sock_writeset, NULL, pto) > 0)
        {

          if (FD_ISSET(g_video_out.socketfd, &sock_writeset))
          {
            exported_bytes = 0; // clear number of bytes exported (assuming they all went through)
            // This is what needs to be sent through the socket
            //      export = caca_export_canvas_to_memory(cv, vid_params->caca_format ? format : "ansi", &exported_bytes);
            //            export = caca_export_area_to_memory(cv, 0, 0, cols, rows, vid_params->caca_format ? vid_params->caca_format : "ansi", &exported_bytes);
            export = caca_export_area_to_memory(
                cv, 0, 0, cols, rows,
                g_video_out.vid_params->caca_format ? g_video_out.vid_params->caca_format : "caca", &exported_bytes);
            if (!export)
            {
              fprintf(stderr, "grab: Can't export to format '%s'\n", g_video_out.vid_params->caca_format);
            }
            else
            {
              int sent_bytes = send(g_video_out.socketfd, export, exported_bytes, 0);

              char sent_label[100] = "";
              sprintf(sent_label, "Exported = %d bytes, Sent %d bytes (%dx%d) frame", exported_bytes, sent_bytes, cols,
                      rows);
              caca_put_str(cv, 0, rows + 3, sent_label);
              free(export);
            }
          }
        }
      }

      // TODO: Nice display margin:
      // ------------------------------------------------------------------
      //       caca_set_color_ansi(cv, CACA_LIGHTGRAY, CACA_BLACK);
      //       caca_draw_thin_box(cv, 1, 1, caca_get_canvas_width(cv) - 2, caca_get_canvas_height(cv) - 2);
      //       caca_printf(cv, 4, 1, "[%i.%i FPS]----", 1000000 / caca_get_display_time(dp),
      //       (10000000 / caca_get_display_time(dp)) % 10);
      // ------------------------------------------------------------------

      xioctl(g_video_out.vid_params->v4l_fd, VIDIOC_QBUF, &(g_video_out.vid_params->buf));
    }
  }

  pthread_exit(NULL );
  return NULL ;
}

