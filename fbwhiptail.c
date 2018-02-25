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

typedef struct {
  char *tag;
  char *item;
} whiptail_menu_item;

typedef struct {
  char *title;
  char *backtitle;
  char *text;
  char *default_item;
  int noitem;
  int notags;
  int topleft;
  int output_fd;
  int width;
  int height;
  int menu_height;
  whiptail_menu_item *items;
  int num_items;
} whiptail_args;

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

int parse_whiptail_args (int argc, char **argv, whiptail_args *args)
{
  int i;
  int end_of_args = 0, menu = 0;

  /*
  static whiptail_menu_item items[] = {
    {"first", "menu entry 1"},
    {"second", "blablabla"},
    {"foo", "bar"},
    {"default", "item"},
    {"usb1", "usb boot"},
    {"linux2", "Linux boot"},
    {"etc..", "you get the idea"},
  };
  args->title = "Whiptail test menu";
  args->backtitle = "background title";
  args->text = "some text to show in frame?";
  args->default_item = "default";
  args->noitem = 0;
  args->notags = 0;
  args->topleft = 0;
  args->output_fd = 2;
  args->width = 600;
  args->height = 400;
  args->menu_height = 10;
  args->items = items;
  args->num_items = 7;
  */
  memset (args, 0, sizeof(whiptail_args));
  args->output_fd = 2;

  for (i = 1; i < argc; i++) {
    if (end_of_args == 0 && strncmp (argv[i], "--", 2) == 0) {
      if (strcmp (argv[i], "--title") == 0) {
        i++;
        if (i >= argc)
          goto error;
        args->title = argv[i];
      } else if (strcmp (argv[i], "--backtitle") == 0) {
        i++;
        if (i >= argc)
          goto error;
        args->backtitle = argv[i];
      } else if (strcmp (argv[i], "--default-item") == 0) {
        i++;
        if (i >= argc)
          goto error;
        args->default_item = argv[i];
      } else if (strcmp (argv[i], "--output-fd") == 0) {
        i++;
        if (i >= argc)
          goto error;
        args->output_fd = atoi (argv[i]);
      }else if (strcmp (argv[i], "--noitem") == 0) {
        args->noitem = 1;
      } else if (strcmp (argv[i], "--notags") == 0) {
        args->notags = 1;
      } else if (strcmp (argv[i], "--topleft") == 0) {
        args->topleft = 1;
      } else if (menu == 0 && strcmp (argv[i], "--menu") == 0) {
        if (i + 4 >= argc)
          goto error;
        args->text = argv[i+1];
        args->height = atoi (argv[i+2]);
        args->width = atoi (argv[i+3]);
        args->menu_height = atoi (argv[i+4]);
        i += 4;
        menu = 1;
        args->items = malloc (sizeof(whiptail_menu_item) * (argc - i) / 2);
      } else if (strcmp (argv[i], "--") == 0) {
        end_of_args = 1;
      } else if (strcmp (argv[i], "--yes-button") == 0 ||
                 strcmp (argv[i], "--no-button") == 0 ||
                 strcmp (argv[i], "--ok-button") == 0 ||
                 strcmp (argv[i], "--cancel-button") == 0) {
        i++;
        if (i >= argc)
          goto error;
        // Ignore arguments
      } else if (strcmp (argv[i], "--clear") == 0 ||
          strcmp (argv[i], "--fb") == 0 ||
          strcmp (argv[i], "--fullbuttons") == 0 ||
          strcmp (argv[i], "--defaultno") == 0 ||
          strcmp (argv[i], "--nocancel") == 0 ||
          strcmp (argv[i], "--scrolltext") == 0 ||
          strcmp (argv[i], "--separate-output") == 0 ||
          strcmp (argv[i], "--help") == 0 ||
          strcmp (argv[i], "--version") == 0) {
        // Ignore arguments
      }
    } else if (menu) {
      if (i + 1 >= argc)
        goto error;

      args->items[args->num_items].tag = argv[i++];
      args->items[args->num_items].item = argv[i];
      args->num_items++;
    } else {
      goto error;
    }
  }
  if (menu == 0 || args->num_items == 0)
    goto error;
  return 0;
 error:
  if (args->items)
    free (args->items);
  return -1;
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
  whiptail_args args;
  int default_selection;
  cairo_t *cr;
#ifdef USE_LINUXFB
  cairo_surface_t *fbsurface = NULL;
#else
  cairo_dri_t *dri = NULL;
  cairo_surface_t *surfaces[2] = {NULL, NULL};
  cairo_t *crs[2] = {NULL, NULL};
  int current_fb = 0;
#endif

  if (parse_whiptail_args (argc, argv, &args) != 0) {
    printf ("Invalid arguments received\n");
    return -1;
  }

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
  if (dri && dri->num_screens > 0) {
    xres = dri->screens[0].mode.hdisplay;
    yres = dri->screens[0].mode.vdisplay;
    surfaces[0] = cairo_dri_create_surface(dri, &dri->screens[0]);
    surfaces[1] = cairo_dri_create_surface(dri, &dri->screens[0]);
    crs[0] = cairo_create(surfaces[0]);
    crs[1] = cairo_create(surfaces[1]);
  }
#endif

  /*
  menu = standard_menu_create (args.title, xres, yres,
      -1, (args.noitem || args.notags) ? 1 : 2);

  for (i = 0; i < args.num_items; i++) {
    if (args.notags == 0) {
      idx = standard_menu_add_tag (menu, args.items[i].tag, 10);
      if (1 || args.noitem)
        menu->menu->items[idx].enabled = 1;
    }
    if (args.noitem == 0) {
      idx = standard_menu_add_item (menu, args.items[i].item, 20);
    }
    if (args.default_item && strcmp (args.default_item, args.items[i].item) == 0) {
      CairoMenuRectangle bbox;
      cairo_menu_set_selection (menu->menu, idx, &bbox);
    }
  }
  */

  menu = standard_menu_create (args.title, xres, yres,-1, 1);

  idx = standard_menu_add_item (menu, args.text, 15);
  menu->menu->items[idx].enabled = 0;
  for (i = 0; i < args.num_items; i++) {
    char *text;
    whiptail_menu_item *item = &args.items[i];
    if (args.notags || args.noitem) {
      text = malloc ((args.notags ? strlen (item->item) : strlen (item->tag)) + 1);
      strcpy (text, args.notags ? item->item : item->tag);
    } else {
      text = malloc (strlen (item->item) + strlen (item->tag) + 1 + 3);
      strcpy (text, args.notags ? item->item : item->tag);
      strcat (text, " - ");
      strcat (text, item->item);
    }
    idx = standard_menu_add_item (menu, text, 20);
    menu->menu->items[idx].alignment = CAIRO_MENU_ALIGN_MIDDLE_LEFT;
    free (text);
    if (i == 0 ||
        (args.default_item && strcmp (args.default_item, item->tag) == 0)) {
      CairoMenuRectangle bbox;
      cairo_menu_set_selection (menu->menu, idx, &bbox);
    }
  }
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
        if (escape == 0)
          escape = 1;
        else
          cancel = 1;
        break;
      case 0xA:
        if (escape == 0) {
          cancel = 1;
          fprintf (stderr, "%s", args.items[menu->menu->selection].tag);
        }
        escape = 0;
        break;
      case 0x41:
      case 0x42:
      case 0x43:
      case 0x44:
        if (escape == 2) {
          redraw = handle_input (menu, c);
        }
      default:
        if (escape == 0) {
        }
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
