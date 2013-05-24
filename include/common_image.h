/*
 *  Imaging tools for cacatalk
 *  Copyright (c) 2013 Carlos Jaramillo <cjaramillo@gc.cuny.edu>
 *                All Rights Reserved
 *
 *  This program is free software. It comes without any warranty, to
 *  the extent permitted by applicable law. You can redistribute it
 *  and/or modify it under the terms of the Do What the Fuck You Want
 *  to Public License, Version 2, as published by Sam Hocevar. See
 *  http://www.wtfpl.net/ for more details.
 */

#ifndef COMMON_IMAGE_H_
#define COMMON_IMAGE_H_

#include <linux/videodev2.h>
#include <libv4l2.h>
#include <caca.h>

/**
 * @brief A buffer place holder to be used with video streaming buffers given by the v4l device
 */
struct buffer
{
  void *start; ///< Indicates where the buffer begins in memory
  size_t length; ///< Indicates the size of the buffer
};

/**
 * @brief An image object to indicate the corresponding pixels of a video buffer in memory and other parameters,
 *      such as widht, height (pixels) and the caca dither format of choice.
 */
struct image
{
  char *pixels; ///< Points to the starting location in memory buffer
  unsigned int w, h; ///< width and height (in pixels), respectively
  struct caca_dither *dither; ///< The chosen libcaca dither format
  void *priv; ///< Unused
};

// Local functions
/**
 * @brief It loads an image from the v4l buffer
 *
 * @param fmt The v4l2 stream data format
 * @param buf The buffer structure to memory location
 * @param bytesused The number of bytes used by the buffer
 *
 * @return A shared pointer to the bytes allocated by the image structure
 */
extern struct image * load_image_from_V4L_buffer(struct v4l2_format * fmt, struct buffer * buf, int bytesused);

/**
 * @brief It unloads the image from the v4l memory buffer
 *
 * @param image The shared pointer to the bytes allocated to the image structure
 */
extern void unload_image(struct image *);

#endif // COMMON_IMAGE_H_
