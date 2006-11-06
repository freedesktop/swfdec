/* Swfdec
 * Copyright (C) 2006 Benjamin Otte <otte@gnome.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gtk/gtk.h>
#include <math.h>
#include <libswfdec/swfdec.h>

#include "swfdec_playback.h"
#include "swfdec_source.h"
#include "swfdec_widget.h"

static gpointer playback;

static void
set_title (GtkWindow *window, const char *filename)
{
  char *name = g_filename_display_basename (filename);
  char *title = g_strdup_printf ("%s : Swfplay", name);

  g_free (name);
  gtk_window_set_title (window, title);
  g_free (title);
}

static GtkWidget *
view_swf (SwfdecPlayer *player, double scale, gboolean use_image)
{
  GtkWidget *window, *widget;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  widget = swfdec_widget_new (player);
  swfdec_widget_set_scale (SWFDEC_WIDGET (widget), scale);
  swfdec_widget_set_use_image (SWFDEC_WIDGET (widget), use_image);
  gtk_container_add (GTK_CONTAINER (window), widget);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show_all (window);

  return window;
}

static void
play_swf (SwfdecPlayer *player)
{
  guint id;

  id = swfdec_iterate_add (player);

  gtk_main ();

  g_source_remove (id);
}

static void
print_trace (SwfdecPlayer *player, const char *message, gpointer unused)
{
  g_print ("%s\n", message);
}

int 
main (int argc, char *argv[])
{
  int ret = 0;
  double scale;
  SwfdecPlayer *player;
  GError *error = NULL;
  gboolean use_image = FALSE, no_sound = FALSE;
  gboolean trace = FALSE;
  GtkWidget *window;

  GOptionEntry options[] = {
    { "image", 'i', 0, G_OPTION_ARG_NONE, &use_image, "use an intermediate image surface for drawing", NULL },
    { "no-sound", 'n', 0, G_OPTION_ARG_NONE, &no_sound, "don't play sound", NULL },
    { "scale", 's', 0, G_OPTION_ARG_INT, &ret, "scale factor", "PERCENT" },
    { "trace", 't', 0, G_OPTION_ARG_NONE, &trace, "print trace output to stdout", NULL },
    { NULL }
  };
  GOptionContext *ctx;

  ctx = g_option_context_new ("");
  g_option_context_add_main_entries (ctx, options, "options");
  g_option_context_add_group (ctx, gtk_get_option_group (TRUE));
  g_option_context_parse (ctx, &argc, &argv, &error);
  g_option_context_free (ctx);

  if (error) {
    g_printerr ("Error parsing command line arguments: %s\n", error->message);
    g_error_free (error);
    return 1;
  }

  scale = ret / 100.;
  swfdec_init ();

  if (argc < 2) {
    g_printerr ("Usage: %s [OPTIONS] filename\n", argv[0]);
    return 1;
  }

  if (trace) {
    SwfdecLoader *loader;
    
    player = swfdec_player_new ();
    loader = swfdec_loader_new_from_file (argv[1], &error);
    if (loader == NULL) {
      g_object_unref (player);
      player = NULL;
    } else {
      g_signal_connect (player, "trace", G_CALLBACK (print_trace), NULL);
      swfdec_player_set_loader (player, loader);
    }
  } else {
    player = swfdec_player_new_from_file (argv[1], &error);
  }
  if (player == NULL) {
    g_printerr ("Couldn't open file \"%s\": %s\n", argv[1], error->message);
    g_error_free (error);
    return 1;
  }
  /* FIXME add smarter "not my file" detection */
  if (swfdec_player_get_rate (player) == 0) {
    g_printerr ("File \"%s\" is not a SWF file\n", argv[1]);
    g_object_unref (player);
    player = NULL;
    return 1;
  }

  window = view_swf (player, scale, use_image);
  set_title (GTK_WINDOW (window), argv[1]);

  if (no_sound) {
    playback = NULL;
  } else {
    playback = swfdec_playback_open (player);
  }
  play_swf (player);

  if (playback)
    swfdec_playback_close (playback);

  g_object_unref (player);
  player = NULL;
  return 0;
}

