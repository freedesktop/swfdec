/* Swfdec
 * Copyright (C) 2003-2006 David Schleef <ds@schleef.org>
 *		 2005-2006 Eric Anholt <eric@anholt.net>
 *		      2006 Benjamin Otte <otte@gnome.org>
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
#include "swfdec_audio_stream.h"
#include "swfdec_debug.h"
#include "swfdec_player_internal.h"
#include "swfdec_sound.h"
#include "swfdec_sprite.h"


G_DEFINE_TYPE (SwfdecAudioStream, swfdec_audio_stream, SWFDEC_TYPE_AUDIO)

static void
swfdec_audio_stream_dispose (GObject *object)
{
  SwfdecAudioStream *stream = SWFDEC_AUDIO_STREAM (object);

  if (stream->decoder != NULL) {
    swfdec_sound_finish_decoder (stream->sound, stream->decoder);
    stream->decoder = NULL;
  }
  g_queue_foreach (stream->playback_queue, (GFunc) swfdec_buffer_unref, NULL);
  g_queue_free (stream->playback_queue);

  G_OBJECT_CLASS (swfdec_audio_stream_parent_class)->dispose (object);
}

/* return TRUE if done */
static gboolean
swfdec_audio_stream_iterate (SwfdecAudio *audio, guint remove)
{
  SwfdecAudioStream *stream = SWFDEC_AUDIO_STREAM (audio);
  SwfdecBuffer *buffer;

  stream->playback_skip += remove;
  buffer = g_queue_peek_head (stream->playback_queue);
  while (buffer && stream->playback_skip >= buffer->length / 4) {
    buffer = g_queue_pop_head (stream->playback_queue);
    stream->playback_skip -= buffer->length / 4;
    swfdec_buffer_unref (buffer);
    buffer = g_queue_peek_head (stream->playback_queue);
  }
  
  return stream->done && g_queue_is_empty (stream->playback_queue);
}

static SwfdecBuffer *
swfdec_audio_stream_decode_one (SwfdecAudioStream *stream)
{
  SwfdecSpriteFrame *frame;
  SwfdecBuffer *buffer;

  g_assert (!stream->done);
  while (!stream->done) {
    frame = &stream->sprite->frames[stream->current_frame];
    stream->current_frame = swfdec_sprite_get_next_frame (stream->sprite, stream->current_frame);
    if (stream->current_frame == 0)
      stream->done = TRUE;
    if (frame->sound_head != stream->sound) {
      stream->done = TRUE;
      return NULL;
    }
    if (frame->sound_samples == 0)
      continue;
    /* FIXME: with this method and mad not giving out full samples, we end up 
     * putting silence too early */
    if (frame->sound_block) {
      buffer = swfdec_sound_decode_buffer (stream->sound, stream->decoder, frame->sound_block);
      if (buffer == NULL)
	continue;
    } else {
      /* wanna speed this up by not allocating buffers? */
      buffer = swfdec_buffer_new_and_alloc (frame->sound_samples * 4);
      memset (buffer, 0, buffer->length);
    }
    g_queue_push_tail (stream->playback_queue, buffer);
    return buffer;
  }
  swfdec_sound_finish_decoder (stream->sound, stream->decoder);
  stream->decoder = NULL;
  return NULL;
}

static void
swfdec_audio_stream_render (SwfdecAudio *audio, gint16* dest, guint start, guint n_samples)
{
  SwfdecAudioStream *stream = SWFDEC_AUDIO_STREAM (audio);
  GList *walk;
  guint samples;
  gint16 *src;
  SwfdecBuffer *buffer;

  start += stream->playback_skip;
  walk = g_queue_peek_head_link (stream->playback_queue);
  while (n_samples) {
    if (walk) {
      walk = walk->next;
      buffer = walk->data;
    } else {
      if (stream->done)
	break;
      buffer = swfdec_audio_stream_decode_one (stream);
      if (!buffer)
	break;
    }
    if (start) {
      if (buffer->length >= 4 * start) {
	start -= buffer->length / 4;
	continue;
      }
      src = (gint16 *) buffer->data + 2 * start;
      samples = buffer->length / 4 - start;
      start = 0;
    } else {
      src = (gint16 *) buffer->data;
      samples = buffer->length / 4;
    }
    samples = MIN (samples , n_samples);
    swfdec_sound_add (dest, src, samples);
    n_samples -= samples;
  }
}

static void
swfdec_audio_stream_class_init (SwfdecAudioStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecAudioClass *audio_class = SWFDEC_AUDIO_CLASS (klass);

  object_class->dispose = swfdec_audio_stream_dispose;

  audio_class->iterate = swfdec_audio_stream_iterate;
  audio_class->render = swfdec_audio_stream_render;
}

static void
swfdec_audio_stream_init (SwfdecAudioStream *stream)
{
  stream->playback_queue = g_queue_new ();
}

SwfdecAudio *
swfdec_audio_stream_new (SwfdecPlayer *player, SwfdecSprite *sprite, guint start_frame)
{
  SwfdecAudioStream *stream;
  SwfdecSpriteFrame *frame;
  
  stream = (SwfdecAudioStream *) swfdec_audio_new (player, SWFDEC_TYPE_AUDIO_STREAM);

  SWFDEC_DEBUG ("new audio stream for sprite %d, starting at %u", 
      SWFDEC_CHARACTER (sprite)->id, start_frame);
  stream->sprite = sprite;
  frame = &sprite->frames[start_frame];
  g_assert (frame->sound_head);
  stream->sound = frame->sound_head;
  stream->playback_skip = frame->sound_skip;
  stream->current_frame = start_frame;
  stream->decoder = swfdec_sound_init_decoder (stream->sound);
  return SWFDEC_AUDIO (stream);
}

