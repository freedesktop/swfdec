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

#include <libsoup/soup.h>
#include <string.h>
#include "swfdec_gtk_loader.h"

/*** GTK-DOC ***/

/**
 * SECTION:SwfdecGtkLoader
 * @title: SwfdecGtkLoader
 * @short_description: advanced loader able to load network ressources
 * @see_also: #SwfdecLoader
 *
 * #SwfdecGtkLoader is a #SwfdecLoader that is intended as an easy way to be 
 * access ressources that are not stored in files, such as HTTP. It can 
 * however be compiled with varying support for different protocols, so don't
 * rely on support for a particular protocol being available. If you need this,
 * code your own SwfdecLoader subclass.
 */

/**
 * SwfdecGtkLoader:
 *
 * This is the object used to represent a loader. Since it may use varying 
 * backends, it is completely private.
 */

struct _SwfdecGtkLoader
{
  SwfdecLoader		loader;

  SoupMessage *		message;	/* the message we're sending */
  gboolean		opened;		/* set after first bytes of data have arrived */
};

struct _SwfdecGtkLoaderClass {
  SwfdecLoaderClass	loader_class;

  SoupSession *		session;	/* the session used by the loader */
};

/*** SwfdecGtkLoader ***/

G_DEFINE_TYPE (SwfdecGtkLoader, swfdec_gtk_loader, SWFDEC_TYPE_FILE_LOADER)

static void
swfdec_gtk_loader_dispose (GObject *object)
{
  SwfdecGtkLoader *gtk = SWFDEC_GTK_LOADER (object);

  if (gtk->message) {
    g_object_unref (gtk->message);
    gtk->message = NULL;
  }

  G_OBJECT_CLASS (swfdec_gtk_loader_parent_class)->dispose (object);
}

static void
swfdec_gtk_loader_open (SoupMessage *msg, gpointer loader)
{
  g_print ("open %u\n", msg->status_code);
}

static void
swfdec_gtk_loader_push (SoupMessage *msg, gpointer loader)
{
  SwfdecGtkLoader *gtk = SWFDEC_GTK_LOADER (loader);
  SwfdecBuffer *buffer;

  if (!gtk->opened) {
    char *real_uri = soup_uri_to_string (soup_message_get_uri (msg), TRUE);
    g_print ("open %s\n", real_uri);
    swfdec_loader_open (loader, real_uri);
    gtk->opened = TRUE;
    g_free (real_uri);
  }
  g_print ("push\n");
  buffer = swfdec_buffer_new_and_alloc (msg->response.length);
  memcpy (buffer->data, msg->response.body, msg->response.length);
  g_print (" ... %u bytes\n", msg->response.length);
  swfdec_loader_push (loader, buffer);
}

static void
swfdec_gtk_loader_eof (SoupMessage *msg, gpointer loader)
{
  g_print ("eof\n");
  swfdec_loader_eof (loader);
}

static void
swfdec_gtk_loader_load (SwfdecLoader *loader,
    SwfdecLoaderRequest request, const char *data, gsize data_len)
{
  const SwfdecURL *url = swfdec_loader_get_url (loader);

  if (g_ascii_strcasecmp (swfdec_url_get_protocol (url), "http") != 0 &&
      g_ascii_strcasecmp (swfdec_url_get_protocol (url), "https") != 0) {
    SWFDEC_LOADER_CLASS (swfdec_gtk_loader_parent_class)->load (loader, request, data, data_len);
  } else {
    SwfdecGtkLoader *gtk = SWFDEC_GTK_LOADER (loader);
    SwfdecGtkLoaderClass *klass = SWFDEC_GTK_LOADER_GET_CLASS (gtk);

    gtk->message = soup_message_new (request == SWFDEC_LOADER_REQUEST_POST ? "POST" : "GET",
	swfdec_url_get_url (swfdec_loader_get_url (loader)));
    soup_message_set_flags (gtk->message, SOUP_MESSAGE_OVERWRITE_CHUNKS);
    g_signal_connect (gtk->message, "got-headers", G_CALLBACK (swfdec_gtk_loader_open), gtk);
    g_signal_connect (gtk->message, "got-chunk", G_CALLBACK (swfdec_gtk_loader_push), gtk);
    if (data)
      soup_message_set_request (gtk->message, "appliation/x-www-urlencoded",
	  SOUP_BUFFER_USER_OWNED, (char *) data, data_len);
    soup_session_queue_message (klass->session, gtk->message, swfdec_gtk_loader_eof, gtk);
  }
}

static void
swfdec_gtk_loader_class_init (SwfdecGtkLoaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecLoaderClass *loader_class = SWFDEC_LOADER_CLASS (klass);

  object_class->dispose = swfdec_gtk_loader_dispose;

  loader_class->load = swfdec_gtk_loader_load;
  
  g_thread_init (NULL);
  klass->session = soup_session_async_new ();
}

static void
swfdec_gtk_loader_init (SwfdecGtkLoader *gtk_loader)
{
}

/**
 * swfdec_gtk_loader_new:
 * @uri: The location of the file to open
 *
 * Creates a new loader for the given URI. The uri must be a valid UTF-8-encoded
 * URL. 
 *
 * Returns: a new #SwfdecGtkLoader
 **/
SwfdecLoader *
swfdec_gtk_loader_new (const char *uri)
{
  SwfdecLoader *loader;

  g_return_val_if_fail (uri != NULL, NULL);

  loader = g_object_new (SWFDEC_TYPE_GTK_LOADER, NULL);
  loader->url = swfdec_url_new (uri);
  swfdec_gtk_loader_load (loader, SWFDEC_LOADER_REQUEST_DEFAULT, NULL, 0);
  return loader;
}
