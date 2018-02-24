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

#include "cairo_linuxfb.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>


typedef struct _cairo_linuxfb_device {
  int fb_fd;
  unsigned char *fb_data;
  unsigned char *previous_fb_data;
  struct fb_var_screeninfo fb_vinfo;
  struct fb_fix_screeninfo fb_finfo;
  int num_buffers;
  int buffer_id;
} cairo_linuxfb_device_t;

static cairo_user_data_key_t user_data_key;

/*
 * Flip framebuffer, return the next buffer id which will be used or -1 if
 * an operation failed
 */
int cairo_linuxfb_flip_buffer(cairo_surface_t *surface, int vsync, int bufid)
{
  cairo_linuxfb_device_t *device;

  device = cairo_surface_get_user_data (surface, &user_data_key);

  if (device == NULL)
    return -1;

  if (device->buffer_id != bufid && bufid < device->num_buffers) {
    /* Pan the framebuffer */
    device->fb_vinfo.yoffset = device->fb_vinfo.yres * bufid;
    if (ioctl(device->fb_fd, FBIOPAN_DISPLAY, &device->fb_vinfo)) {
      perror("Error panning display");
      return -1;
    }
    device->buffer_id = bufid;
  }

  if (vsync) {
    int dummy = 0;
    if (ioctl(device->fb_fd, FBIO_WAITFORVSYNC, &dummy)) {
      perror("Error waiting for VSYNC");
      return -1;
    }
  }

  return (bufid+1) % device->num_buffers;
}

int cairo_linuxfb_get_resolution(cairo_surface_t *surface, int *xres, int *yres)
{
  cairo_linuxfb_device_t *device;

  device = cairo_surface_get_user_data (surface, &user_data_key);

  if (device == NULL)
    return -1;

  *xres = device->fb_vinfo.xres;
  *yres  = device->fb_vinfo.yres;

  return 0;
}


/* Destroy a cairo surface */
static void cairo_linuxfb_surface_destroy(void *device)
{
  cairo_linuxfb_device_t *dev = (cairo_linuxfb_device_t *)device;

  if (dev == NULL)
    return;

  memcpy (dev->fb_data, dev->previous_fb_data, dev->fb_finfo.smem_len);
  free(dev->previous_fb_data);
  munmap(dev->fb_data, dev->fb_finfo.smem_len);
  close(dev->fb_fd);
  free(dev);
}

/* Create a cairo surface using the specified framebuffer
 * can return an error if fb driver doesn't support double buffering
 */
cairo_surface_t *cairo_linuxfb_surface_create(const char *fb_filename, int num_buffers)
{
  cairo_surface_t *surface;
  cairo_linuxfb_device_t *device;

  device = malloc(sizeof(cairo_linuxfb_device_t));
  if (device == NULL) {
    perror ("Error: can't allocate structure");
    return NULL;
  }
  // Open the file for reading and writing
  device->fb_fd = open(fb_filename, O_RDWR);
  if (device->fb_fd == -1) {
    perror("Error: cannot open framebuffer device");
    goto handle_open_error;
  }

  // Get variable screen information
  if (ioctl(device->fb_fd, FBIOGET_VSCREENINFO, &device->fb_vinfo) == -1) {
    perror("Error: reading variable information");
    goto handle_ioctl_error;
  }

  /*
  printf("Frame buffer var screen info : \n");
  printf("  X Resolution : %u\n", device->fb_vinfo.xres);
  printf("  Y Resolution : %u\n", device->fb_vinfo.yres);
  printf("  X Virtual Resolution : %u\n", device->fb_vinfo.xres_virtual);
  printf("  Y Virtual Resolution : %u\n", device->fb_vinfo.yres_virtual);
  printf("  X Offset : %u\n", device->fb_vinfo.xoffset);
  printf("  X Offset : %u\n", device->fb_vinfo.yoffset);
  printf("  Bits per pixel : %u\n", device->fb_vinfo.bits_per_pixel);
  printf("  Grayscale : %u\n", device->fb_vinfo.grayscale);
  */

  /* Set virtual display size double the width for double buffering */
  device->fb_vinfo.bits_per_pixel = 32;
  device->fb_vinfo.yoffset = 0;
  device->fb_vinfo.yres_virtual = device->fb_vinfo.yres * num_buffers;
  if (ioctl(device->fb_fd, FBIOPUT_VSCREENINFO, &device->fb_vinfo)) {
    perror("Error setting variable screen info from fb");
    goto handle_ioctl_error;
  }

  // Get fixed screen information
  if (ioctl(device->fb_fd, FBIOGET_FSCREENINFO, &device->fb_finfo) == -1) {
    perror("Error reading fixed information");
    goto handle_ioctl_error;
  }
  /*

  printf("Frame buffer fixed screen info : \n");
  printf("  ID : %16s\n", device->fb_finfo.id);
  printf("  Line length : %u\n", device->fb_finfo.line_length);
  printf("  Smem length : %u\n", device->fb_finfo.smem_len);
  */

  // Map the device to memory
  device->fb_data = (unsigned char *)mmap(0, device->fb_finfo.smem_len,
      PROT_READ | PROT_WRITE, MAP_SHARED,
      device->fb_fd, 0);
  if (device->fb_data == MAP_FAILED) {
    perror("Error: failed to map framebuffer device to memory");
    goto handle_ioctl_error;
  }

  device->previous_fb_data = malloc (device->fb_finfo.smem_len);
  if (device->previous_fb_data)
    memcpy (device->previous_fb_data, device->fb_data,
        device->fb_finfo.smem_len);

  /* Create the cairo surface which will be used to draw to */
  // TODO: Actually verify the format
  surface = cairo_image_surface_create_for_data(device->fb_data,
      CAIRO_FORMAT_RGB24,
      device->fb_vinfo.xres,
      device->fb_vinfo.yres_virtual,
      cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32,
          device->fb_vinfo.xres));
  cairo_surface_set_user_data(surface, &user_data_key, device,
      &cairo_linuxfb_surface_destroy);

  return surface;

 handle_ioctl_error:
  close(device->fb_fd);
 handle_open_error:
  free(device);
  return NULL;
}
