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

#include "swfdec_rtmp_channel.h"

#include "swfdec_debug.h"
#include "swfdec_rtmp_socket.h"

/*** SwfdecRtmpChannel ***/

enum {
  PROP_0,
  PROP_CONNECTION
};

G_DEFINE_ABSTRACT_TYPE (SwfdecRtmpChannel, swfdec_rtmp_channel, G_TYPE_OBJECT)

static void
swfdec_rtmp_channel_get_property (GObject *object, guint param_id, GValue *value, 
    GParamSpec * pspec)
{
  SwfdecRtmpChannel *channel = SWFDEC_RTMP_CHANNEL (object);

  switch (param_id) {
    case PROP_CONNECTION:
      g_value_set_object (value, channel->conn);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_rtmp_channel_set_property (GObject *object, guint param_id, const GValue *value, 
    GParamSpec * pspec)
{
  SwfdecRtmpChannel *channel = SWFDEC_RTMP_CHANNEL (object);

  switch (param_id) {
    case PROP_CONNECTION:
      channel->conn = g_value_get_object (value);
      g_assert (channel->conn != NULL);
      swfdec_rtmp_channel_get_time (channel, &channel->timestamp);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfdec_rtmp_channel_dispose (GObject *object)
{
  SwfdecRtmpChannel *channel = SWFDEC_RTMP_CHANNEL (object);

  if (channel->recv_queue) {
    swfdec_buffer_queue_unref (channel->recv_queue);
    channel->recv_queue = NULL;
  }
  if (channel->send_queue) {
    swfdec_buffer_queue_unref (channel->send_queue);
    channel->send_queue = NULL;
  }

  G_OBJECT_CLASS (swfdec_rtmp_channel_parent_class)->dispose (object);
}

static void
swfdec_rtmp_channel_class_init (SwfdecRtmpChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_rtmp_channel_dispose;
  object_class->get_property = swfdec_rtmp_channel_get_property;
  object_class->set_property = swfdec_rtmp_channel_set_property;

  g_object_class_install_property (object_class, PROP_CONNECTION,
      g_param_spec_object ("connection", "connection", "RTMP connection this channel belongs to",
	  SWFDEC_TYPE_RTMP_CONNECTION, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
swfdec_rtmp_channel_init (SwfdecRtmpChannel *channel)
{
  swfdec_rtmp_header_invalidate (&channel->recv_cache);
  channel->recv_queue = swfdec_buffer_queue_new ();
  swfdec_rtmp_header_invalidate (&channel->send_cache);
  channel->send_queue = swfdec_buffer_queue_new ();
}

SwfdecBuffer *
swfdec_rtmp_channel_next_buffer (SwfdecRtmpChannel *channel)
{
  SwfdecRtmpChannelClass *klass;
  SwfdecRtmpPacket *packet;
  SwfdecRtmpHeader header;
  SwfdecBuffer *buffer;
  SwfdecBots *bots;
  guint i;

  g_return_val_if_fail (SWFDEC_IS_RTMP_CHANNEL (channel), NULL);
  g_return_val_if_fail (swfdec_rtmp_channel_is_registered (channel), NULL);

  buffer = swfdec_buffer_queue_pull_buffer (channel->send_queue);
  if (buffer)
    return buffer;

  klass = SWFDEC_RTMP_CHANNEL_GET_CLASS (channel);
  swfdec_rtmp_header_copy (&header, &channel->send_cache);
  packet = klass->send (channel);
  if (packet == NULL)
    return NULL;

  buffer = packet->buffer;
  bots = swfdec_bots_new ();
  header.channel = channel->channel_id;
  header.type = packet->header.type;
  header.timestamp = packet->header.timestamp;
  header.size = buffer->length;
  header.stream = channel->stream_id;

  swfdec_rtmp_header_write (&header, bots,
      swfdec_rtmp_header_diff (&header, &channel->send_cache));
  swfdec_rtmp_header_copy (&channel->send_cache, &header);

  for (i = 0; i < buffer->length; i += SWFDEC_RTMP_BLOCK_SIZE) {
    if (i != 0) {
      /* write a continuation header */
      bots = swfdec_bots_new ();
      swfdec_rtmp_header_write (&header, bots, SWFDEC_RTMP_HEADER_1_BYTE);
    }
    swfdec_bots_put_data (bots, buffer->data + i, MIN (SWFDEC_RTMP_BLOCK_SIZE, buffer->length - i));
    swfdec_buffer_queue_push (channel->send_queue, swfdec_bots_close (bots));
  }
  swfdec_rtmp_packet_free (packet);
  
  return swfdec_buffer_queue_pull_buffer (channel->send_queue);
}

static int
swfdec_rtmp_channel_compare (gconstpointer a, gconstpointer b)
{
  SwfdecRtmpChannel *ca = (SwfdecRtmpChannel *) a;
  SwfdecRtmpChannel *cb = (SwfdecRtmpChannel *) b;

  return cb->channel_id - ca->channel_id;
}

void
swfdec_rtmp_channel_register (SwfdecRtmpChannel *channel,
    guint channel_id, guint stream_id)
{
  SwfdecRtmpConnection *conn;

  g_return_if_fail (SWFDEC_IS_RTMP_CHANNEL (channel));
  g_return_if_fail (!swfdec_rtmp_channel_is_registered (channel));
  g_return_if_fail (channel_id > 1);
  
  if (channel_id >= 65536 + 64) {
    SWFDEC_FIXME ("figure out how huge ids (like %u) are handled. Channel registration failed", channel_id);
    return;
  }
  SWFDEC_DEBUG ("registering %s as channel %u for stream %u", G_OBJECT_TYPE_NAME (channel),
      channel_id, stream_id);

  conn = channel->conn;
  conn->channels = g_list_insert_sorted (conn->channels, channel,
      swfdec_rtmp_channel_compare);
  channel->channel_id = channel_id;
  channel->stream_id = stream_id;
  g_object_ref (channel);

  swfdec_rtmp_socket_send (channel->conn->socket);
}

void
swfdec_rtmp_channel_unregister (SwfdecRtmpChannel *channel)
{
  g_return_if_fail (SWFDEC_IS_RTMP_CHANNEL (channel));

  if (!swfdec_rtmp_channel_is_registered (channel))
    return;

  if (channel->conn->last_send->data == channel) {
    channel->conn->last_send = channel->conn->last_send->next ?
      channel->conn->last_send->next : channel->conn->last_send->prev;
  }
  channel->conn->channels = g_list_remove (channel->conn->channels, channel);
  channel->channel_id = 0;
  channel->stream_id = 0;
  g_object_unref (channel);
}

