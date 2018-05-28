/*
 * cairo_menu.c : Cairo Menu drawing API
 *
 * Copyright (C) Youness Alaoui (KaKaRoTo)
 * Written for PS3 in 2011. Refactored in 2018.
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

#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "cairo_menu.h"
#include "cairo_utils.h"

static void
_draw_text (CairoMenu *menu, CairoMenuItem *item, cairo_t *cr,
    int x, int y, int width, int height)
{
  cairo_font_extents_t fex;
  cairo_text_extents_t tex;
  cairo_font_options_t *opt;

  cairo_save (cr);

  /* Set antialiasing */
  opt = cairo_font_options_create ();
  cairo_get_font_options (cr, opt);
  cairo_font_options_set_antialias (opt, CAIRO_ANTIALIAS_SUBPIXEL);
  cairo_set_font_options (cr, opt);
  cairo_font_options_destroy (opt);

  cairo_select_font_face(cr,
      "sans-serif",
      CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_BOLD);

  cairo_set_font_size(cr, item->text_size);

  cairo_font_extents (cr, &fex);
  cairo_text_extents (cr, item->text, &tex);

  if (item->alignment & CAIRO_MENU_ALIGN_TOP)
    y += fex.ascent;
  else if (item->alignment & CAIRO_MENU_ALIGN_MIDDLE)
    y +=  (height + fex.ascent) / 2;
  else if (item->alignment & CAIRO_MENU_ALIGN_BOTTOM)
    y += height - fex.descent;

  if (item->alignment & CAIRO_MENU_ALIGN_CENTER)
    x += (width - tex.width) / 2;
  else if (item->alignment & CAIRO_MENU_ALIGN_RIGHT)
    x += width - tex.width;

  x -= tex.x_bearing;

  cairo_set_source_rgba (cr, item->text_color.red, item->text_color.green,
      item->text_color.blue, item->text_color.alpha);
  cairo_move_to (cr, x, y);
  cairo_show_text (cr, item->text);
  cairo_restore (cr);
}

static int
_draw_item (CairoMenu *menu, CairoMenuItem *item,
    int selected, cairo_t *cr, int x, int y, void *user_data)
{
  cairo_surface_t *bg;
  int width = item->width - (2 * item->ipad_x);
  int height = item->height - (2 * item->ipad_y);

  cairo_save (cr);

  cairo_rectangle (cr, x, y, item->width, item->height);
  cairo_clip (cr);

  if (selected)
    bg = item->bg_sel_image;
  else
    bg = item->bg_image;

  cairo_save (cr);
  cairo_scale (cr, (float) item->width / cairo_utils_get_surface_width (bg),
      (float) item->height / cairo_utils_get_surface_height (bg));
  cairo_set_source_surface (cr, bg, x, y);

  /* Avoid getting the edge blended with 0 alpha */
  cairo_pattern_set_extend (cairo_get_source(cr), CAIRO_EXTEND_PAD);

  /* Replace the destination with the source instead of overlaying */
  //cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  cairo_restore (cr);

  /* Reset the operator to what it should be */
  //cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  cairo_rectangle (cr, x + item->ipad_x, y + item->ipad_y, width, height);
  cairo_clip (cr);
  if (item->image) {
    if (item->image_position == CAIRO_MENU_IMAGE_POSITION_BOTTOM)
      cairo_set_source_surface (cr, item->image,
          x + item->ipad_x, y + item->height - item->ipad_y -
          cairo_utils_get_surface_height (item->image));
    else if (item->image_position == CAIRO_MENU_IMAGE_POSITION_TOP)
      cairo_set_source_surface (cr, item->image,
          x + item->ipad_x, y + item->ipad_y);
    else if (item->image_position == CAIRO_MENU_IMAGE_POSITION_RIGHT)
      cairo_set_source_surface (cr, item->image,
          x + item->width - item->ipad_x -
          cairo_utils_get_surface_width (item->image), y + item->ipad_y);
    else
      cairo_set_source_surface (cr, item->image,
          x + item->ipad_x, y + item->ipad_y);
    cairo_paint (cr);
  }
  _draw_text (menu, item, cr, x + item->ipad_x, y + item->ipad_y,
      item->width - (2 * item->ipad_x), item->height - (2 * item->ipad_y));

  cairo_restore (cr);

  if (!item->enabled) {
    cairo_set_source_surface (cr, menu->disabled_image, x, y);
    cairo_paint (cr);
  }


  return 0;
}

cairo_surface_t *
_load_image (cairo_surface_t *image, int size)
{
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
      size, size);
  cairo_t *cr = cairo_create (surface);
  int width, height;

  cairo_utils_get_surface_size (image, &width, &height);

  cairo_scale (cr, (float) size / width, (float) size / width);
  cairo_set_source_surface (cr, image, 0, 0);

  /* Avoid getting the edge blended with 0 alpha */
  cairo_pattern_set_extend (cairo_get_source(cr), CAIRO_EXTEND_PAD);

  /* Replace the destination with the source instead of overlaying */
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  cairo_destroy (cr);

  return surface;
}

#define BUTTON_ARC_PAD_X 7
#define BUTTON_ARC_PAD_Y 7
#define BUTTON_ARC_RADIUS 7

static void
RGBToHSV(float r, float g, float b, float *h, float *s, float *v)
{
  float max, min, delta;

  max = fmaxf(r,(fmaxf(g,b)));
  min = fminf(r,(fminf(g,b)));

  *v = max;

  /* Calculate saturation */

  if (max != 0.0)
    *s = (max-min)/max;
  else
    *s = 0.0;

  *h = 0.0;

  if (*s != 0.0) {
    /* chromatic case: Saturation is not 0, so determine hue */
    delta = max-min;

    if (r == max)
      *h = (g - b) / delta;
    else if (g == max)
      *h = 2.0 + (b - r) / delta;
    else if (b == max)
      *h = 4.0 + (r - g) / delta;

    *h *= 60.0;
    if (*h < 0.0)
      *h += 360.0;
  }
}

static void
HSVToRGB(float h, float s, float v, float *r, float *g, float *b)
{
  int i;
  float f, p, q, t;

  if(s == 0 ) {
    *r = *g = *b = v;
  } else {
    h /= 60;                        // sector 0 to 5
    i = (int) floor (h);
    f = h - i;                      // factorial part of h
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));

    switch (i) {
      case 0:
        *r = v;
        *g = t;
        *b = p;
        break;
      case 1:
        *r = q;
        *g = v;
        *b = p;
        break;
      case 2:
        *r = p;
        *g = v;
        *b = t;
        break;
      case 3:
        *r = p;
        *g = q;
        *b = v;
        break;
      case 4:
        *r = t;
        *g = p;
        *b = v;
        break;
      default:                // case 5:
        *r = v;
        *g = p;
        *b = q;
        break;
    }
  }
}

cairo_surface_t *
cairo_menu_create_default_background (int width, int height,
    float r, float g, float b)
{
  cairo_surface_t *surface;
  cairo_pattern_t *linpat = NULL;
  cairo_t *cr = NULL;
  float h, s, v;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
      width, height);

  linpat = cairo_pattern_create_linear (width, 0, width, height);

  /* Transform the RGB into HSV so we can make it lighter */
  RGBToHSV (r, g, b, &h, &s, &v);
  cairo_pattern_add_color_stop_rgb (linpat, 0.0, r, g, b);
  v += 0.1;
  HSVToRGB (h, s, v, &r, &g, &b);
  cairo_pattern_add_color_stop_rgb (linpat, 0.3, r, g, b);
  v += 0.15;
  HSVToRGB (h, s, v, &r, &g, &b);
  cairo_pattern_add_color_stop_rgb (linpat, 0.7, r, g, b);
  v += 0.25;
  HSVToRGB (h, s, v, &r, &g, &b);
  cairo_pattern_add_color_stop_rgb (linpat, 1.0, r, g, b);

  cr = cairo_create (surface);

  cairo_utils_clip_round_edge (cr, width, height,
      BUTTON_ARC_PAD_X, BUTTON_ARC_PAD_Y,
      BUTTON_ARC_RADIUS);

  cairo_set_source (cr, linpat);
  cairo_paint (cr);
  /*
  cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.2);
  cairo_new_path (cr);
  cairo_arc (cr, width / 2, - (width * 4) + (height / 2),
      width * 4, 0, M_PI * 2);
  cairo_close_path (cr);
  cairo_fill (cr);*/

  cairo_destroy (cr);
  cairo_pattern_destroy (linpat);
  cairo_surface_flush (surface);

  return surface;
}


static cairo_surface_t *
_create_disabled_overlay (CairoMenu *menu, int width, int height)
{
  cairo_surface_t *surface;
  cairo_t *cr = NULL;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
      width, height);

  cr = cairo_create (surface);

  cairo_set_source_rgba (cr, 0.7, 0.7, 0.7, 0.7);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_mask_surface (cr, menu->bg_image, 0, 0);

  cairo_destroy (cr);
  cairo_surface_flush (surface);

  return surface;
}

CairoMenu *
cairo_menu_new_full (cairo_surface_t *surface, int rows, int columns,
    int default_item_width, int default_item_height, int pad_x, int pad_y,
    int dropshadow_radius, cairo_surface_t *bg_image,
    cairo_surface_t *bg_sel_image, cairo_surface_t *disabled_overlay)
{
  CairoMenu *menu = NULL;

  /* Infinite scrolling horizontally and vertically is not practical,
   * return error  */
  if (rows == -1 && columns == -1)
    return NULL;

  if (surface == NULL)
    return NULL;

  menu = malloc (sizeof(CairoMenu));

  memset (menu, 0, sizeof(CairoMenu));
  menu->surface = cairo_surface_reference (surface);
  menu->rows = rows;
  menu->columns = columns;
  menu->default_item_width = default_item_width;
  menu->default_item_height = default_item_height;
  menu->pad_x = pad_x;
  menu->pad_y = pad_y;
  menu->dropshadow_radius = dropshadow_radius;
  menu->nitems = 0;
  menu->items = NULL;
  menu->selection = 0; /* Select the first item by default */
  menu->start_item = 0;

  if (bg_image)
    menu->bg_image = cairo_surface_reference (bg_image);
  else
    menu->bg_image = cairo_menu_create_default_background (
        menu->default_item_width, menu->default_item_height, 0.0, 0.0, 0.0);

  if (bg_image)
    menu->bg_sel_image = cairo_surface_reference (bg_sel_image);
  else
    menu->bg_sel_image = cairo_menu_create_default_background (
        menu->default_item_width, menu->default_item_height, 0.05, 0.30, 0.60);

  if (disabled_overlay)
    menu->disabled_image = cairo_surface_reference (disabled_overlay);
  else
    menu->disabled_image = _create_disabled_overlay (menu,
        menu->default_item_width, menu->default_item_height);

  if (dropshadow_radius) {
    cairo_t *cr;

    menu->dropshadow = cairo_image_surface_create  (CAIRO_FORMAT_ARGB32,
        menu->default_item_width + (6 * dropshadow_radius),
        menu->default_item_height + (6 * dropshadow_radius));
    cr = cairo_create (menu->dropshadow);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_mask_surface (cr, menu->bg_image, dropshadow_radius * 3,
        dropshadow_radius * 3);
    cairo_destroy (cr);

    cairo_utils_image_surface_blur (menu->dropshadow, dropshadow_radius);
  }

  return menu;
}

CairoMenu *
cairo_menu_new (cairo_surface_t *surface, int rows, int columns,
    int default_item_width, int default_item_height)
{
  return cairo_menu_new_full (surface, rows, columns,
      default_item_width, default_item_height,
      CAIRO_MENU_DEFAULT_PAD_X, CAIRO_MENU_DEFAULT_PAD_Y, 0,
      NULL, NULL, NULL);
}

cairo_surface_t *
cairo_menu_scale_or_reference_surface (cairo_surface_t *surface, int width, int height)
{
  /* int ref_width, ref_height;

  cairo_utils_get_surface_size (surface, &ref_width, &ref_height);
  if (ref_width == width && ref_height == height)*/
  return cairo_surface_reference (surface);
}
int
cairo_menu_add_item_full (CairoMenu *menu, cairo_surface_t *image,
    CairoMenuImagePosition image_position, const char *text, int text_size,
    CairoMenuColor text_color, CairoMenuTextAlignment alignment,
    int width, int height, int ipad_x, int ipad_y, int enabled,
    cairo_surface_t *bg_image, cairo_surface_t *bg_sel_image,
    CairoMenuDrawItemCb draw_cb, void *draw_data)
{
  CairoMenuItem *item = NULL;

  if (menu == NULL)
    return -1;

  /* If the menu has a fixed and has no room left, return an error */
  if (menu->rows != -1 && menu->columns != -1 &&
      menu->nitems >= (menu->rows * menu->columns))
    return -1;

  menu->nitems++;
  menu->items = realloc (menu->items, menu->nitems * sizeof(CairoMenuItem));

  item = &menu->items[menu->nitems - 1];
  memset (item, 0, sizeof(CairoMenuItem));

  item->index = menu->nitems - 1;
  item->image = NULL;
  item->image_position = image_position;
  item->text = text ? strdup (text) : NULL;
  item->text_size = text_size;
  item->text_color = text_color;
  item->alignment = alignment;
  if (draw_cb != NULL)
    item->draw_cb = draw_cb;
  else
    item->draw_cb = _draw_item;

  item->draw_data = draw_data;
  item->width = width;
  item->height = height;
  item->ipad_x = ipad_x;
  item->ipad_y = ipad_y;
  item->enabled = enabled;

  if (bg_image == NULL)
    bg_image = menu->bg_image;
  if (bg_sel_image == NULL)
    bg_sel_image = menu->bg_sel_image;

  item->bg_image = cairo_menu_scale_or_reference_surface (bg_image, width, height);
  item->bg_sel_image = cairo_menu_scale_or_reference_surface (bg_sel_image, width, height);

  if (image != NULL)
    cairo_menu_set_item_image (menu, item->index, image, image_position);

  return item->index;
}

int
cairo_menu_add_item (CairoMenu *menu, const char *text, int text_size)
{
  return cairo_menu_add_item_full (menu, NULL, CAIRO_MENU_IMAGE_POSITION_LEFT, text,
      text_size, CAIRO_MENU_DEFAULT_TEXT_COLOR, CAIRO_MENU_ALIGN_MIDDLE_CENTER,
      menu->default_item_width, menu->default_item_height,
      CAIRO_MENU_DEFAULT_IPAD_X, CAIRO_MENU_DEFAULT_IPAD_Y, TRUE,
      NULL, NULL, NULL, NULL);
}

void
cairo_menu_set_item_image (CairoMenu *menu, int item_index, cairo_surface_t *image,
    CairoMenuImagePosition image_position)
{
  CairoMenuItem *item = &menu->items[item_index];

  if (item->image)
    cairo_surface_destroy (item->image);

  item->image = NULL;
  item->image_position = image_position;

  if (image) {
    if (image_position == CAIRO_MENU_IMAGE_POSITION_BOTTOM ||
        image_position == CAIRO_MENU_IMAGE_POSITION_TOP)
      item->image = _load_image (image, item->width - (2 * item->ipad_x));
    else
      item->image = _load_image (image, item->height - (2 * item->ipad_y));
  }
}


static int
_handle_input_internal (CairoMenu *menu, CairoMenuInput input,
    CairoMenuRectangle *bbox)
{
  int row, new_row, start_row, max_rows, max_visible_rows;
  int column, new_column, start_column, max_columns, max_visible_columns;
  int width, height;

  cairo_utils_get_surface_size (menu->surface, &width, &height);

  /* TODO: Actually walk the items and calculate the real value depending on
     individual item's width/height */
  max_visible_rows = height / (menu->default_item_height + (2 * menu->pad_y));
  max_visible_columns = width / (menu->default_item_width + (2 * menu->pad_x));

  if (max_visible_rows == 0)
    max_visible_rows = 1;
  if (max_visible_columns == 0)
    max_visible_columns = 1;

  /* Define which row/column the selection is in */
  if (menu->columns != -1) {
    row = menu->selection / menu->columns;
    column = menu->selection % menu->columns;
    start_row = menu->start_item / menu->columns;
    start_column = menu->start_item % menu->columns;

    if (menu->nitems < menu->columns)
      max_columns = menu->nitems;
    else
      max_columns = menu->columns;

    if (menu->nitems == 0)
      max_rows = 0;
    else
      max_rows = ((menu->nitems - 1) / menu->columns) + 1;
  } else {
    column = menu->selection / menu->rows;
    row = menu->selection % menu->rows;
    start_row = menu->start_item % menu->rows;
    start_column = menu->start_item / menu->rows;

    if (menu->nitems < menu->rows)
      max_rows = menu->nitems;
    else
      max_rows = menu->rows;

    if (menu->nitems == 0)
      max_columns = 0;
    else
      max_columns = ((menu->nitems - 1) / menu->rows) + 1;
  }

  switch(input) {
    case CAIRO_MENU_INPUT_UP:
      if (row > 0) {
        if (menu->columns != -1) {
          menu->selection -= menu->columns;
        } else {
          menu->selection--;
        }
      }
      break;
    case CAIRO_MENU_INPUT_DOWN:
      if (row < max_rows - 1) {
        if (menu->columns != -1) {
          if (menu->selection + menu->columns < menu->nitems)
          menu->selection += menu->columns;
        } else {
          if (menu->selection + 1 < menu->nitems)
            menu->selection++;
        }
      }
      break;
    case CAIRO_MENU_INPUT_LEFT:
      if (column > 0) {
        if (menu->rows != -1) {
          menu->selection -= menu->rows;
        } else {
          menu->selection--;
        }
      }
      break;
    case CAIRO_MENU_INPUT_RIGHT:
      if (column < max_columns - 1) {
        if (menu->rows != -1) {
          if (menu->selection + menu->rows < menu->nitems)
            menu->selection += menu->rows;
        } else {
          if (menu->selection + 1 < menu->nitems)
            menu->selection++;
        }
      }
      break;
  }

  if (menu->columns != -1) {
    new_row = menu->selection / menu->columns;
    new_column = menu->selection % menu->columns;
  } else {
    new_column = menu->selection / menu->rows;
    new_row = menu->selection % menu->rows;
  }

  /*
  printf ("row - %d - new %d - start %d - max %d - visible %d\n",
      row, new_row, start_row, max_rows, max_visible_rows);
  printf ("column - %d - new %d - start %d - max %d - visible %d\n",
      column, new_column, start_column, max_columns, max_visible_columns);
  */

  if (new_row < start_row || new_column < start_column) {
    /* We go left/up back to a non-displayed item */
    if (menu->columns != -1) {
      if (input == CAIRO_MENU_INPUT_LEFT)
        menu->start_item -= 1;
      else
        menu->start_item -= menu->columns;
    } else {
      if (input == CAIRO_MENU_INPUT_LEFT)
        menu->start_item -= menu->rows;
      else
        menu->start_item -= 1;
    }
    cairo_menu_redraw (menu);

    bbox->x = 0;
    bbox->y = 0;
    cairo_utils_get_surface_size (menu->surface, &bbox->width, &bbox->height);
  } else if (((new_row - start_row) >= max_visible_rows) ||
      ((new_column - start_column) >= max_visible_columns)) {
    /* We go right/down to a hidden item */
    if (menu->columns != -1) {
      /* If we go on the right, we only need to shift by one column.
         If we go down, we need to shift by own row */
      if (input == CAIRO_MENU_INPUT_RIGHT)
        menu->start_item += 1;
      else
        menu->start_item += menu->columns;
    } else {
      if (input == CAIRO_MENU_INPUT_RIGHT)
        menu->start_item += menu->rows;
      else
        menu->start_item += 1;
    }
    cairo_menu_redraw (menu);

    bbox->x = 0;
    bbox->y = 0;
    cairo_utils_get_surface_size (menu->surface, &bbox->width, &bbox->height);
  } else {
    /* TODO: Only redraw the part that we need, and set the right bbox */
    cairo_menu_redraw (menu);

    bbox->x = 0;
    bbox->y = 0;
    cairo_utils_get_surface_size (menu->surface, &bbox->width, &bbox->height);
  }

  return menu->selection;
}

int
cairo_menu_handle_input (CairoMenu *menu, CairoMenuInput input,
    CairoMenuRectangle *bbox)
{
  int old_start_item;
  int old_selection;
  int new_selection;
  int previous_selection;

  if (menu->items == NULL)
    return -1;

  old_start_item = menu->start_item;
  old_selection = new_selection = previous_selection = menu->selection;

  do {
    new_selection = _handle_input_internal (menu, input, bbox);

    /* Make sure this isn't the last possible item we can go to */
    if (new_selection == previous_selection)
      break;
    previous_selection = new_selection;
  } while (menu->items[new_selection].enabled == FALSE);

  /* We were already on the last selectable item, then revert */
  if (menu->items[new_selection].enabled == FALSE) {
    menu->selection = new_selection = old_selection;
    menu->start_item = old_start_item;

    cairo_menu_redraw (menu);

    bbox->x = 0;
    bbox->y = 0;
    cairo_utils_get_surface_size (menu->surface, &bbox->width, &bbox->height);
  }

  return new_selection;
}

void
cairo_menu_set_selection (CairoMenu *menu, int id, CairoMenuRectangle *bbox)
{

  if (menu->items[id].enabled == FALSE)
    return;

  menu->selection = id;
  //TODO : menu->start_item = old_start_item;

  cairo_menu_redraw (menu);
  bbox->x = 0;
  bbox->y = 0;
  cairo_utils_get_surface_size (menu->surface, &bbox->width, &bbox->height);
}

void
cairo_menu_redraw (CairoMenu *menu)
{
  int i;
  cairo_t *cr;
  int width, height;
  int x = 0;
  int y = 0;
  int row, start_row;
  int column, start_column;

  cr = cairo_create (menu->surface);

  /* Clear the whole surface before redrawing */
  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_restore (cr);

  cairo_utils_get_surface_size (menu->surface, &width, &height);

  /* Define which row/column we're drawing if we're scrolled */
  if (menu->columns != -1) {
    row = menu->start_item / menu->columns;
    column = menu->start_item % menu->columns;
  } else {
    row = menu->start_item % menu->rows;
    column = menu->start_item / menu->rows;
  }
  start_row = row;
  start_column = column;

  for (i = menu->start_item; i < menu->nitems;) {
    CairoMenuItem *item = &menu->items[i];

    x += menu->pad_x;
    y += menu->pad_y;

    /* No need to draw the items that are outside the visible area */
    if (x < width && y < height) {
      if (menu->dropshadow) {
        cairo_set_source_surface (cr, menu->dropshadow,
            x - menu->dropshadow_radius, y - menu->dropshadow_radius);
        cairo_paint (cr);
      }
      cairo_rectangle (cr, x, y, item->width, item->height);
      cairo_clip (cr);
      item->draw_cb (menu, item, (menu->selection == item->index), cr,
          x, y, item->draw_data);
    }

    /* Move to the next item position */
    if (menu->columns != -1) {
      /* Filling the rows from left to right */
      column++;
      if (column < menu->columns) {
        x += item->width + menu->pad_x;
        y -= menu->pad_y;
      } else {
        row++;
        column = start_column;
        x = 0;
        y += item->height + menu->pad_y;
      }
      i = (menu->columns * row) + column;
    } else {
      /* Filling the columns from top to bottom */
      row++;
      if (row < menu->rows) {
        x -= menu->pad_x;
        y += item->height + menu->pad_y;
      } else {
        column++;
        row = start_row;
        x += item->width + menu->pad_x;
        y = 0;
      }
      i = (menu->rows * column) + row;
    }

    cairo_reset_clip (cr);
  }
  cairo_destroy (cr);
  cairo_surface_flush (menu->surface);
}

cairo_surface_t *
cairo_menu_get_surface (CairoMenu *menu)
{
  return cairo_surface_reference (menu->surface);
}

void
cairo_menu_free (CairoMenu *menu)
{
  int i;

  if (menu->items) {
    for (i = 0; i < menu->nitems; i++) {
      CairoMenuItem *item = &menu->items[i];
      if (item->image)
        cairo_surface_destroy (item->image);
      free (item->text);
      if (item->bg_image)
        cairo_surface_destroy (item->bg_image);
      if (item->bg_sel_image)
        cairo_surface_destroy (item->bg_sel_image);
    }
    free (menu->items);
  }

  cairo_surface_destroy (menu->surface);
  if (menu->bg_image)
    cairo_surface_destroy (menu->bg_image);
  if (menu->bg_sel_image)
    cairo_surface_destroy (menu->bg_sel_image);
  if (menu->disabled_image)
    cairo_surface_destroy (menu->disabled_image);
  if (menu->dropshadow)
    cairo_surface_destroy (menu->dropshadow);
  free (menu);
}
