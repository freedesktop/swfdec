/* Swfdec
 * Copyright (C) 2008 Benjamin Otte <otte@gnome.org>
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

#include "swfdec_test_plugin.h"
#include <swfdec/swfdec.h>
#include "swfdec_test_test.h"

static void
swfdec_test_plugin_swfdec_advance (SwfdecTestPlugin *plugin, unsigned int msecs)
{
  swfdec_player_advance (plugin->data, msecs);
}

static void
swfdec_test_plugin_swfdec_finish (SwfdecTestPlugin *plugin)
{
  g_object_unref (plugin->data);
}

static void
swfdec_test_plugin_swfdec_trace (SwfdecPlayer *player, const char *message, 
    SwfdecTestPlugin *plugin)
{
  plugin->trace (plugin, message);
}

static void
swfdec_test_plugin_swfdec_fscommand (SwfdecPlayer *player, const char *command, 
    const char *para, SwfdecTestPlugin *plugin)
{
  if (g_ascii_strcasecmp (command, "quit") == 0) {
    plugin->quit (plugin);
  }
}

void
swfdec_test_plugin_swfdec_new (SwfdecTestPlugin *plugin)
{
  SwfdecPlayer *player;
  SwfdecURL *url;

  plugin->advance = swfdec_test_plugin_swfdec_advance;
  plugin->finish = swfdec_test_plugin_swfdec_finish;
  plugin->data = player = swfdec_player_new (NULL);

  g_signal_connect (player, "fscommand", G_CALLBACK (swfdec_test_plugin_swfdec_fscommand), plugin);
  g_signal_connect (player, "trace", G_CALLBACK (swfdec_test_plugin_swfdec_trace), plugin);
  url = swfdec_url_new_from_input (plugin->filename);
  swfdec_player_set_url (player, url);
  swfdec_url_free (url);
  swfdec_player_advance (player, 0);
}