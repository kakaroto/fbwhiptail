/*
 * cairo_dri.c : Cairo Direct Rendering surface implementation
 *
 * Copyright (c) 2018, Youness Alaoui (KaKaRoTo)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "cairo_dri.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>

#ifdef DRI_DEBUG
#define PERROR perror
#define PRINTF printf
#else
#define PERROR(...)
#define PRINTF(...)
#endif


static cairo_user_data_key_t user_data_key;

typedef struct {
  cairo_dri_t *dri;
  uint8_t *fb_data;
  uint32_t size;
  uint32_t fb_id;
  uint32_t handle;
  dri_screen_t *screen;
} dri_surface_fb_t;

/* Create a cairo surface using the specified framebuffer
 * can return an error if fb driver doesn't support double buffering
 */
cairo_dri_t *cairo_dri_open(const char *dri_filename)
{
  cairo_surface_t *surface;
  cairo_dri_t *device = NULL;
  uint32_t *res_fb_ids = NULL;
  uint32_t *res_crtc_ids = NULL;
  uint32_t *res_conn_ids = NULL;
  uint32_t *res_enc_ids = NULL;
  struct drm_mode_card_res res = {0};
  int i, j, k, l;

  device = malloc(sizeof(cairo_dri_t));
  if (device == NULL) {
    PERROR ("Error: can't allocate structure");
    return NULL;
  }

  memset (device, 0, sizeof(cairo_dri_t));
  // Open the file for reading and writing
  device->dri_fd = open(dri_filename, O_RDWR);
  if (device->dri_fd == -1) {
    PERROR ("Error: cannot open DRM device");
    goto handle_open_error;
  }

  device->master_is_set = ioctl(device->dri_fd, DRM_IOCTL_SET_MASTER, 0);

  //Get resource counts
  if (ioctl(device->dri_fd, DRM_IOCTL_MODE_GETRESOURCES, &res) == -1) {
    PERROR ("Error: reading resource count information");
    goto handle_ioctl_error;
  }

  if (res.count_fbs)
    res_fb_ids = malloc (sizeof(uint32_t) * res.count_fbs);
  if (res.count_crtcs)
    res_crtc_ids = malloc (sizeof(uint32_t) * res.count_crtcs);
  if (res.count_connectors)
    res_conn_ids = malloc (sizeof(uint32_t) * res.count_connectors);
  if (res.count_encoders)
    res_enc_ids = malloc (sizeof(uint32_t) * res.count_encoders);

  res.fb_id_ptr = (uint64_t) res_fb_ids;
  res.crtc_id_ptr = (uint64_t) res_crtc_ids;
  res.connector_id_ptr = (uint64_t) res_conn_ids;
  res.encoder_id_ptr = (uint64_t) res_enc_ids;

  //Get resource IDs
  if (ioctl(device->dri_fd, DRM_IOCTL_MODE_GETRESOURCES, &res) == -1) {
    PERROR ("Error: reading resource information");
    goto handle_ioctl_error;
  }

  device->screens = malloc (sizeof(dri_screen_t) * res.count_connectors);
  memset (device->screens, 0, sizeof(dri_screen_t) * res.count_connectors);
  device->num_screens = 0;
  for (i = 0; i < res.count_connectors; i++) {
    struct drm_mode_get_connector conn = {0};
    struct drm_mode_get_encoder enc = {0};
    struct drm_mode_modeinfo *conn_modes = NULL;
    uint32_t *conn_enc_ids = NULL;
    int32_t crtc = -1;

    conn.connector_id = res_conn_ids[i];

    // Get connector resource counts
    if (ioctl(device->dri_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) == -1) {
      PRINTF ("Can't get connector information for connector %d\n", conn.connector_id);
      continue;
    }

    // Verify the connector has at least one encoder and one mode and is physically connected
    if (conn.count_encoders < 1 || conn.count_modes < 1 || conn.connection != 1) {
      PRINTF ("Connector %d is not connected\n", conn.connector_id);
      continue;
    }
    if (conn.count_modes)
      conn_modes = malloc (sizeof(struct drm_mode_modeinfo) * conn.count_modes);
    if (conn.count_encoders)
      conn_enc_ids = malloc (sizeof(uint32_t) * conn.count_modes);

    // Reset count to 0 for the properties since we don't want to retreive them
    conn.count_props = 0;

    conn.modes_ptr = (uint64_t) conn_modes;
    conn.encoders_ptr = (uint64_t) conn_enc_ids;

    if (ioctl(device->dri_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) == -1) {
      free (conn_modes);
      free (conn_enc_ids);
      PRINTF ("Can't get connector information for connector %d\n", conn.connector_id);
      continue;
    }

    crtc = -1;
    if (conn.encoder_id) {
      enc.encoder_id = conn.encoder_id;
      if (ioctl(device->dri_fd, DRM_IOCTL_MODE_GETENCODER, &enc) == 0)
        crtc = enc.crtc_id;

      for (j = 0; j < device->num_screens; j++) {
        if (device->screens[j].crtc == crtc) {
          crtc = -1;
          break;
        }
      }
    }

    /* If crtc was not found, find one */
    for (j = 0; j < conn.count_encoders && crtc == -1; j++) {
      enc.encoder_id = conn_enc_ids[j];
      if (ioctl(device->dri_fd, DRM_IOCTL_MODE_GETENCODER, &enc) == -1) {
        PRINTF ("Can't get encoder information for encoder %d\n", enc.encoder_id);
        continue;
      }
      for (k = 0; k < res.count_crtcs && crtc == -1; k++) {
        /* check whether this CRTC works with the encoder */
        if (!(enc.possible_crtcs & (1 << k)))
          continue;
        crtc = res_crtc_ids[k];
        for (l = 0; l < device->num_screens; l++) {
          if (device->screens[l].crtc == crtc) {
            crtc = -1;
            break;
          }
        }
      }
    }

    if (crtc != -1) {
      struct drm_mode_crtc *saved_crtc = &(device->screens[device->num_screens].saved_crtc);
      device->screens[device->num_screens].conn = conn.connector_id;
      device->screens[device->num_screens].mode = conn_modes[0];
      device->screens[device->num_screens].crtc = crtc;
      saved_crtc->crtc_id = crtc;
      if (ioctl(device->dri_fd, DRM_IOCTL_MODE_GETCRTC, saved_crtc) == 0)
        device->num_screens++;
    }
    free (conn_modes);
    free (conn_enc_ids);
  }

  return device;

 handle_ioctl_error:
  close(device->dri_fd);
  if (res_fb_ids)
    free (res_fb_ids);
  if (res_crtc_ids)
    free (res_crtc_ids);
  if (res_conn_ids)
    free (res_conn_ids);
  if (res_enc_ids)
    free (res_enc_ids);
 handle_open_error:
  free (device);
  return NULL;
}

void cairo_dri_close(cairo_dri_t *dri)
{
  struct drm_mode_crtc crtc = {0};
  int i;

  for (i = 0; i < dri->num_screens; i++) {
    dri->screens[i].saved_crtc.set_connectors_ptr = (uint64_t)&dri->screens[i].conn;
    dri->screens[i].saved_crtc.count_connectors = 1;

    ioctl(dri->dri_fd, DRM_IOCTL_MODE_SETCRTC, &dri->screens[i].saved_crtc);
  }

  if (dri->master_is_set)
    ioctl(dri->dri_fd, DRM_IOCTL_DROP_MASTER, 0);
  close (dri->dri_fd);
  free (dri);
}

/* Destroy a cairo surface */
static void cairo_dri_surface_destroy(void *data)
{
  dri_surface_fb_t *fb = (dri_surface_fb_t *)data;
  struct drm_mode_destroy_dumb destroy_dumb = {0};
  struct drm_mode_crtc crtc = {0};

  if (fb == NULL)
    return;

  /* If current fb is mapped, restored saved fb */
  PRINTF ("Destroying surface with fb : %d\n", fb->fb_id);
  crtc.crtc_id = fb->screen->crtc;
  if (ioctl(fb->dri->dri_fd, DRM_IOCTL_MODE_GETCRTC, &crtc) == 0) {
    if (crtc.fb_id == fb->fb_id) {
      PRINTF ("Currently displayed FB, restoring saved one\n");
      crtc = fb->screen->saved_crtc;
      crtc.set_connectors_ptr = (uint64_t)&fb->screen->conn;
      crtc.count_connectors = 1;
      ioctl(fb->dri->dri_fd, DRM_IOCTL_MODE_SETCRTC, &crtc);
    }
  }
  munmap(fb->fb_data, fb->size);
  ioctl(fb->dri->dri_fd, DRM_IOCTL_MODE_RMFB, &fb->fb_id);
  destroy_dumb.handle = fb->handle;
  ioctl(fb->dri->dri_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
  free(fb);
}

cairo_surface_t *
cairo_dri_create_surface(cairo_dri_t *dri, dri_screen_t *screen)
{
  struct drm_mode_create_dumb create_dumb = {0};
  struct drm_mode_map_dumb map_dumb = {0};
  struct drm_mode_fb_cmd cmd_dumb = {0};
  struct drm_mode_destroy_dumb destroy_dumb = {0};
  cairo_surface_t *surface;
  dri_surface_fb_t *fb = NULL;
  uint8_t *fb_data;

  create_dumb.width = screen->mode.hdisplay;
  create_dumb.height = screen->mode.vdisplay;
  create_dumb.bpp = 32;
  create_dumb.flags = 0;

  if (ioctl(dri->dri_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb) == -1) {
    PERROR ("Can't create dumb buffer");
    return NULL;
  }

  cmd_dumb.width = create_dumb.width;
  cmd_dumb.height = create_dumb.height;
  cmd_dumb.bpp = create_dumb.bpp;
  cmd_dumb.pitch = create_dumb.pitch;
  cmd_dumb.depth = 24;
  cmd_dumb.handle = create_dumb.handle;
  if (ioctl(dri->dri_fd, DRM_IOCTL_MODE_ADDFB, &cmd_dumb) == -1) {
    PERROR ("Can't create dumb buffer");
    goto destroy_dumb;
  }

  map_dumb.handle = create_dumb.handle;
  if (ioctl(dri->dri_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb) == -1) {
    PERROR ("Can't map dumb buffer");
    goto remove_fb;
  }

  fb_data = mmap(0, create_dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED,
      dri->dri_fd, map_dumb.offset);
  if (fb_data == MAP_FAILED) {
    PERROR ("Failed to mmap frambuffer");
    goto remove_fb;
  }

  fb = malloc (sizeof(dri_surface_fb_t));
  fb->dri = dri;
  fb->fb_data = fb_data;
  fb->size = create_dumb.size;
  fb->handle = create_dumb.handle;
  fb->fb_id = cmd_dumb.fb_id;
  fb->screen = screen;

  PRINTF ("Created framebuffer %p of size %d (%dx%d) with id %d\n", fb->fb_data,
      fb->size, fb->screen->mode.hdisplay, fb->screen->mode.vdisplay, fb->fb_id);
  /* Create the cairo surface which will be used to draw to */
  surface = cairo_image_surface_create_for_data(fb_data,
      CAIRO_FORMAT_RGB24, cmd_dumb.width, cmd_dumb.height, cmd_dumb.pitch);

  cairo_surface_set_user_data(surface, &user_data_key, fb,
      &cairo_dri_surface_destroy);

  return surface;

 remove_fb:
  ioctl(dri->dri_fd, DRM_IOCTL_MODE_RMFB, &cmd_dumb.fb_id);
 destroy_dumb:
  destroy_dumb.handle = create_dumb.handle;
  ioctl(dri->dri_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
  return NULL;
}

int cairo_dri_flip_buffer(cairo_surface_t *surface, int vsync)
{
  dri_surface_fb_t *fb;
  struct drm_mode_crtc crtc = {0};

  fb = cairo_surface_get_user_data (surface, &user_data_key);

  if (fb == NULL)
    return -1;

  crtc = fb->screen->saved_crtc;
  crtc.crtc_id = fb->screen->crtc;
  crtc.fb_id = fb->fb_id;
  crtc.set_connectors_ptr = (uint64_t)&fb->screen->conn;
  crtc.count_connectors = 1;
  crtc.mode = fb->screen->mode;
  crtc.mode_valid = 1;

  if (ioctl(fb->dri->dri_fd, DRM_IOCTL_MODE_SETCRTC, &crtc) == -1) {
    PERROR ("Can't set CRTC information");
    return -1;
  }

  return 0;
}
