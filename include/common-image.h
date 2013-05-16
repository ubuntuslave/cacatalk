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

#include <linux/videodev2.h>
#include <libv4l2.h>
#include <caca.h>

struct buffer {
        void   *start;
        size_t length;
};

struct image
{
    char *pixels;
    unsigned int w, h;
    struct caca_dither *dither;
    void *priv;
};

// Local functions
extern struct image * load_image_from_V4L_buffer(struct v4l2_format * fmt, struct buffer * buf, int bytesused);
extern void unload_image(struct image *);

