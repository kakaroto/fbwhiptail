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

#include <sys/time.h>

#include <cairo/cairo.h>

#include "fbwhiptail_menu.h"

#ifdef GTKWHIPTAIL
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#else
#include <fcntl.h>
#include <termios.h>

#include <linux/types.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <signal.h>

#include "cairo_dri.h"
#endif

#define VERSION_STRING "0.0.1"

#ifdef GTKWHIPTAIL

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

static int result = 0;

static gint key_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  Menu *menu = data;
  CairoMenuInput input;
  CairoMenuRectangle bbox;

  if (event->keyval == GDK_Up)
    input = CAIRO_MENU_INPUT_UP;
  else if (event->keyval == GDK_Down)
    input = CAIRO_MENU_INPUT_DOWN;
  else if (event->keyval == GDK_Left)
    input = CAIRO_MENU_INPUT_LEFT;
  else if (event->keyval == GDK_Right)
    input = CAIRO_MENU_INPUT_RIGHT;
  else if (event->keyval == GDK_Escape)
    gtk_main_quit ();
  else if (event->keyval == GDK_KP_Enter || event->keyval == GDK_Return) {
    gtk_main_quit ();
    result = 1;
  } else
    return TRUE;

  cairo_menu_handle_input (menu->menu, input, &bbox);
  gtk_widget_queue_draw_area (widget, 0, 0, menu->width, menu->height);
  return TRUE;
}


static void
draw (GtkWidget *widget, GdkEventExpose *eev, gpointer data)
{
  Menu *menu = data;
  cairo_t *cr;

  cr = gdk_cairo_create(widget->window);

  draw_background (menu, cr);
  menu->draw (menu, cr);

  cairo_destroy(cr);
}

#else

static volatile sig_atomic_t cancel = 0;

void signal_handler(int signum)
{
  cancel = 1;
}

static int handle_arrow_input(Menu *menu, short code)
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

static int handle_input(Menu *menu)
{
  static int escape = 0;
  char c;
  int result = 0;

  switch (c = getchar ()) {
    case 0x1B: // Escape character
      if (escape == 0)
        escape = 1;
      else
        cancel = 1; // Double Escape
      break;
    case 0xA: // Enter
      if (escape == 0) {
        cancel = 1;
        result = 2;
      }
      escape = 0;
      break;
    case 0x41: // Arrow keys
    case 0x42:
    case 0x43:
    case 0x44:
      if (escape == 2) {
        handle_arrow_input (menu, c);
        result = 1;
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

  return result;
}

#endif

void print_version (int exit_code)
{
  printf ("Framebuffer Whiptail (fbwhiptail): %s\n", VERSION_STRING);
  exit (exit_code);
}

void print_usage (int exit_code)
{
  printf ("Box options: \n");
  printf ("\t--msgbox <text> <height> <width>\n");
  printf ("\t--yesno  <text> <height> <width>\n");
  printf ("\t--infobox <text> <height> <width>\n");
  printf ("\t\tThis option is not supported\n");
  printf ("\t--inputbox <text> <height> <width> [init] \n");
  printf ("\t\tThis option is not supported\n");
  printf ("\t--passwordbox <text> <height> <width> [init] \n");
  printf ("\t\tThis option is not supported\n");
  printf ("\t--textbox <file> <height> <width>\n");
  printf ("\t\tThis option is not yet supported\n");
  printf ("\t--menu <text> <height> <width> <listheight> [tag item] ...\n");
  printf ("\t--checklist <text> <height> <width> <listheight> [tag item status]...\n");
  printf ("\t\tThis option is not supported\n");
  printf ("\t--radiolist <text> <height> <width> <listheight> [tag item status]...\n");
  printf ("\t\tThis option is not supported\n");
  printf ("\t--gauge <text> <height> <width> <percent>\n");
  printf ("\t\tThis option is not supported\n");
  printf ("Options: (depend on box-option)\n");
  printf ("\t--clear\t\t\t\tclear screen on exit\n");
  printf ("\t--defaultno\t\t\tdefault no button\n");
  printf ("\t--default-item <string>\t\tset default string\n");
  printf ("\t--fb, --fullbuttons\t\tuse full buttons\n");
  printf ("\t\tThis option is not supported\n");
  printf ("\t--nocancel\t\t\tno cancel button\n");
  printf ("\t--yes-button <text>\t\tset text of yes button\n");
  printf ("\t--no-button <text>\t\tset text of no button\n");
  printf ("\t--ok-button <text>\t\tset text of ok button\n");
  printf ("\t--cancel-button <text>\t\tset text of cancel button\n");
  printf ("\t--noitem\t\t\tdon't display items\n");
  printf ("\t--notags\t\t\tdon't display tags\n");
  printf ("\t--separate-output\t\toutput one line at a time\n");
  printf ("\t\tThis option is not supported\n");
  printf ("\t--output-fd <fd>\t\toutput to fd, not stdout\n");
  printf ("\t--title <title>\t\t\tdisplay title\n");
  printf ("\t--backtitle <backtitle>\t\tdisplay backtitle\n");
  printf ("\t--scrolltext\t\t\tforce vertical scrollbars\n");
  printf ("\t\tThis option is not supported\n");
  printf ("\t--topleft\t\t\tput window in top-left corner\n");
  printf ("\t-h, --help\t\t\tprint this message\n");
  printf ("\t-v, --version\t\t\tprint version information\n");
  printf ("Frambuffer options:\n");
  printf ("\t--background-png <file>\t\tDisplay PNG image as background\n");
  printf ("\t--background-gradient <start red> <start green> <start blue> <end red> <end green> <end blue>\n");
  printf ("\t\t\t\t\tGenerate a linear gradient background from left to right\n");

  exit (exit_code);
}

int parse_whiptail_args (int argc, char **argv, whiptail_args *args)
{
  int i;
  int end_of_args = 0;

  memset (args, 0, sizeof(whiptail_args));
  args->output_fd = 2;
  args->yes_button = "Yes";
  args->no_button = "No";
  args->ok_button = "OK";
  args->cancel_button = "Cancel";
  args->background_grad_rgb[0] = 0;
  args->background_grad_rgb[1] = 0;
  args->background_grad_rgb[2] = 0;
  args->background_grad_rgb[3] = 0.6;
  args->background_grad_rgb[4] = 0.6;
  args->background_grad_rgb[5] = 0.6;

  for (i = 1; i < argc; i++) {
    if (end_of_args == 0 && strcmp (argv[i], "-h") == 0) {
        print_usage (0);
    }
    if (end_of_args == 0 && strcmp (argv[i], "-v") == 0) {
        print_version (0);
    }
    if (end_of_args == 0 && strncmp (argv[i], "--", 2) == 0) {
      if (strcmp (argv[i], "--help") == 0) {
        print_usage (0);
      } else if (strcmp (argv[i], "--version") == 0) {
        print_version (0);
      } else if (strcmp (argv[i], "--title") == 0) {
        if (i + 1 >= argc)
          goto missing_value;
        args->title = argv[++i];
      } else if (strcmp (argv[i], "--backtitle") == 0) {
        if (i + 1 >= argc)
          goto missing_value;
        args->backtitle = argv[++i];
      } else if (strcmp (argv[i], "--default-item") == 0) {
        if (i + 1 >= argc)
          goto missing_value;
        args->default_item = argv[++i];
      } else if (strcmp (argv[i], "--output-fd") == 0) {
        if (i + 1 >= argc)
          goto missing_value;
        args->output_fd = atoi (argv[++i]);
      }else if (strcmp (argv[i], "--noitem") == 0) {
        args->noitem = 1;
      } else if (strcmp (argv[i], "--notags") == 0) {
        args->notags = 1;
      } else if (strcmp (argv[i], "--topleft") == 0) {
        args->topleft = 1;
      } else if (strcmp (argv[i], "--clear") == 0) {
        args->clear = 1;
      } else if (strcmp (argv[i], "--menu") == 0) {
        if (args->mode != MODE_NONE)
          goto mode_already_set;
        if (i + 4 >= argc)
          goto missing_value;
        args->text = argv[i+1];
        args->height = atoi (argv[i+2]);
        args->width = atoi (argv[i+3]);
        args->menu_height = atoi (argv[i+4]);
        i += 4;
        args->mode = MODE_MENU;
        args->items = malloc (sizeof(whiptail_menu_item) * (argc - i) / 2);
      } else if (strcmp (argv[i], "--yesno") == 0) {
        if (args->mode != MODE_NONE)
          goto mode_already_set;
        if (i + 3 >= argc)
          goto missing_value;
        args->text = argv[i+1];
        args->height = atoi (argv[i+2]);
        args->width = atoi (argv[i+3]);
        i += 3;
        args->mode = MODE_YESNO;
      } else if (strcmp (argv[i], "--msgbox") == 0) {
        if (args->mode != MODE_NONE)
          goto mode_already_set;
        if (i + 3 >= argc)
          goto missing_value;
        args->text = argv[i+1];
        args->height = atoi (argv[i+2]);
        args->width = atoi (argv[i+3]);
        i += 3;
        args->mode = MODE_MSGBOX;
      }else if (strcmp (argv[i], "--") == 0) {
        end_of_args = 1;
      } else if (strcmp (argv[i], "--yes-button") == 0) {
        if (i + 1 >= argc)
          goto missing_value;
        args->yes_button = argv[++i];
      } else if (strcmp (argv[i], "--no-button") == 0) {
        if (i + 1 >= argc)
          goto missing_value;
        args->no_button = argv[++i];
      } else if (strcmp (argv[i], "--ok-button") == 0) {
        if (i + 1 >= argc)
          goto missing_value;
        args->ok_button = argv[++i];
      } else if (strcmp (argv[i], "--cancel-button") == 0) {
        if (i + 1 >= argc)
          goto missing_value;
        args->cancel_button = argv[++i];
      } else if (strcmp (argv[i], "--fb") == 0 ||
          strcmp (argv[i], "--fullbuttons") == 0 ||
          strcmp (argv[i], "--defaultno") == 0 ||
          strcmp (argv[i], "--nocancel") == 0 ||
          strcmp (argv[i], "--scrolltext") == 0 ||
          strcmp (argv[i], "--separate-output") == 0 ||
          strcmp (argv[i], "--version") == 0) {
        // Ignore unsupported whiptail arguments
      } else if (strcmp (argv[i], "--background-png") == 0) {
        // FBwhiptail specific arguments
        if (i + 1 >= argc)
          goto missing_value;
        args->background_png = argv[++i];
      } else if (strcmp (argv[i], "--background-gradient") == 0) {
        // FBwhiptail specific arguments
        if (i + 6 >= argc)
          goto missing_value;

        args->background_grad_rgb[0] = (float) atoi (argv[i+1]) / 256;
        args->background_grad_rgb[1] = (float) atoi (argv[i+2]) / 256;
        args->background_grad_rgb[2] = (float) atoi (argv[i+3]) / 256;
        args->background_grad_rgb[3] = (float) atoi (argv[i+4]) / 256;
        args->background_grad_rgb[4] = (float) atoi (argv[i+5]) / 256;
        args->background_grad_rgb[5] = (float) atoi (argv[i+6]) / 256;
        i += 6;
      } else {
        printf ("Unknown argument : '%s'\n", argv[i]);
        goto error;
      }
    } else if (args->mode == MODE_MENU) {
      if (i + 1 >= argc)
        goto error;

      args->items[args->num_items].tag = argv[i++];
      args->items[args->num_items].item = argv[i];
      args->num_items++;
    } else {
      goto error;
    }
  }
  if (args->mode == MODE_NONE ||
      (args->mode == MODE_MENU && args->num_items == 0))
    goto error;
  return 0;
 mode_already_set:
  printf ("Only one box option can be set at a time\n");
  goto error;
 missing_value:
  printf ("Not enough values to argument : '%s'\n", argv[i]);
  goto error;
 error:
  if (args->items)
    free (args->items);
  return -1;
}

int main(int argc, char **argv)
{
#ifdef GTKWHIPTAIL
  GtkWidget *window;
  GtkWidget *area;
#else
  struct sigaction action;
  struct termios oldt, newt;
  cairo_t *cr;
  cairo_dri_t *dri = NULL;
  cairo_surface_t **surfaces = NULL;
  cairo_t **crs = NULL;
  int *hdisplay = NULL, *vdisplay = NULL;
  int screens = 0;
  int current_fb = 0;
  int redraw = 1;
  int input_result = 0;
#endif
  int i, idx;
  unsigned int xres, yres;
  Menu *menu = NULL;
  whiptail_args args;
  int return_value = 0;

  if (parse_whiptail_args (argc, argv, &args) != 0) {
    printf ("Invalid arguments received\n");
    print_usage (-1);
    return -1;
  }

  if (args.clear) {
    printf ("\033c");
    fflush (stdout);
  }

#ifdef GTKWHIPTAIL
  gtk_init(&argc, &argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  area = gtk_drawing_area_new();
  gtk_widget_set_size_request (area, WINDOW_WIDTH, WINDOW_HEIGHT);
  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(area));

  gtk_widget_show_all(window);

  xres = WINDOW_WIDTH;
  yres = WINDOW_HEIGHT;
#else
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

  screens = 0;
  dri = cairo_dri_open("/dev/dri/card0");
  if (dri && dri->num_screens > 0) {
    surfaces = malloc (sizeof(cairo_surface_t *) * dri->num_screens * 2);
    crs = malloc (sizeof(cairo_t *) * dri->num_screens * 2);
    hdisplay = malloc (sizeof(int) * dri->num_screens);
    vdisplay = malloc (sizeof(int) * dri->num_screens);
    xres = yres = 0xFFFFFFFF;
    current_fb = 0;
    for (i = 0; i < dri->num_screens; i++) {
      if (dri->screens[i].mode.hdisplay < xres)
        xres = dri->screens[i].mode.hdisplay;
      if (dri->screens[i].mode.vdisplay < yres)
        yres = dri->screens[i].mode.vdisplay;

      hdisplay[screens] = dri->screens[i].mode.hdisplay;
      vdisplay[screens] = dri->screens[i].mode.vdisplay;
      surfaces[screens*2] = cairo_dri_create_surface(dri, &dri->screens[i]);
      surfaces[screens*2+1] = cairo_dri_create_surface(dri, &dri->screens[i]);
      crs[screens*2] = cairo_create(surfaces[screens*2]);
      crs[screens*2+1] = cairo_create(surfaces[screens*2+1]);

      if (cairo_dri_flip_buffer (surfaces[i*2+current_fb], 0) != 0) {
        cairo_destroy(crs[screens*2]);
        cairo_destroy(crs[screens*2+1]);
        cairo_surface_destroy (surfaces[screens*2]);
        cairo_surface_destroy (surfaces[screens*2+1]);
        surfaces[screens*2] = surfaces[screens*2+1] = NULL;
        crs[screens*2] = crs[screens*2+1] = NULL;
      } else {
        screens++;
      }
    }
  }
  if (screens == 0) {
    printf ("Error: Can't find usable screen\n");
    goto error;
  }
  printf ("Found %d screens with smallest one of %dx%d\n", screens, xres, yres);
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

  if (args.mode == MODE_MENU) {
    menu = standard_menu_create (args.title, args.text, xres, yres, -1, 1);

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
  } else if (args.mode == MODE_YESNO) {
    menu = standard_menu_create (args.title, args.text, xres, yres, 1, -1);
    standard_menu_add_item (menu, args.yes_button, 20);
    standard_menu_add_item (menu, args.no_button, 20);
  } else if (args.mode == MODE_MSGBOX) {
    menu = standard_menu_create (args.title, args.text, xres, yres, -1, 1);
    standard_menu_add_item (menu, args.ok_button, 20);
  }
  if (args.background_png)
    menu->background = load_image_and_scale (args.background_png, xres, yres);
  if (menu->background == NULL)
    menu->background = create_gradient_background (xres, yres,
        args.background_grad_rgb[0], args.background_grad_rgb[1],
        args.background_grad_rgb[2], args.background_grad_rgb[3],
        args.background_grad_rgb[4], args.background_grad_rgb[5]);

#ifdef GTKWHIPTAIL
  g_signal_connect (G_OBJECT (window), "delete-event",
      G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (G_OBJECT (area), "expose-event", G_CALLBACK (draw), menu);
    gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
        GTK_SIGNAL_FUNC(key_event), menu);
  gtk_main ();
  if (result) {
    if (args.mode == MODE_MENU)
      fprintf (stderr, "%s", args.items[menu->menu->selection].tag);
    else if (args.mode == MODE_YESNO) {
      if (menu->menu->selection != 0)
        return_value = 1;
    }
  }
#else
  while (!cancel) {

    if (redraw) {
      for (i = 0; i < screens; i++) {
        int off_x = (hdisplay[i] - xres) / 2;
        int off_y = (vdisplay[i] - yres) / 2;

        cr = crs[i*2+current_fb];
        cairo_save (cr);
        cairo_translate (cr, off_x, off_y);
        draw_background (menu, cr);
        menu->draw (menu, cr);
        cairo_restore (cr);
        if (cairo_dri_flip_buffer (surfaces[i*2 + current_fb], 0) != 0) {
          printf ("Flip failed. Cancelling\n");
          break;
        }
      }
      current_fb = (current_fb + 1) % 2;
      redraw = 0;
    }
    input_result = handle_input (menu);
    if (input_result == 1)
      redraw = 1;
    else if (input_result == 2) {
      if (args.mode == MODE_MENU)
        fprintf (stderr, "%s", args.items[menu->menu->selection].tag);
      else if (args.mode == MODE_YESNO) {
        if (menu->menu->selection != 0)
          return_value = 1;
      }
    }
  }

 error:
  /*restore the old settings*/
  tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
  for (i = 0; i < screens * 2; i++) {
    if (crs[i])
      cairo_destroy(crs[i]);
    if (surfaces[i])
      cairo_surface_destroy (surfaces[i]);
  }
  if (crs)
    free (crs);
  if (surfaces)
    free (surfaces);
  if (hdisplay)
    free (hdisplay);
  if (vdisplay)
    free (vdisplay);
  if (dri)
    cairo_dri_close (dri);
#endif

  if (menu) {
    if (menu->menu)
      cairo_menu_free (menu->menu);
    free (menu);
  }

  return return_value;
}
