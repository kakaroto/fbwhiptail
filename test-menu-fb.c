/*
 * test-menu-gtk.c : PS3 Menu testing application
 *
 * Copyright (C) Youness Alaoui (KaKaRoTo)
 *
 * This software is distributed under the terms of the GNU General Public
 * License ("GPL") version 3, as published by the Free Software Foundation.
 *
 *
*/


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

#include "cairo_menu.h"
#include "cairo_linuxfb.h"

static CairoMenu *menu = NULL;
static volatile sig_atomic_t cancel = 0;

void signal_handler(int signum)
{
	cancel = 1;
}


static void handle_input(short code)
{
    CairoMenuInput input;
    CairoMenuRectangle bbox;

    if (code == KEY_UP)
        input = CAIRO_MENU_INPUT_UP;
    else if (code == KEY_DOWN)
        input = CAIRO_MENU_INPUT_DOWN;
    else if (code == KEY_LEFT)
        input = CAIRO_MENU_INPUT_LEFT;
    else if (code == KEY_RIGHT)
        input = CAIRO_MENU_INPUT_RIGHT;
    else
      return;

    cairo_menu_handle_input (menu, input, &bbox);
}

int main(int argc, char **argv)
{
  int idx;
  cairo_surface_t *image = NULL;
  cairo_surface_t *surface = NULL;
  cairo_surface_t *fbsurface = NULL;
  cairo_t *cr;
  int xres, yres;
  struct sigaction action;
  int input;
  struct input_event ev[64];
  int fd, console, rd, value, size = sizeof (struct input_event);
  struct termios old, new;

  fd = open ("/dev/input/event0", O_RDONLY);
  console = -1;
  if (argc > 1) {
    console = open (argv[1], O_RDONLY);
  } else {
    console = 0;
  }
  ioctl(console,KDSETMODE,KD_GRAPHICS);
  /* Turn echoing off and fail if we can't. */
  tcgetattr (console, &old);
  new = old;
  new.c_lflag &= ~ECHO;
  tcsetattr (console, TCSAFLUSH, &new);

  memset(&action, 0, sizeof(struct sigaction));
  action.sa_handler = signal_handler;
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGINT, &action, NULL);

  fbsurface = cairo_linuxfb_surface_create("/dev/fb0", 1);
  cairo_linuxfb_get_resolution(fbsurface, &xres, &yres);
  cr = cairo_create(fbsurface);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, xres, yres);
  menu = cairo_menu_new (surface, -1, 3, (xres - 10) / 3, 100);

  idx = cairo_menu_add_item (menu, "TOP LEFT", 10);
  menu->items[idx].alignment = CAIRO_MENU_ALIGN_TOP_LEFT;
  idx = cairo_menu_add_item (menu, "TOP CENTER", 10);
  menu->items[idx].alignment = CAIRO_MENU_ALIGN_TOP_CENTER;
  idx = cairo_menu_add_item (menu, "TOP RIGHT", 10);
  menu->items[idx].alignment = CAIRO_MENU_ALIGN_TOP_RIGHT;
  idx = cairo_menu_add_item (menu, "MIDDLE LEFT", 10);
  menu->items[idx].alignment = CAIRO_MENU_ALIGN_MIDDLE_LEFT;
  idx = cairo_menu_add_item (menu, "MIDDLE CENTER", 10);
  menu->items[idx].alignment = CAIRO_MENU_ALIGN_MIDDLE_CENTER;
  idx = cairo_menu_add_item (menu, "MIDDLE RIGHT", 10);
  menu->items[idx].alignment = CAIRO_MENU_ALIGN_MIDDLE_RIGHT;
  idx = cairo_menu_add_item (menu, "BOTTOM LEFT", 10);
  menu->items[idx].alignment = CAIRO_MENU_ALIGN_BOTTOM_LEFT;
  idx = cairo_menu_add_item (menu, "BOTTOM CENTER", 10);
  menu->items[idx].alignment = CAIRO_MENU_ALIGN_BOTTOM_CENTER;
  idx = cairo_menu_add_item (menu, "BOTTOM RIGHT", 10);
  menu->items[idx].alignment = CAIRO_MENU_ALIGN_BOTTOM_RIGHT;
  idx = cairo_menu_add_item (menu, "Pattern", 15);
  menu->items[idx].alignment = CAIRO_MENU_ALIGN_MIDDLE_LEFT;
  image = cairo_image_surface_create_from_png ("pattern.png");
  cairo_menu_set_item_image (menu, idx, image, CAIRO_MENU_IMAGE_POSITION_RIGHT);
  cairo_surface_destroy (image);
  idx = cairo_menu_add_item (menu, "Rectangles", 15);
  menu->items[idx].alignment = CAIRO_MENU_ALIGN_MIDDLE_RIGHT;
  image = cairo_image_surface_create_from_png ("rect.png");
  cairo_menu_set_item_image (menu, idx, image, CAIRO_MENU_IMAGE_POSITION_LEFT);
  cairo_surface_destroy (image);
  cairo_menu_add_item (menu, "Hello world 3", 20);
  cairo_menu_add_item (menu, "Hello world 4", 20);
  cairo_menu_add_item (menu, "Hello world 5", 20);
  cairo_menu_add_item (menu, "Hello world 6", 20);
  cairo_menu_add_item (menu, "Hello world 7", 20);
  cairo_menu_add_item (menu, "Hello world 8", 20);
  cairo_menu_add_item (menu, "Hello world 9", 20);
  cairo_menu_add_item (menu, "Hello world 10", 15);
  cairo_menu_add_item (menu, "Hello world 11", 5);
  cairo_menu_add_item (menu, "Hello world 12", 5);
  cairo_menu_add_item (menu, "Hello world 13", 5);
  cairo_menu_add_item (menu, "Hello world 14", 5);
  cairo_menu_add_item (menu, "Hello world 5", 15);
  cairo_menu_add_item (menu, "Hello world 1", 10);
  cairo_menu_add_item (menu, "Hello world 2", 10);
  cairo_menu_add_item (menu, "Hello world 3", 10);
  cairo_menu_add_item (menu, "Hello world 4", 10);
  cairo_menu_add_item (menu, "Hello world 5", 10);
  cairo_menu_add_item (menu, "Hello world 6", 10);
  cairo_menu_add_item (menu, "Hello world 7", 10);
  cairo_menu_add_item (menu, "Hello world 8", 10);
  cairo_menu_add_item (menu, "Hello world 9", 10);
  cairo_menu_add_item (menu, "Hello world 10", 10);
  cairo_menu_add_item (menu, "Hello world 11", 10);
  cairo_menu_add_item (menu, "Hello world 12", 10);
  cairo_menu_add_item (menu, "Hello world 13", 10);
  cairo_menu_add_item (menu, "Hello world 14", 10);
  cairo_menu_add_item (menu, "Hello world 15", 10);

  cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint(cr);

  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  while (!cancel) {
    int i;
    cairo_menu_redraw (menu);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    if ((rd = read (fd, ev, size * 64)) < size) {
      break;
    }
    for (i = 0; i * size < rd; i++) {
      //printf ("Got event %d : %d - %d - %d\n", i, ev[i].type, ev[i].code, ev[i].value);
      if (ev[i].type == EV_KEY && ev[i].value != 0){ // key press and repeat
        handle_input (ev[i].code);
      }
    }
  }

  cairo_surface_destroy (surface);
  cairo_menu_free (menu);
  cairo_destroy(cr);
  cairo_surface_destroy (fbsurface);
  /* Restore terminal. */
  (void) tcsetattr (console, TCSAFLUSH, &old);
  ioctl(console,KDSETMODE,KD_TEXT);
  return 0;
}
