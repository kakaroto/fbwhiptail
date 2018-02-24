/*
 * cairo_fb.c : Cairo Linux Framebuffer surface implementation
 * Based on the demo application at https://github.com/toradex/cairo-fb-examples
 *
 * Copyright (c) 2015, Toradex AG
 * Copyright (c) 2018, Youness Alaoui (KaKaRoTo)
 *
 * This project is licensed under the terms of the MIT license (see
 * LICENSE)
 */

#ifndef __CAIRO_LINUXFB_H__
#define __CAIRO_LINUXFB_H__

#include <cairo/cairo.h>

/*
 * Flip framebuffer, return the next buffer id which will be used or -1 if
 * an operation failed
 */
int cairo_linuxfb_flip_buffer(cairo_surface_t *surface, int vsync, int bufid);
int cairo_linuxfb_get_resolution(cairo_surface_t *surface, int *xres, int *yres);
/* Create a cairo surface using the specified framebuffer
 * can return an error if fb driver doesn't support double buffering
 */
cairo_surface_t *cairo_linuxfb_surface_create(const char *fb_filename, int num_buffers);

#endif /* __CAIRO_LINUXFB_H__ */
