/*
 * fbwhiptail.c : Framebuffer Cairo Menu
 *
 * Copyright (C) Youness Alaoui (KaKaRoTo)
 * Written in 2011 for PS3, rewritten in 2018
 *
 * This software is distributed under the terms of the GNU General Public
 * License ("GPL") version 3, as published by the Free Software Foundation.
 *
 *
*/

//#define USE_LINUXFB

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>

#include <sys/time.h>

#include <cairo/cairo.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <signal.h>

#include "fbwhiptail_menu.h"
#ifdef USE_LINUXFB
#include "cairo_linuxfb.h"
#else
#include "cairo_dri.h"
#endif

static volatile sig_atomic_t cancel = 0;

void signal_handler(int signum)
{
	cancel = 1;
}

static int handle_input(Menu *menu, short code)
{
    CairoMenuInput input;
    CairoMenuRectangle bbox = {0, 0, 0, 0};

    if (code == 0x41)
        input = CAIRO_MENU_INPUT_UP;
    else if (code == 0x42)
        input = CAIRO_MENU_INPUT_DOWN;
    else if (code == 0x43)
        input = CAIRO_MENU_INPUT_RIGHT;
    else if (code == 0x44)
        input = CAIRO_MENU_INPUT_LEFT;
    else
      return 0;

    cairo_menu_handle_input (menu->menu, input, &bbox);
    return (bbox.width * bbox.height) != 0;
}

int main(int argc, char **argv)
{
  struct sigaction action;
  struct termios oldt, newt;
  int escape = 0;
  int i, idx;
  cairo_surface_t *image = NULL;
  int xres, yres;
  Menu *menu = NULL;
  int redraw = 1;
  cairo_t *cr;
#ifdef USE_LINUXFB
  cairo_surface_t *fbsurface = NULL;
#else
  cairo_dri_t *dri = NULL;
  cairo_surface_t *surfaces[2] = {NULL, NULL};
  cairo_t *crs[2] = {NULL, NULL};
  int current_fb = 0;
#endif

  memset(&action, 0, sizeof(struct sigaction));
  action.sa_handler = signal_handler;
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGINT, &action, NULL);

  tcgetattr( STDIN_FILENO, &oldt);
  newt = oldt;
  /* ICANON normally takes care that one line at a time will be processed
    that means it will return if it sees a "\n" or an EOF or an EOL*/
  newt.c_lflag &= ~(ICANON | ECHO);

  tcsetattr( STDIN_FILENO, TCSANOW, &newt);

#ifdef USE_LINUXFB
  fbsurface = cairo_linuxfb_surface_create("/dev/fb0", 1);
  cairo_linuxfb_get_resolution(fbsurface, &xres, &yres);
  cr = cairo_create(fbsurface);
#else
  dri = cairo_dri_open("/dev/dri/card0");
  printf ("Got DRI %p with %d screens : \n", dri, dri? dri->num_screens : 0);
  if (dri && dri->num_screens > 0) {
    for (i = 0; i < dri->num_screens; i++) {
      printf (" %d: %dx%d on connector %d using crtc %d\n", i,
          dri->screens[i].mode.hdisplay, dri->screens[i].mode.vdisplay,
          dri->screens[i].conn, dri->screens[i].crtc);
    }
    xres = dri->screens[0].mode.hdisplay;
    yres = dri->screens[0].mode.vdisplay;
    surfaces[0] = cairo_dri_create_surface(dri, &dri->screens[0]);
    surfaces[1] = cairo_dri_create_surface(dri, &dri->screens[0]);
    crs[0] = cairo_create(surfaces[0]);
    crs[1] = cairo_create(surfaces[1]);
  }
#endif

  menu = standard_menu_create ("Frambuffer Cairo menu", xres, yres);

  idx = standard_menu_add_item (menu, "TOP LEFT", 10);
  menu->menu->items[idx].alignment = CAIRO_MENU_ALIGN_TOP_LEFT;
  idx = standard_menu_add_item (menu, "TOP CENTER", 10);
  menu->menu->items[idx].alignment = CAIRO_MENU_ALIGN_TOP_CENTER;
  idx = standard_menu_add_item (menu, "TOP RIGHT", 10);
  menu->menu->items[idx].alignment = CAIRO_MENU_ALIGN_TOP_RIGHT;
  idx = standard_menu_add_item (menu, "MIDDLE LEFT", 10);
  menu->menu->items[idx].alignment = CAIRO_MENU_ALIGN_MIDDLE_LEFT;
  idx = standard_menu_add_item (menu, "MIDDLE CENTER", 10);
  menu->menu->items[idx].alignment = CAIRO_MENU_ALIGN_MIDDLE_CENTER;
  idx = standard_menu_add_item (menu, "MIDDLE RIGHT", 10);
  menu->menu->items[idx].alignment = CAIRO_MENU_ALIGN_MIDDLE_RIGHT;
  idx = standard_menu_add_item (menu, "BOTTOM LEFT", 10);
  menu->menu->items[idx].alignment = CAIRO_MENU_ALIGN_BOTTOM_LEFT;
  idx = standard_menu_add_item (menu, "BOTTOM CENTER", 10);
  menu->menu->items[idx].alignment = CAIRO_MENU_ALIGN_BOTTOM_CENTER;
  idx = standard_menu_add_item (menu, "BOTTOM RIGHT", 10);
  menu->menu->items[idx].alignment = CAIRO_MENU_ALIGN_BOTTOM_RIGHT;
  idx = standard_menu_add_item (menu, "Pattern", 15);
  menu->menu->items[idx].alignment = CAIRO_MENU_ALIGN_MIDDLE_LEFT;
  image = cairo_image_surface_create_from_png ("pattern.png");
  cairo_menu_set_item_image (menu->menu, idx, image, CAIRO_MENU_IMAGE_POSITION_RIGHT);
  cairo_surface_destroy (image);
  idx = standard_menu_add_item (menu, "Rectangles", 15);
  menu->menu->items[idx].alignment = CAIRO_MENU_ALIGN_MIDDLE_RIGHT;
  image = cairo_image_surface_create_from_png ("rect.png");
  cairo_menu_set_item_image (menu->menu, idx, image, CAIRO_MENU_IMAGE_POSITION_LEFT);
  cairo_surface_destroy (image);
  standard_menu_add_item (menu, "Hello world 3", 20);
  standard_menu_add_item (menu, "Hello world 4", 20);
  standard_menu_add_item (menu, "Hello world 5", 20);
  standard_menu_add_item (menu, "Hello world 6", 20);
  standard_menu_add_item (menu, "Hello world 7", 20);
  standard_menu_add_item (menu, "Hello world 8", 20);
  standard_menu_add_item (menu, "Hello world 9", 20);
  standard_menu_add_item (menu, "Hello world 10", 15);
  standard_menu_add_item (menu, "Hello world 11", 5);
  standard_menu_add_item (menu, "Hello world 12", 5);
  standard_menu_add_item (menu, "Hello world 13", 5);
  standard_menu_add_item (menu, "Hello world 14", 5);
  standard_menu_add_item (menu, "Hello world 5", 15);
  standard_menu_add_item (menu, "Hello world 1", 10);
  standard_menu_add_item (menu, "Hello world 2", 10);
  standard_menu_add_item (menu, "Hello world 3", 10);
  standard_menu_add_item (menu, "Hello world 4", 10);
  standard_menu_add_item (menu, "Hello world 5", 10);
  standard_menu_add_item (menu, "Hello world 6", 10);
  standard_menu_add_item (menu, "Hello world 7", 10);
  standard_menu_add_item (menu, "Hello world 8", 10);
  standard_menu_add_item (menu, "Hello world 9", 10);
  standard_menu_add_item (menu, "Hello world 10", 10);
  standard_menu_add_item (menu, "Hello world 11", 10);
  standard_menu_add_item (menu, "Hello world 12", 10);
  standard_menu_add_item (menu, "Hello world 13", 10);
  standard_menu_add_item (menu, "Hello world 14", 10);
  standard_menu_add_item (menu, "Hello world 15", 10);

  while (!cancel) {
    char c;

    if (redraw) {
#ifndef USE_LINUXFB
      cr = crs[current_fb];
#endif
      draw_background (menu, cr);
      menu->draw (menu, cr);
#ifndef USE_LINUXFB
      if (cairo_dri_flip_buffer (surfaces[current_fb], 0) != 0) {
        printf ("Flip failed. Cancelling\n");
        break;
      }
      current_fb = (current_fb + 1) % 2;
#endif
      redraw = 0;
    }

    switch (c = getchar ()) {
      case 0x1B: // Escape character
        escape = 1;
        break;
      case 0xA:
        if (escape == 0) {
          printf ("Selection is %d: %s\n", menu->menu->selection,
              menu->menu->items[menu->menu->selection].text);
          cancel = 1;
        }
        break;
      case 0x41:
      case 0x42:
      case 0x43:
      case 0x44:
        if (escape == 2) {
          redraw = handle_input (menu, c);
        }
      default:
        if (escape == 1) {
          escape = 2;
        } else {
          escape = 0;
        }
        break;
    }
  }

  /*restore the old settings*/
  tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
#ifdef USE_LINUXFB
  cairo_destroy(cr);
  cairo_surface_destroy (fbsurface);
#else
  cairo_destroy(crs[0]);
  cairo_destroy(crs[1]);
  cairo_surface_destroy (surfaces[0]);
  cairo_surface_destroy (surfaces[1]);
  cairo_dri_close (dri);
#endif

  cairo_menu_free (menu->menu);
  free (menu);

  return 0;
}
