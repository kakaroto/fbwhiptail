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

cairo_surface_t *
create_gradient_background (int width, int height,
    float start_r, float start_g, float start_b,
    float end_r, float end_g, float end_b)
{
  cairo_surface_t * background = NULL;
  cairo_pattern_t *linpat = NULL;
  cairo_t *grad_cr = NULL;

  background = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  linpat = cairo_pattern_create_linear (0, 0, width, height);
  cairo_pattern_add_color_stop_rgb (linpat, 0, start_r, start_g, start_b);
  cairo_pattern_add_color_stop_rgb (linpat, 1, end_r, end_g, end_b);

  grad_cr = cairo_create (background);
  cairo_set_source (grad_cr, linpat);
  cairo_paint (grad_cr);
  cairo_destroy (grad_cr);
  cairo_pattern_destroy (linpat);
  cairo_surface_flush (background);

  return background;
}

cairo_surface_t *
load_image_and_scale (char *path, int width, int height)
{
  cairo_surface_t *image = NULL;
  cairo_surface_t *surface = NULL;

  image = cairo_image_surface_create_from_png (path);
  if (image) {
    cairo_t *cr;
    int img_width, img_height;

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
        width, height);
    cr = cairo_create (surface);

    cairo_utils_get_surface_size (image, &img_width, &img_height);

    cairo_scale (cr, (float) width / img_width, (float) height / img_height);
    cairo_set_source_surface (cr, image, 0, 0);

    /* Avoid getting the edge blended with 0 alpha */
    cairo_pattern_set_extend (cairo_get_source(cr), CAIRO_EXTEND_PAD);

    /* Replace the destination with the source instead of overlaying */
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_destroy (cr);
  }

  return surface;
}


void
draw_background (Menu *menu, cairo_t *cr)
{
  if (menu->background) {
    cairo_set_source_surface (cr, menu->background, 0, 0);
    cairo_paint (cr);
  }
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
  // If horizontal, don't extend frame downward
  if (menu->menu->rows == 1)
    height = STANDARD_MENU_ITEM_TOTAL_HEIGHT;
  else
    height = menu->menu->nitems * STANDARD_MENU_ITEM_TOTAL_HEIGHT;
  height += cairo_utils_get_surface_height (menu->text.surface);
  if (height > STANDARD_MENU_HEIGHT)
    height = STANDARD_MENU_HEIGHT;
  height += STANDARD_MENU_FRAME_HEIGHT;

  frame = cairo_image_surface_create  (CAIRO_FORMAT_ARGB32,
      width, height);

  // Draw silver border around the frame
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

  // Create frame gradient
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
  menu->frame = cairo_utils_surface_add_dropshadow (frame,
      FRAME_DROPSHADOW_DISTANCE);
  cairo_surface_destroy (frame);
}

static void
refresh_text_surface (Menu *menu)
{
  double x;
  double y;
  double width = menu->width * 0.8;
  double height = menu->height * 0.8;
  char **line;
  int cnt = menu->text.start_line;
  cairo_t *cr;

  cr = cairo_create (menu->text.surface);
  x = 0;
  y = 0;

  line = menu->text.lines;
  while (*line != NULL && y + 20 < height) {
    if (cnt > 0) {
      cnt--;
      line++;
      continue;
    }

    /* Drawing text line */
    cairo_save (cr);

    cairo_select_font_face (cr,
        "monospace",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_BOLD);

    cairo_set_font_size (cr, 15);

    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_move_to (cr, x, y + 20);
    cairo_show_text (cr, *line);
    cairo_restore (cr);
    y += 20;
    line++;
  }

}

static void
draw_standard_menu (Menu *menu, cairo_t *cr)
{
  cairo_surface_t *surface;
  int w, h;
  int menu_width;
  int menu_height;
  int text_height;

  if (menu->frame == NULL) {
    create_standard_menu_frame (menu);
  }
  refresh_text_surface (menu);

  cairo_utils_get_surface_size (menu->frame, &w, &h);
  text_height = cairo_utils_get_surface_height (menu->text.surface);
  surface = cairo_menu_get_surface (menu->menu);

  cairo_menu_redraw (menu->menu);

  cairo_set_source_surface (cr, menu->frame, (menu->width - w) / 2,
      (menu->height - h) / 2);
  cairo_paint (cr);

  cairo_set_source_surface (cr, menu->text.surface,
      (menu->width - w) / 2 + STANDARD_MENU_FRAME_SIDE,
      (menu->height - h) / 2 + STANDARD_MENU_FRAME_TOP);
  cairo_paint (cr);

  /* Draw a frame around the menu so cut off buttons don't appear clippped */

  menu_width = STANDARD_MENU_WIDTH;
  if (menu->menu->rows == 1)
    menu_height = STANDARD_MENU_ITEM_TOTAL_HEIGHT;
  else
    menu_height = menu->menu->nitems * STANDARD_MENU_ITEM_TOTAL_HEIGHT;

  if (menu_height > STANDARD_MENU_HEIGHT - text_height)
    menu_height = STANDARD_MENU_HEIGHT - text_height;

  cairo_save (cr);
  cairo_translate (cr, ((menu->width - w) / 2) + STANDARD_MENU_FRAME_SIDE,
      ((menu->height - h) / 2) + STANDARD_MENU_FRAME_TOP + text_height);
  cairo_utils_clip_round_edge (cr, menu_width, menu_height, 20, 20, 20);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint_with_alpha (cr, 0.5);

  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);
  cairo_restore (cr);

  cairo_surface_destroy (surface);
}

static cairo_surface_t *
create_standard_background (int width, int height, float r, float g, float b) {
  cairo_surface_t *bg;
  cairo_surface_t *background;
  cairo_t *cr;
  cairo_pattern_t *linpat = NULL;

  background = cairo_image_surface_create  (CAIRO_FORMAT_ARGB32, width, height);

  bg = cairo_menu_create_default_background (width - (2 * STANDARD_MENU_BOX_X),
      height - (2 * STANDARD_MENU_BOX_Y), r, g, b);

  // Draw the grey/silver gradient (for the border)
  cr = cairo_create (background);
  cairo_utils_clip_round_edge (cr, width, height,
      STANDARD_MENU_BOX_CORNER_RADIUS, STANDARD_MENU_BOX_CORNER_RADIUS,
      STANDARD_MENU_BOX_CORNER_RADIUS);

  linpat = cairo_pattern_create_linear (width, 0, width, height);

  cairo_pattern_add_color_stop_rgb (linpat, 0.0, 0.3, 0.3, 0.3);
  cairo_pattern_add_color_stop_rgb (linpat, 1.0, 0.7, 0.7, 0.7);

  cairo_set_source (cr, linpat);
  cairo_paint_with_alpha (cr, 0.5);

  // Clear the inside of the gradient in order to create a border
  cairo_utils_clip_round_edge (cr, width, height,
      STANDARD_MENU_BOX_CORNER_RADIUS + STANDARD_MENU_BOX_BORDER_WIDTH,
      STANDARD_MENU_BOX_CORNER_RADIUS + STANDARD_MENU_BOX_BORDER_WIDTH,
      STANDARD_MENU_BOX_CORNER_RADIUS);

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  // Add a 40% alpha black filter inside, creating a frame around the button
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint_with_alpha (cr, 0.4);

  // Draw the actual button
  cairo_set_source_surface (cr, bg, STANDARD_MENU_BOX_X, STANDARD_MENU_BOX_Y);
  cairo_paint (cr);
  cairo_destroy (cr);
  cairo_pattern_destroy (linpat);
  cairo_surface_destroy (bg);

  return background;
}

char *
load_text_from_file (char *filename)
{
 FILE* fd = NULL;
 char *text = NULL;
 int filesize;

 fd = fopen(filename,"rb");
 if (fd != NULL) {
   fseek(fd, 0, SEEK_END);
   filesize = ftell(fd);
   fseek(fd, 0, SEEK_SET);

   text = (char*) malloc(filesize + 1);
   fread(text, 1, filesize, fd);
   text[filesize] = 0;
   fclose(fd);
 }

 return text;
}

void
create_text_suface (Menu *menu, char *text)
{
  char *ptr, *ptr2;
  int filesize;
  int lines;

 memset (&menu->text, 0, sizeof(MenuText));

 /* Calculate number of lines */
 lines = 0;
 ptr = ptr2 = text;
 while (*ptr != 0) {
   if (*ptr == '\\' && ptr[1] == 'n') {
     *ptr2 = '\n';
     ptr += 2;
   } else {
     *ptr2 = *ptr;
     ptr++;
   }
   if (*ptr2++ == '\n')
     lines++;
 }
 *ptr2 = '\0';
 /* Last line */
 lines++;

 menu->text.lines = malloc ((lines + 1) * sizeof(char *));
 menu->text.nlines = 0;
 ptr = text;
 while (*ptr != 0) {
   if (*ptr != '\r')
     menu->text.lines[menu->text.nlines++] = ptr;

   while (*ptr != 0 && *ptr != '\r' && *ptr != '\n')
     ptr++;
   if (*ptr == '\r')
     *ptr++ = 0;
   if (*ptr == '\n')
     *ptr++ = 0;
 }
 menu->text.lines[menu->text.nlines] = NULL;

 menu->text.surface = cairo_image_surface_create  (CAIRO_FORMAT_ARGB32,
     STANDARD_MENU_WIDTH, 10 + 20 * menu->text.nlines);
}

Menu *
standard_menu_create (const char *title, char * text,
    int width, int height, int rows, int columns)
{
  cairo_surface_t *surface;
  cairo_surface_t *background, *selected_background, *disabled;
  cairo_t *cr;
  Menu * menu = malloc (sizeof(Menu));
  int text_height;
  int horizontal = !!(rows == 1);
  int button_width, button_height;

  button_width = STANDARD_MENU_ITEM_BOX_WIDTH;
  if (rows == 1)
    button_width = (button_width / 2) - STANDARD_MENU_PAD_X;
  button_height = STANDARD_MENU_ITEM_BOX_HEIGHT;

  memset (menu, 0, sizeof(Menu));
  menu->draw = draw_standard_menu;
  menu->title = title;
  menu->width = width;
  menu->height = height;

  create_text_suface (menu, text);
  text_height = cairo_utils_get_surface_height (menu->text.surface);
  background = create_standard_background (button_width, button_height, 0, 0, 0);
  selected_background = create_standard_background (button_width, button_height,
      0.05, 0.30, 0.60);
  disabled = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
      button_width, button_height);

  cr = cairo_create (disabled);

  cairo_set_source_rgba (cr, 0.7, 0.7, 0.7, 0.7);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_utils_clip_round_edge (cr, button_width, button_height,
      STANDARD_MENU_BOX_X + 7, STANDARD_MENU_BOX_Y + 7, 7);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_flush (disabled);

  surface = cairo_image_surface_create  (CAIRO_FORMAT_ARGB32,
      STANDARD_MENU_WIDTH, STANDARD_MENU_HEIGHT - text_height);
  /* Infinite vertical scrollable menu */
  menu->menu = cairo_menu_new_full (surface, rows, columns,
      button_width, button_height,
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

void
free_text (Menu *menu)
{
  if (menu->text.lines != NULL && menu->text.lines[0] != NULL)
    free (menu->text.lines[0]);
  if (menu->text.lines != NULL)
    free (menu->text.lines);
}
