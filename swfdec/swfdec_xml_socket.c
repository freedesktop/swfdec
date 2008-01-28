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

#include <string.h>

#include "swfdec_xml_socket.h"
#include "swfdec_as_internal.h"
#include "swfdec_as_strings.h"
#include "swfdec_buffer.h"
#include "swfdec_debug.h"
#include "swfdec_loader_internal.h"
#include "swfdec_movie.h"
#include "swfdec_player_internal.h"

static GQuark xml_socket_quark = 0;

static void
swfdec_xml_socket_ensure_closed (SwfdecXmlSocket *xml)
{
  gpointer cur;

  if (xml->socket == NULL)
    return;

  swfdec_stream_set_target (SWFDEC_STREAM (xml->socket), NULL);
  g_object_unref (xml->socket);
  xml->socket = NULL;

  swfdec_player_unroot (SWFDEC_PLAYER (SWFDEC_AS_OBJECT (xml)->context), xml);
  cur = g_object_get_qdata (G_OBJECT (xml->target), xml_socket_quark);
  if (cur != xml)
    g_object_set_qdata (G_OBJECT (xml->target), xml_socket_quark, cur);
  xml->target = NULL;
}

/*** SWFDEC_STREAM_TARGET ***/

static SwfdecPlayer *
swfdec_xml_socket_stream_target_get_player (SwfdecStreamTarget *target)
{
  return SWFDEC_PLAYER (SWFDEC_AS_OBJECT (target)->context);
}

static void
swfdec_xml_socket_stream_target_error (SwfdecStreamTarget *target,
    SwfdecStream *stream)
{
  SwfdecXmlSocket *xml = SWFDEC_XML_SOCKET (target);

  if (xml->open) {
    SWFDEC_FIXME ("is onClose emitted on error?");
    swfdec_as_object_call (xml->target, SWFDEC_AS_STR_onClose, 0, NULL, NULL);
  } else {
    SwfdecAsValue value;

    SWFDEC_AS_VALUE_SET_BOOLEAN (&value, FALSE);
    swfdec_as_object_call (xml->target, SWFDEC_AS_STR_onConnect, 1, &value, NULL);
  }

  swfdec_xml_socket_ensure_closed (xml);
}

static void
swfdec_xml_socket_stream_target_parse (SwfdecStreamTarget *target,
    SwfdecStream *stream)
{
  SwfdecXmlSocket *xml = SWFDEC_XML_SOCKET (target);
  SwfdecBufferQueue *queue;
  SwfdecBuffer *buffer;
  gsize len;

  /* parse until next 0 byte or take everything */
  queue = swfdec_stream_get_queue (stream);
  while ((buffer = swfdec_buffer_queue_peek_buffer (queue))) {
    guchar *nul = memchr (buffer->data, 0, buffer->length);
    
    len = nul ? (gsize) (nul - buffer->data + 1) : buffer->length;
    g_assert (len > 0);
    swfdec_buffer_unref (buffer);
    buffer = swfdec_buffer_queue_pull (queue, len);
    swfdec_buffer_queue_push (xml->queue, buffer);
    if (nul) {
      len = swfdec_buffer_queue_get_depth (xml->queue);
      g_assert (len > 0);
      buffer = swfdec_buffer_queue_pull (xml->queue, len);
      if (!g_utf8_validate ((char *) buffer->data, len, NULL)) {
	SWFDEC_FIXME ("invalid utf8 sent through socket, what now?");
      } else {
	SwfdecAsValue val;

	SWFDEC_AS_VALUE_SET_STRING (&val, swfdec_as_context_get_string (
	      SWFDEC_AS_OBJECT (xml)->context, (char *) buffer->data));
	swfdec_as_object_call (xml->target, SWFDEC_AS_STR_onData, 1, &val, NULL);
      }
    }
  }
}

static void
swfdec_xml_socket_stream_target_close (SwfdecStreamTarget *target,
    SwfdecStream *stream)
{
  SwfdecXmlSocket *xml = SWFDEC_XML_SOCKET (target);

  if (swfdec_buffer_queue_get_depth (xml->queue)) {
    SWFDEC_FIXME ("data left in socket, what now?");
  }

  swfdec_as_object_call (xml->target, SWFDEC_AS_STR_onClose, 0, NULL, NULL);

  swfdec_xml_socket_ensure_closed (xml);
}

static void
swfdec_xml_socket_stream_target_init (SwfdecStreamTargetInterface *iface)
{
  iface->get_player = swfdec_xml_socket_stream_target_get_player;
  iface->parse = swfdec_xml_socket_stream_target_parse;
  iface->close = swfdec_xml_socket_stream_target_close;
  iface->error = swfdec_xml_socket_stream_target_error;
}

/*** SWFDEC_XML_SOCKET ***/

G_DEFINE_TYPE_WITH_CODE (SwfdecXmlSocket, swfdec_xml_socket, SWFDEC_TYPE_AS_OBJECT,
    G_IMPLEMENT_INTERFACE (SWFDEC_TYPE_STREAM_TARGET, swfdec_xml_socket_stream_target_init))

static void
swfdec_xml_socket_mark (SwfdecAsObject *object)
{
  swfdec_as_object_mark (SWFDEC_XML_SOCKET (object)->target);

  SWFDEC_AS_OBJECT_CLASS (swfdec_xml_socket_parent_class)->mark (object);
}

static void
swfdec_xml_socket_dispose (GObject *object)
{
  SwfdecXmlSocket *xml = SWFDEC_XML_SOCKET (object);

  swfdec_xml_socket_ensure_closed (xml);
  if (xml->queue) {
    swfdec_buffer_queue_unref (xml->queue);
    xml->queue = NULL;
  }

  G_OBJECT_CLASS (swfdec_xml_socket_parent_class)->dispose (object);
}

static void
swfdec_xml_socket_class_init (SwfdecXmlSocketClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecAsObjectClass *as_object_class = SWFDEC_AS_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_xml_socket_dispose;

  as_object_class->mark = swfdec_xml_socket_mark;
}

static void
swfdec_xml_socket_init (SwfdecXmlSocket *xml)
{
  xml->queue = swfdec_buffer_queue_new ();
}

static SwfdecXmlSocket *
swfdec_xml_socket_create (SwfdecAsObject *target, const char *hostname, guint port)
{
  SwfdecAsContext *cx = target->context;
  SwfdecXmlSocket *xml;
  SwfdecSocket *sock;

  if (!swfdec_as_context_use_mem (cx, sizeof (SwfdecXmlSocket)))
    return NULL;

  SWFDEC_FIXME ("implement security checks please");
  sock = swfdec_player_create_socket (SWFDEC_PLAYER (cx), hostname, port);
  if (sock == NULL)
    return NULL;

  xml = g_object_new (SWFDEC_TYPE_XML_SOCKET, NULL);
  swfdec_as_object_add (SWFDEC_AS_OBJECT (xml), cx, sizeof (SwfdecXmlSocket));
  swfdec_player_root (SWFDEC_PLAYER (cx), xml, (GFunc) swfdec_as_object_mark);

  xml->target = target;
  xml->socket = sock;

  return xml;
}

/*** AS CODE ***/

SWFDEC_AS_NATIVE (400, 0, swfdec_xml_socket_connect)
void
swfdec_xml_socket_connect (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  const char *host;
  int port;

  SWFDEC_AS_CHECK (0, NULL, "si", &host, &port);

  if (SWFDEC_IS_MOVIE (object) || object == NULL)
    return;

  swfdec_xml_socket_create (object, host, port);
  SWFDEC_STUB ("XMLSocket.connect");
}

SWFDEC_AS_NATIVE (400, 1, swfdec_xml_socket_send)
void
swfdec_xml_socket_send (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SWFDEC_STUB ("XMLSocket.send");
}

SWFDEC_AS_NATIVE (400, 2, swfdec_xml_socket_close)
void
swfdec_xml_socket_close (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SWFDEC_STUB ("XMLSocket.close");
}