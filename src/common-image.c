/*
 *  Imaging tools for cacaview and img2irc
 *  Copyright (c) 2002-2012 Sam Hocevar <sam@hocevar.net>
 *                All Rights Reserved
 *
 *  This program is free software. It comes without any warranty, to
 *  the extent permitted by applicable law. You can redistribute it
 *  and/or modify it under the terms of the Do What the Fuck You Want
 *  to Public License, Version 2, as published by Sam Hocevar. See
 *  http://www.wtfpl.net/ for more details.
 */

#if !defined(__KERNEL__)
#   include <string.h>
#   include <stdlib.h>
#endif

#include "common-image.h"

struct image * load_image_from_V4L_buffer(struct v4l2_format * fmt, struct buffer * buf, int bytesused)
{
  struct image * im = malloc(sizeof(struct image));
  unsigned int depth, bpp, rmask, gmask, bmask, amask;
  unsigned int i;

  uint32_t red[256], green[256], blue[256], alpha[256];

  if (!fmt || !buf)
  {
    free(im);
    return NULL;
  }

  im->w = fmt->fmt.pix.width;
  im->h = fmt->fmt.pix.height;
  bpp = 24; // TODO: find it from structs

  depth = (bpp + 7) / 8;

  // Sanity check
  if (!im->w || im->w > 0x10000 || !im->h || im->h > 0x10000)
  {
    free(im);
    return NULL;
  }

  // Allocate the pixel buffer
//  im->pixels = malloc(im->w * im->h * depth);
   im->pixels = malloc(bytesused); // Alternative

  if (!im->pixels)
  {
    free(im);
    return NULL;
  }

//  memset(im->pixels, 0, im->w * im->h * depth);
  memset(im->pixels, 0, bytesused);

  // Copy the bitmap data
  im->pixels = buf->start; // Using just assignment
  // More secure, but it needs to unload_image() from memory
  // memcpy (im->pixels, buf->start, bytesused * sizeof (char));

  /*
  // -----------------------------------------------------
  // TEST writing to PPM images
  char                            out_name[256];
  FILE                            *fout;
  static int count = 0;
  sprintf(out_name, "out%03d.ppm", count); // file name
  count++;
  fout = fopen(out_name, "w");
  if (!fout) {
          perror("Cannot open image");
          exit(EXIT_FAILURE);
  }
  // Header for the PPM file
  fprintf(fout, "P6\n%d %d 255\n",
          im->w, im->h);

  // Writing actual pixel data
  // Note: buffers seem to alternate (Double buffer): buf.index is 0/1 (2 requests)
  fwrite(im->pixels, bytesused, 1, fout);
  fclose(fout);
  // -----------------------------------------------------
  */

  switch (depth)
  {
    case 3:
      rmask = 0x0000ff;
      gmask = 0x00ff00;
      bmask = 0xff0000;
      amask = 0x000000;
      break;
    case 2: // XXX: those are the 16 bits values
      rmask = 0x001f;
      gmask = 0x03e0;
      bmask = 0x7c00;
      amask = 0x0000;
      break;
    case 1:
    default:
      bpp = 8;
      rmask = gmask = bmask = amask = 0;
      break;
  }

  // Create the libcaca dither
  im->dither = caca_create_dither(bpp, im->w, im->h, depth * im->w, rmask, gmask, bmask, amask);
  if (!im->dither)
  {
    free(im->pixels);
    free(im);
    return NULL;
  }

  if (bpp == 8) // Only for 8 bpp dither objects
  {
    // Fill the rest of the palette FIXME: blind mapping
    for (i = 0; i < 256; i++)
    {
      blue[i] = green[i] = red[i] = alpha[i] = 0;
    }
    caca_set_dither_palette(im->dither, red, green, blue, alpha);
  }

  return im;
}

void unload_image(struct image * im)
{
  free(im->pixels);
  caca_free_dither(im->dither);
}

