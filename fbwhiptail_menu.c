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

#include "fbwhiptail_menu.h"

void
draw_background (Menu *menu, cairo_t *cr)
{
  /* Pre-cache the gradient background pattern into a cairo image surface.
   * The cairo_pattern is a vector basically, so when painting the gradient
   * into our surface, it needs to rasterize it, which makes the FPS drop
   * to 6 or 7 FPS and makes the game unusable (misses controller input, and
   * animations don't work anymore). So by pre-rasterizing it into an
   * image surface, we can do the background paint very quickly and FPS should
   * stay at 60fps or if we miss the VSYNC, drop to 30fps.
   */
  if (menu->background == NULL) {
    cairo_pattern_t *linpat = NULL;
    cairo_t *grad_cr = NULL;

    menu->background = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
        menu->width, menu->height);

    linpat = cairo_pattern_create_linear (0, 0,
        menu->width, menu->height);
    cairo_pattern_add_color_stop_rgb (linpat, 0, 0, 0.3, 0.8);
    cairo_pattern_add_color_stop_rgb (linpat, 1, 0, 0.8, 0.3);

    grad_cr = cairo_create (menu->background);
    cairo_set_source (grad_cr, linpat);
    cairo_paint (grad_cr);
    cairo_destroy (grad_cr);
    cairo_pattern_destroy (linpat);
    cairo_surface_flush (menu->background);
  }
  cairo_set_source_surface (cr, menu->background, 0, 0);
  cairo_paint (cr);
}

static void
create_standard_menu_frame (Menu *menu)
{
  cairo_font_extents_t fex;
  cairo_text_extents_t tex;
  cairo_pattern_t *linpat = NULL;
  cairo_surface_t *frame = NULL;
  int width, height;
  cairo_t *cr;
  int x, y;

  /* Adapt frame height depending on items in the menu */
  width = STANDARD_MENU_FRAME_WIDTH;
  height = menu->menu->nitems * STANDARD_MENU_ITEM_TOTAL_HEIGHT;
  if (height > STANDARD_MENU_HEIGHT)
    height = STANDARD_MENU_HEIGHT;
  height += STANDARD_MENU_FRAME_HEIGHT;

  frame = cairo_image_surface_create  (CAIRO_FORMAT_ARGB32,
      width, height);
  cr = cairo_create (frame);
  cairo_utils_clip_round_edge (cr, width, height,
      STANDARD_MENU_FRAME_CORNER_RADIUS, STANDARD_MENU_FRAME_CORNER_RADIUS,
      STANDARD_MENU_FRAME_CORNER_RADIUS);
  linpat = cairo_pattern_create_linear (width, 0, width, height);

  cairo_pattern_add_color_stop_rgb (linpat, 0.0, 0.3, 0.3, 0.3);
  cairo_pattern_add_color_stop_rgb (linpat, 1.0, 0.8, 0.8, 0.8);

  cairo_set_source (cr, linpat);
  cairo_paint (cr);
  cairo_pattern_destroy (linpat);

  linpat = cairo_pattern_create_linear (width, 0, width, height);

  cairo_pattern_add_color_stop_rgb (linpat, 0.0, 0.03, 0.07, 0.10);
  cairo_pattern_add_color_stop_rgb (linpat, 0.1, 0.04, 0.09, 0.16);
  cairo_pattern_add_color_stop_rgb (linpat, 0.5, 0.05, 0.20, 0.35);
  cairo_pattern_add_color_stop_rgb (linpat, 1.0, 0.06, 0.55, 0.75);

  cairo_utils_clip_round_edge (cr, width, height,
      STANDARD_MENU_FRAME_CORNER_RADIUS + STANDARD_MENU_FRAME_BORDER_WIDTH,
      STANDARD_MENU_FRAME_CORNER_RADIUS + STANDARD_MENU_FRAME_BORDER_WIDTH,
      STANDARD_MENU_FRAME_CORNER_RADIUS);

  cairo_set_source (cr, linpat);
  cairo_paint (cr);
  cairo_pattern_destroy (linpat);

  cairo_select_font_face(cr, "Arial",
      CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_BOLD);

  cairo_set_font_size(cr, STANDARD_MENU_TITLE_FONT_SIZE);

  cairo_font_extents (cr, &fex);
  cairo_text_extents (cr, menu->title, &tex);

  y = (STANDARD_MENU_FRAME_TOP / 2) + (fex.ascent / 2);
  x = ((width - tex.width) / 2) - tex.x_bearing;
  cairo_move_to(cr, x, y);
  cairo_set_source_rgb (cr, 0.0, 0.3, 0.5);
  cairo_show_text (cr, menu->title);
  cairo_destroy (cr);

  /* Create the frame with a dropshadow */
  menu->frame = cairo_utils_surface_add_dropshadow (frame, 3);
  cairo_surface_destroy (frame);
}

static void
draw_standard_menu (Menu *menu, cairo_t *cr)
{
  cairo_surface_t *surface;
  int w, h;
  int menu_width;
  int menu_height;

  if (menu->frame == NULL) {
    create_standard_menu_frame (menu);
  }

  cairo_utils_get_surface_size (menu->frame, &w, &h);
  surface = cairo_menu_get_surface (menu->menu);

  cairo_menu_redraw (menu->menu);

  cairo_set_source_surface (cr, menu->frame, (menu->width - w) / 2,
      (menu->height - h) / 2);
  cairo_paint (cr);

  /* Draw a frame around the menu so cut off buttons don't appear clippped */

  menu_width = STANDARD_MENU_WIDTH;
  menu_height = menu->menu->nitems * STANDARD_MENU_ITEM_TOTAL_HEIGHT;

  if (menu_height > STANDARD_MENU_HEIGHT)
    menu_height = STANDARD_MENU_HEIGHT;

  cairo_save (cr);
  cairo_translate (cr, ((menu->width - w) / 2) + STANDARD_MENU_FRAME_SIDE,
      ((menu->height - h) / 2) + STANDARD_MENU_FRAME_TOP);
  cairo_utils_clip_round_edge (cr, menu_width, menu_height, 20, 20, 20);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint_with_alpha (cr, 0.5);

  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);
  cairo_restore (cr);

  cairo_surface_destroy (surface);
}

static cairo_surface_t *
create_standard_background (float r, float g, float b) {
  cairo_surface_t *bg;
  cairo_surface_t *background;
  cairo_t *cr;
  int width, height;
  cairo_pattern_t *linpat = NULL;

  width = STANDARD_MENU_ITEM_BOX_WIDTH;
  height = STANDARD_MENU_ITEM_BOX_HEIGHT;
  background = cairo_image_surface_create  (CAIRO_FORMAT_ARGB32, width, height);

  bg = cairo_menu_create_default_background (STANDARD_MENU_ITEM_WIDTH,
      STANDARD_MENU_ITEM_HEIGHT, r, g, b);

  cr = cairo_create (background);
  cairo_utils_clip_round_edge (cr, width, height,
      STANDARD_MENU_BOX_CORNER_RADIUS, STANDARD_MENU_BOX_CORNER_RADIUS,
      STANDARD_MENU_BOX_CORNER_RADIUS);

  linpat = cairo_pattern_create_linear (width, 0, width, height);

  cairo_pattern_add_color_stop_rgb (linpat, 0.0, 0.3, 0.3, 0.3);
  cairo_pattern_add_color_stop_rgb (linpat, 1.0, 0.7, 0.7, 0.7);

  cairo_set_source (cr, linpat);
  cairo_paint_with_alpha (cr, 0.5);

  cairo_utils_clip_round_edge (cr, width, height,
      STANDARD_MENU_BOX_CORNER_RADIUS + STANDARD_MENU_BOX_BORDER_WIDTH,
      STANDARD_MENU_BOX_CORNER_RADIUS + STANDARD_MENU_BOX_BORDER_WIDTH,
      STANDARD_MENU_BOX_CORNER_RADIUS);

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint_with_alpha (cr, 0.4);

  cairo_set_source_surface (cr, bg, STANDARD_MENU_BOX_X, STANDARD_MENU_BOX_Y);
  cairo_paint (cr);
  cairo_destroy (cr);
  cairo_pattern_destroy (linpat);
  cairo_surface_destroy (bg);

  return background;
}

Menu *
standard_menu_create (const char *title, int width, int height, int rows, int columns)
{
  cairo_surface_t *surface;
  cairo_surface_t *background, *selected_background, *disabled;
  cairo_t *cr;
  Menu * menu = malloc (sizeof(Menu));

  memset (menu, 0, sizeof(Menu));
  menu->draw = draw_standard_menu;
  menu->title = title;
  menu->width = width;
  menu->height = height;

  background = create_standard_background (0, 0, 0);
  selected_background = create_standard_background (0.05, 0.30, 0.60);
  disabled = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
      STANDARD_MENU_ITEM_BOX_WIDTH, STANDARD_MENU_ITEM_BOX_HEIGHT);

  cr = cairo_create (disabled);

  cairo_set_source_rgba (cr, 0.7, 0.7, 0.7, 0.7);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_utils_clip_round_edge (cr,
      STANDARD_MENU_ITEM_BOX_WIDTH, STANDARD_MENU_ITEM_BOX_HEIGHT,
      STANDARD_MENU_BOX_X + 7, STANDARD_MENU_BOX_Y + 7, 7);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_flush (disabled);

  surface = cairo_image_surface_create  (CAIRO_FORMAT_ARGB32,
      STANDARD_MENU_WIDTH, STANDARD_MENU_HEIGHT);
  /* Infinite vertical scrollable menu */
  menu->menu = cairo_menu_new_full (surface, rows, columns,
      STANDARD_MENU_ITEM_BOX_WIDTH, STANDARD_MENU_ITEM_BOX_HEIGHT,
      STANDARD_MENU_PAD_X, STANDARD_MENU_PAD_Y, 0,
      background, selected_background, disabled);
  cairo_surface_destroy (surface);
  cairo_surface_destroy (background);
  cairo_surface_destroy (selected_background);
  cairo_surface_destroy (disabled);

  return menu;
}

int
standard_menu_add_tag (Menu *menu, const char *title, int fontsize)
{
  return cairo_menu_add_item_full (menu->menu, NULL, CAIRO_MENU_IMAGE_POSITION_LEFT, title,
      fontsize, CAIRO_MENU_DEFAULT_TEXT_COLOR, CAIRO_MENU_ALIGN_MIDDLE_CENTER,
      menu->menu->default_item_width / 2, menu->menu->default_item_height,
      STANDARD_MENU_ITEM_IPAD_X, STANDARD_MENU_ITEM_IPAD_Y, FALSE,
      NULL, NULL, NULL, NULL);
}

int
standard_menu_add_item (Menu *menu, const char *title, int fontsize)
{
  int idx;

  idx = cairo_menu_add_item (menu->menu, title, fontsize);
  menu->menu->items[idx].ipad_x = STANDARD_MENU_ITEM_IPAD_X;
  menu->menu->items[idx].ipad_y = STANDARD_MENU_ITEM_IPAD_Y;

  return idx;
}
