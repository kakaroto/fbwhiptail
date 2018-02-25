/*
 * fbwhiptail_menu.h : Menu utility functions
 *
 * Copyright (C) Youness Alaoui (KaKaRoTo)
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

#ifndef __FBWHIPTAIL_MENU_H__
#define __FBWHIPTAIL_MENU_H__

#include <cairo/cairo.h>
#include "cairo_menu.h"
#include "cairo_utils.h"

typedef struct Menu_s Menu;

struct Menu_s {
  cairo_surface_t *background;
  CairoMenu *menu;
  int width;
  int height;
  const char *title;
  cairo_surface_t *frame;
  void (*callback) (Menu *menu, int accepted);
  void (*draw) (Menu *menu, cairo_t *cr);
};

#define STANDARD_MENU_ITEM_WIDTH (600)
#define STANDARD_MENU_ITEM_HEIGHT 60
#define STANDARD_MENU_PAD_X 10
#define STANDARD_MENU_PAD_Y 3
#define STANDARD_MENU_BOX_X 7
#define STANDARD_MENU_BOX_Y 7
#define STANDARD_MENU_ITEM_BOX_WIDTH (STANDARD_MENU_ITEM_WIDTH + \
      (2 * STANDARD_MENU_BOX_X))
#define STANDARD_MENU_ITEM_BOX_HEIGHT (STANDARD_MENU_ITEM_HEIGHT + \
      (2 * STANDARD_MENU_BOX_Y))
#define STANDARD_MENU_ITEM_TOTAL_WIDTH (STANDARD_MENU_ITEM_BOX_WIDTH + \
      (2 * STANDARD_MENU_PAD_X))
#define STANDARD_MENU_ITEM_TOTAL_HEIGHT (STANDARD_MENU_ITEM_BOX_HEIGHT + \
      (2 * STANDARD_MENU_PAD_Y))
#define STANDARD_MENU_ITEM_IPAD_X (STANDARD_MENU_BOX_X + CAIRO_MENU_DEFAULT_IPAD_X)
#define STANDARD_MENU_ITEM_IPAD_Y (STANDARD_MENU_BOX_Y + CAIRO_MENU_DEFAULT_IPAD_Y)
#define STANDARD_MENU_FRAME_SIDE 25
#define STANDARD_MENU_FRAME_TOP 60
#define STANDARD_MENU_FRAME_BOTTOM 40
#define STANDARD_MENU_FRAME_HEIGHT (STANDARD_MENU_FRAME_TOP + \
      STANDARD_MENU_FRAME_BOTTOM) // + nitems * STANDARD_MENU_ITEM_TOTAL_HEIGHT
#define STANDARD_MENU_FRAME_WIDTH (STANDARD_MENU_ITEM_TOTAL_WIDTH + \
      (2 * STANDARD_MENU_FRAME_SIDE))

#define STANDARD_MENU_WIDTH (STANDARD_MENU_ITEM_TOTAL_WIDTH)
#define STANDARD_MENU_HEIGHT (menu->height * 0.7)

#define STANDARD_MENU_BOX_CORNER_RADIUS 11
#define STANDARD_MENU_BOX_BORDER_WIDTH 1
#define STANDARD_MENU_FRAME_CORNER_RADIUS 32
#define STANDARD_MENU_FRAME_BORDER_WIDTH 2
#define STANDARD_MENU_TITLE_FONT_SIZE 25
#define MAIN_MENU_FONT_SIZE 15

void draw_background (Menu *menu, cairo_t *cr);
Menu *standard_menu_create (const char *title, int width, int height, int rows, int columns);
int standard_menu_add_item (Menu *menu, const char *title, int fontsize);
int standard_menu_add_tag (Menu *menu, const char *title, int fontsize);

#endif
