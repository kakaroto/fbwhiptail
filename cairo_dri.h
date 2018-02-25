/*
 * cairo_dri.c : Cairo DRI surface implementation
 *
 * Copyright (c) 2018, Youness Alaoui (KaKaRoTo)
 *
 */

#ifndef __CAIRO_DRI_H__
#define __CAIRO_DRI_H__

#include <cairo/cairo.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <stdint.h>

typedef struct {
  uint32_t conn;
  struct drm_mode_modeinfo mode;
  uint32_t crtc;
  struct drm_mode_crtc saved_crtc;
} dri_screen_t;

typedef struct {
  int dri_fd;
  int master_is_set;

  dri_screen_t *screens;
  int num_screens;
} cairo_dri_t ;

/*
 * Flip framebuffer, return the next buffer id which will be used or -1 if
 * an operation failed
 */
int cairo_dri_flip_buffer(cairo_surface_t *surface, int vsync);
/* Create a cairo surface using the specified framebuffer
 * can return an error if fb driver doesn't support double buffering
 */
cairo_dri_t *cairo_dri_open(const char *dri_filename);
void cairo_dri_close(cairo_dri_t *dri);
cairo_surface_t *cairo_dri_create_surface(cairo_dri_t *dri, dri_screen_t *screen);

#endif /* __CAIRO_DRI_H__ */
