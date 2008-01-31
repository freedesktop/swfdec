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

#ifndef _SWFDEC_TEST_PLUGIN_H_
#define _SWFDEC_TEST_PLUGIN_H_

/*
 * SwfdecTestPlugin:
 * @filename: The absolute filename to run
 * @trace: Function the plugin should call when a trace string was captured.
 * @quit: Function to call when the player quits, most likely by encountering
 *        an fscommand. The @finish function will still be called.
 * @error: Function to call whenever the plugin encounters an error. This will
 *         cause the plugin to not call any other function but @finish anymore
 *         and raise an exception containing the provided description.
 * @advance: Used to advance the player by the provided number of milliseconds
 * @finish: Clean up any leftover data as the plugin is aout to be discarded
 *
 * The SwfdecTestPlugin structure is used to hold the functions used for 
 * communication between the plugin and the player. It contains two types of
 * values: Those initialized by the player prior to calling 
 * swfdec_test_plugin_init() to be used by the plugin and those to be 
 * initialized by the plugin during swfdec_test_plugin_init() to be used by
 * the test application for driving the player.
 */
typedef struct _SwfdecTestPlugin SwfdecTestPlugin;
struct _SwfdecTestPlugin {
  /* initialized by the player before calling swfdec_test_plugin_new() */
  char *	filename;
  void		(* trace)	(SwfdecTestPlugin *	plugin,
				 const char *		string);
  void		(* quit)	(SwfdecTestPlugin *	plugin);	  
  void		(* error)	(SwfdecTestPlugin *	plugin,
				 const char *		description);
  /* initialized by the plugin during swfdec_test_plugin_new() */
  void		(* advance)	(SwfdecTestPlugin *	plugin,
				 unsigned int		msecs);
  void		(* finish)	(SwfdecTestPlugin *	plugin);
  void *	data;
};

/**
 * swfdec_test_plugin_init:
 * @SwfdecTestPlugin: the plugin instance to be initialized
 *
 * The only function exported by the plugin. It gets called whenever a new 
 * plugin should be instantiated. The plugin should instantiate the player
 * and do any initialization necessary on it using the provided filename.
 *
 * Returns: a new SwfdecTestPlugin instance
 **/
void swfdec_test_plugin_init (SwfdecTestPlugin *plugin);

#endif