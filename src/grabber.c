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
#include <linux/videodev2.h>
#include <libv4l2.h>

#include "cacatalk_common.h"
#include "common_image.h"
#include "caca_socket.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

int chat(caca_canvas_t *cv, caca_display_t *dp, int sockfd);
int grab(caca_canvas_t *cv, caca_display_t *dp, char *dev_name, int img_width, int img_height);

void xioctl(int fh, int request, void *arg);

int main(int argc, char **argv)
{
  void (*demo)(void * arg1, ...) = NULL;
  int quit = 0;
  caca_canvas_t *cv;
  caca_display_t *dp;
  int img_width = 640;
  int img_height = 480;
  char *dev_name;

  // TODO: use getopt to pass arguments
  if(argc > 1)
    strcpy(dev_name, argv[1]);
  else
    dev_name = "/dev/video0";

  // ----------------------------------------------------------------------
  // -------   Socket related:  -------------------------------------------
  int sockfd;
  char ip_name[256] = "";
  char recvline[MAXLINE]; // receives data from socket
  char sendline[MAXLINE]; // sends data to socket
  // Check if there is a host name on command line; if not use default
  if (argc > 2)
  {
    struct sockaddr_in server;
    strcpy(ip_name, argv[2]);
    sockfd = connect_to_peer_socket(ip_name, &server);
    printf("Connected to FD: %d\n", sockfd);
  }
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

  /* Initialize data */
#if 0
  sprite = caca_load_sprite("caca.txt");
  if(!sprite)
  sprite = caca_load_sprite("examples/caca.txt");
#endif

  // Disable cursor */
  caca_set_mouse(dp, 0);

  // Main menu //TODO
//  display_menu();
//  caca_refresh_display(dp);

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
            demo = grab;
            break;
          case 'c':
          case 'C':
            key_choice = 'c';
            demo = chat;
            break;
        }

        if (demo)
        {
          caca_set_color_ansi(cv, CACA_DEFAULT, CACA_TRANSPARENT);
//                caca_set_color_ansi(cv, CACA_LIGHTGRAY, CACA_BLACK);
          caca_clear_canvas(cv);
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

    if (demo)
    {
      if(key_choice == 'v')
        demo(cv, dp, img_width, img_height);

      if(key_choice == 'c')
        demo(cv, dp, sockfd);

      caca_clear_canvas(cv);
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
  }

// Clean up caca
  caca_free_display(dp);
  caca_free_canvas(cv);

  return 0;
}

int chat(caca_canvas_t *cv, caca_display_t *dp, int sockfd)
{
  textentry entries[TEXT_ENTRIES];
  char * sendline; // Buffer to send through socket
  char * recline;  // Buffer to receive from socket
  unsigned int i, e = 0, running = 1;

  caca_set_cursor(dp, 1);

  caca_set_color_ansi(cv, CACA_WHITE, CACA_BLUE);
  caca_put_str(cv, 1, 1, "Text entries - press tab to cycle");

  for (i = 0; i < TEXT_ENTRIES; i++)
  {
    entries[i].buffer[0] = 0;
    entries[i].size = 0;
    entries[i].cursor = 0;
    entries[i].changed = 1;
    caca_printf(cv, 3, 3 * i + 4, "[entry %i]", i + 1);
  }

  while (running)
  {
    caca_event_t ev;

    for (i = 0; i < TEXT_ENTRIES; i++)
    {
      unsigned int j, start, size;

      if (!entries[i].changed)
        continue;

      caca_set_color_ansi(cv, CACA_BLACK, CACA_LIGHTGRAY);
      caca_fill_box(cv, 2, 3 * i + 5, BUFFER_SIZE + 1, 1, ' ');

      start = 0;
      size = entries[i].size;

      for (j = 0; j < size; j++)
      {
        caca_put_char(cv, 2 + j, 3 * i + 5, entries[i].buffer[start + j]);
      }

      entries[i].changed = 0;
    }

    /* Put the cursor on the active textentry */
    caca_gotoxy(cv, 2 + entries[e].cursor, 3 * e + 5);

    caca_refresh_display(dp);

    if (caca_get_event(dp, CACA_EVENT_KEY_PRESS, &ev, -1) == 0)
      continue;

    switch (caca_get_event_key_ch(&ev))
    {
      case CACA_KEY_ESCAPE:
        running = 0;
        break;
      case CACA_KEY_TAB:
      case CACA_KEY_RETURN:
        // Send line through socket
        /* FIXME
        sendline = malloc(entries[e].size+1);
        recline = malloc(MAXLINE);

        memset(sendline, 0, entries[e].size);
        memset(recline, MAXLINE, sizeof(char));
        int j;
        for (j = 0; j < entries[e].size; j++)
        {
          sendline[j] = (char) entries[e].buffer[j];
        }
        sendline[j] = '\0'; // Null character terminated string
        send_receive_data_through_socket(sockfd, sendline, recline);
        */
        e = (e + 1) % TEXT_ENTRIES; // Switch to next line
        break;
      case CACA_KEY_HOME:
        entries[e].cursor = 0;
        break;
      case CACA_KEY_END:
        entries[e].cursor = entries[e].size;
        break;
      case CACA_KEY_LEFT:
        if (entries[e].cursor)
          entries[e].cursor--;
        break;
      case CACA_KEY_RIGHT:
        if (entries[e].cursor < entries[e].size)
          entries[e].cursor++;
        break;
      case CACA_KEY_DELETE:
        if (entries[e].cursor < entries[e].size)
        {
          memmove(entries[e].buffer + entries[e].cursor, entries[e].buffer + entries[e].cursor + 1,
                  (entries[e].size - entries[e].cursor + 1) * 4);
          entries[e].size--;
          entries[e].changed = 1;
        }
        break;
      case CACA_KEY_BACKSPACE:
        if (entries[e].cursor)
        {
          memmove(entries[e].buffer + entries[e].cursor - 1, entries[e].buffer + entries[e].cursor,
                  (entries[e].size - entries[e].cursor) * 4);
          entries[e].size--;
          entries[e].cursor--;
          entries[e].changed = 1;
        }
        break;
      default:
        if (entries[e].size < BUFFER_SIZE)
        {
          memmove(entries[e].buffer + entries[e].cursor + 1, entries[e].buffer + entries[e].cursor,
                  (entries[e].size - entries[e].cursor) * 4);
          entries[e].buffer[entries[e].cursor] = caca_get_event_key_utf32(&ev);
          entries[e].size++;
          entries[e].cursor++;
          entries[e].changed = 1;
        }
        break;
    }
  }

  return 0;
}

int grab(caca_canvas_t *cv, caca_display_t *dp, char *dev_name, int img_width, int img_height)
{
  struct v4l2_format fmt;
  struct v4l2_buffer buf;
  struct v4l2_requestbuffers req;
  enum v4l2_buf_type type;
  fd_set fds;
  struct timeval tv;
  int r, fd = -1;
  unsigned int i, n_buffers;
  char *dev_name = "/dev/video0";
  struct buffer *buffers;

  //-----------------------------------------
  // caca test: from img2txt
  void *export;
  char *format = NULL;
  size_t len;
  unsigned int font_width = 6, font_height = 10;
  float aspect_ratio = ((float)img_width / (float)img_height) * ((float)font_height / font_width);
  unsigned int lines = caca_get_canvas_height(cv);
  unsigned int cols = (unsigned int)(aspect_ratio * (float)lines);

  //char *format = NULL;
  char *dither = NULL;
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
//  caca_set_color_ansi(cv, CACA_DEFAULT, CACA_TRANSPARENT); //TODO delete
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
      do // FIXME: timing issues with key events from caca
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
        return errno;
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
        return 1;
      }

      caca_clear_canvas(cv);
      if (caca_set_dither_algorithm(im->dither, dither ? dither : "fstein"))
      {
        fprintf(stderr, "grab: Can't dither image with algorithm '%s'\n", dither);
        unload_image(im);
        caca_free_canvas(cv);
        return -1;
      }

      if (brightness != -1)
        caca_set_dither_brightness(im->dither, brightness);
      if (contrast != -1)
        caca_set_dither_contrast(im->dither, contrast);
      if (gamma != -1)
        caca_set_dither_gamma(im->dither, gamma);

      caca_dither_bitmap(cv, 0, 0, cols, lines, im->dither, im->pixels);

      //unload_image(im); // Enable it if using memcpy in the loading

      // TODO: This is what needs to be sent through the socket
      /*
       export = caca_export_canvas_to_memory(cv, format ? format : "ansi", &len);
       if (!export)
       {
       fprintf(stderr, "grab: Can't export to format '%s'\n", format);
       }
       else
       {
       fwrite(export, len, 1, stdout);
       free(export);
       }
       */

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

  return 0;
}

void xioctl(int fh, int request, void *arg)
{
  int r;

  do
  {
    r = v4l2_ioctl(fh, request, arg);
  } while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));

  if (r == -1)
  {
    fprintf(stderr, "error %d, %s\n", errno, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

