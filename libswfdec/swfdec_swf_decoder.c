/* Swfdec
 * Copyright (C) 2003-2006 David Schleef <ds@schleef.org>
 *		 2005-2006 Eric Anholt <eric@anholt.net>
 *		 2006-2007 Benjamin Otte <otte@gnome.org>
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

#include <zlib.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <liboil/liboil.h>

#include "swfdec_swf_decoder.h"
#include "swfdec.h"
#include "swfdec_bits.h"
#include "swfdec_cached.h"
#include "swfdec_debug.h"
#include "swfdec_player_internal.h"
#include "swfdec_script.h"
#include "swfdec_script_internal.h"
#include "swfdec_sprite.h"
#include "swfdec_tag.h"

enum {
  SWFDEC_STATE_INIT1 = 0,
  SWFDEC_STATE_INIT2,
  SWFDEC_STATE_PARSE_FIRST_TAG,
  SWFDEC_STATE_PARSE_TAG,
  SWFDEC_STATE_EOF,
};

G_DEFINE_TYPE (SwfdecSwfDecoder, swfdec_swf_decoder, SWFDEC_TYPE_DECODER)

static void
swfdec_swf_decoder_dispose (GObject *object)
{
  SwfdecSwfDecoder *s = SWFDEC_SWF_DECODER (object);
  guint i,j;

  if (s->root_actions) {
    for (i = 0; i < s->main_sprite->n_frames; i++) {
      GArray *array = s->root_actions[i];
      if (array) {
	for (j = 0; j < array->len; j++) {
	  SwfdecRootAction *action = &g_array_index (array, SwfdecRootAction, j);

	  switch (action->type) {
	    case SWFDEC_ROOT_ACTION_EXPORT:
	      {
		SwfdecRootExportData *data = action->data;
		g_free (data->name);
		g_object_unref (data->character);
		g_free (data);
	      }
	      break;
	    case SWFDEC_ROOT_ACTION_INIT_SCRIPT:
	      swfdec_script_unref (action->data);
	      break;
	    default:
	      g_assert_not_reached ();
	      break;
	  }
	}
	g_array_free (array, TRUE);
      }
    }
    g_free (s->root_actions);
    s->root_actions = NULL;
  }

  g_hash_table_destroy (s->characters);
  g_object_unref (s->main_sprite);
  g_hash_table_destroy (s->scripts);

  if (s->compressed)
    inflateEnd (&s->z);
  if (s->buffer) {
    swfdec_buffer_unref (s->buffer);
    s->buffer = NULL;
  }

  if (s->jpegtables) {
    swfdec_buffer_unref (s->jpegtables);
  }

  g_free (s->password);

  G_OBJECT_CLASS (swfdec_swf_decoder_parent_class)->dispose (object);
}

static void *
zalloc (void *opaque, guint items, guint size)
{
  return g_malloc (items * size);
}

static void
zfree (void *opaque, void *addr)
{
  g_free (addr);
}

static gboolean
swfdec_swf_decoder_deflate_all (SwfdecSwfDecoder * s)
{
  int ret;
  SwfdecBuffer *buffer;
  SwfdecDecoder *dec = SWFDEC_DECODER (s);
  
  while ((buffer = swfdec_buffer_queue_pull_buffer (SWFDEC_DECODER (s)->queue))) {
    if (s->compressed) {
      s->z.next_in = buffer->data;
      s->z.avail_in = buffer->length;
      ret = inflate (&s->z, Z_SYNC_FLUSH);
      if (ret < Z_OK) {
	SWFDEC_ERROR ("error uncompressing data: %s", s->z.msg);
	return FALSE;
      }

      dec->bytes_loaded = s->z.total_out + 8;
    } else {
      guint max = buffer->length;

      if (dec->bytes_loaded + max > s->buffer->length) {
	max = s->buffer->length - dec->bytes_loaded;
	SWFDEC_WARNING ("%u bytes more than declared filesize", max - dec->bytes_loaded);
      }
      memcpy (s->buffer->data + dec->bytes_loaded, buffer->data, max);
      dec->bytes_loaded += max;
    }
    swfdec_buffer_unref (buffer);
  }

  return TRUE;
}

static gboolean
swf_inflate_init (SwfdecSwfDecoder * s)
{
  z_stream *z;
  int ret;
  z = &s->z;
  z->zalloc = zalloc;
  z->zfree = zfree;
  ret = inflateInit (z);
  SWFDEC_DEBUG ("inflateInit returned %d", ret);

  z->next_out = s->buffer->data + 8;
  z->avail_out = s->buffer->length - 8;
  z->opaque = NULL;
  return TRUE;
}

static void
swfdec_swf_decoder_init_bits (SwfdecSwfDecoder *dec, SwfdecBits *bits)
{
  swfdec_bits_init (bits, dec->buffer);
  bits->end = bits->ptr + SWFDEC_DECODER (dec)->bytes_loaded;
  bits->ptr += dec->bytes_parsed;
  g_assert (bits->ptr <= bits->end);
}

static void
swfdec_swf_decoder_flush_bits (SwfdecSwfDecoder *dec, SwfdecBits *bits)
{
  g_assert (bits->idx == 0);
  g_assert (bits->buffer == dec->buffer);

  dec->bytes_parsed = bits->ptr - dec->buffer->data;
  g_assert (dec->bytes_parsed <= SWFDEC_DECODER (dec)->bytes_loaded);
}

static int
swf_parse_header1 (SwfdecSwfDecoder * s)
{
  SwfdecDecoder *dec = SWFDEC_DECODER (s);
  int sig1, sig2, sig3;
  SwfdecBuffer *buffer;
  SwfdecBits bits;
  guint8 *data;

  /* NB: we're still reading from the original queue, since deflating is not initialized yet */
  buffer = swfdec_buffer_queue_pull (dec->queue, 8);
  if (buffer == NULL) {
    return SWFDEC_STATUS_NEEDBITS;
  }

  swfdec_bits_init (&bits, buffer);

  sig1 = swfdec_bits_get_u8 (&bits);
  sig2 = swfdec_bits_get_u8 (&bits);
  sig3 = swfdec_bits_get_u8 (&bits);
  if ((sig1 != 'F' && sig1 != 'C') || sig2 != 'W' || sig3 != 'S') {
    return SWFDEC_STATUS_ERROR;
  }

  s->version = swfdec_bits_get_u8 (&bits);
  dec->bytes_total = swfdec_bits_get_u32 (&bits);

  data = g_try_malloc (dec->bytes_total);
  if (data == NULL)
    return SWFDEC_STATUS_ERROR;
  s->buffer = swfdec_buffer_new_for_data (data, dec->bytes_total);
  s->compressed = (sig1 == 'C');
  if (s->compressed) {
    SWFDEC_DEBUG ("compressed");
    if (!swf_inflate_init (s))
      return SWFDEC_STATUS_ERROR;
  } else {
    SWFDEC_DEBUG ("not compressed");
  }
  memcpy (s->buffer->data, buffer->data, 8);
  SWFDEC_DECODER (s)->bytes_loaded = 8;
  s->bytes_parsed = 8;
  swfdec_buffer_unref (buffer);

  s->state = SWFDEC_STATE_INIT2;

  return SWFDEC_STATUS_OK;
}

static int
swf_parse_header2 (SwfdecSwfDecoder * s)
{
  guint n;
  SwfdecRect rect;
  SwfdecDecoder *dec = SWFDEC_DECODER (s);

  swfdec_swf_decoder_init_bits (s, &s->b);
  n = swfdec_bits_peekbits (&s->b, 5);
  /*  rect      rate + total_frames */
  n = ((5 + 4 * n + 7) / 8 + (2 + 2)) * 8;
  if (swfdec_bits_left (&s->b) < n)
    return SWFDEC_STATUS_NEEDBITS;

  swfdec_bits_get_rect (&s->b, &rect);
  if (rect.x0 != 0.0 || rect.y0 != 0.0)
    SWFDEC_ERROR ("SWF window doesn't start at 0 0 but at %g %g", rect.x0, rect.y0);
  SWFDEC_INFO ("SWF size: %g x %g pixels", rect.x1 / SWFDEC_TWIPS_SCALE_FACTOR,
      rect.y1 / SWFDEC_TWIPS_SCALE_FACTOR);
  dec->width = MAX (0, ceil (rect.x1 / SWFDEC_TWIPS_SCALE_FACTOR));
  dec->height = MAX (0, ceil (rect.y1 / SWFDEC_TWIPS_SCALE_FACTOR));
  swfdec_bits_syncbits (&s->b);
  dec->rate = swfdec_bits_get_u16 (&s->b);
  if (dec->rate == 0) {
    SWFDEC_INFO ("rate is 0, setting to 1");
    dec->rate = 1;
  }
  SWFDEC_LOG ("rate = %g", dec->rate / 256.0);
  dec->frames_total = swfdec_bits_get_u16 (&s->b);
  SWFDEC_LOG ("n_frames = %d", dec->frames_total);
  swfdec_sprite_set_n_frames (s->main_sprite, dec->frames_total, dec->rate);

  swfdec_swf_decoder_flush_bits (s, &s->b);

  s->state = SWFDEC_STATE_PARSE_FIRST_TAG;
  return SWFDEC_STATUS_INIT;
}

static SwfdecStatus
swfdec_swf_decoder_parse (SwfdecDecoder *dec)
{
  SwfdecSwfDecoder *s = SWFDEC_SWF_DECODER (dec);
  int ret = SWFDEC_STATUS_OK;

  s->b = s->parse;
  g_assert (dec->player);

  switch (s->state) {
    case SWFDEC_STATE_INIT1:
      ret = swf_parse_header1 (s);
      break;
    case SWFDEC_STATE_INIT2:
      if (!swfdec_swf_decoder_deflate_all (s))
	return SWFDEC_STATUS_ERROR;
      ret = swf_parse_header2 (s);
      break;
    case SWFDEC_STATE_PARSE_FIRST_TAG:
    case SWFDEC_STATE_PARSE_TAG:
    {
      guint header_length;
      guint x;
      SwfdecTagFunc func;
      guint tag;
      guint tag_len;
      SwfdecBits bits;

      if (!swfdec_swf_decoder_deflate_all (s))
	return SWFDEC_STATUS_ERROR;

      /* we're parsing tags */
      swfdec_swf_decoder_init_bits (s, &bits);
      if (swfdec_bits_left (&bits) < 2 * 8)
	return SWFDEC_STATUS_NEEDBITS;

      x = swfdec_bits_get_u16 (&bits);
      tag = (x >> 6) & 0x3ff;
      SWFDEC_DEBUG ("tag %d %s", tag, swfdec_swf_decoder_get_tag_name (tag));
      tag_len = x & 0x3f;
      if (tag_len == 0x3f) {
	if (swfdec_bits_left (&bits) < 4 * 8)
	  return SWFDEC_STATUS_NEEDBITS;

	tag_len = swfdec_bits_get_u32 (&bits);
	header_length = 6;
      } else {
	header_length = 2;
      }

      SWFDEC_INFO ("parsing at %d, tag %d %s, length %d",
	  s->bytes_parsed, tag,
	  swfdec_swf_decoder_get_tag_name (tag), tag_len);

      if (swfdec_bits_left (&bits) / 8 < tag_len)
	return SWFDEC_STATUS_NEEDBITS;

      swfdec_bits_init_bits (&s->b, &bits, tag_len);
      swfdec_swf_decoder_flush_bits (s, &bits);

      func = swfdec_swf_decoder_get_tag_func (tag);
      if (tag == 0) {
	s->state = SWFDEC_STATE_EOF;
      } else if ((swfdec_swf_decoder_get_tag_flag (tag) & SWFDEC_TAG_FIRST_ONLY) 
	  && s->state == SWFDEC_STATE_PARSE_TAG) {
	SWFDEC_WARNING ("tag %d %s must be first tag in file, ignoring",
	    tag, swfdec_swf_decoder_get_tag_name (tag));
      } else if (func == NULL) {
	SWFDEC_WARNING ("tag function not implemented for %d %s",
	    tag, swfdec_swf_decoder_get_tag_name (tag));
      } else if (s->main_sprite->parse_frame < s->main_sprite->n_frames) {
	s->parse_sprite = s->main_sprite;
	ret = func (s, tag);
	s->parse_sprite = NULL;

	if (swfdec_bits_left (&s->b)) {
	  SWFDEC_WARNING
	      ("early finish (%d bytes) at %d, tag %d %s, length %d",
	      swfdec_bits_left (&s->b) / 8,
	      s->bytes_parsed, tag,
	      swfdec_swf_decoder_get_tag_name (tag), tag_len);
	}
      } else {
	ret = SWFDEC_STATE_EOF;
	SWFDEC_ERROR ("data after last frame");
      }
      s->state = SWFDEC_STATE_PARSE_TAG;

      break;
    }
    case SWFDEC_STATE_EOF:
      if (s->bytes_parsed < dec->bytes_loaded) {
	SWFDEC_WARNING ("%u bytes after EOF", dec->bytes_loaded - s->bytes_parsed);
      }
      return SWFDEC_STATUS_EOF;
  }

  /* copy state */
  dec->frames_loaded = s->main_sprite->parse_frame;

  return ret;
}

static void
swfdec_swf_decoder_class_init (SwfdecSwfDecoderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  SwfdecDecoderClass *decoder_class = SWFDEC_DECODER_CLASS (class);

  object_class->dispose = swfdec_swf_decoder_dispose;

  decoder_class->parse = swfdec_swf_decoder_parse;
}

static void
swfdec_swf_decoder_init (SwfdecSwfDecoder *s)
{
  s->main_sprite = g_object_new (SWFDEC_TYPE_SPRITE, NULL);

  s->characters = g_hash_table_new_full (g_direct_hash, g_direct_equal, 
      NULL, g_object_unref);
  s->scripts = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) swfdec_script_unref);
}

gpointer
swfdec_swf_decoder_get_character (SwfdecSwfDecoder * s, guint id)
{
  g_return_val_if_fail (SWFDEC_IS_SWF_DECODER (s), NULL);

  return g_hash_table_lookup (s->characters, GUINT_TO_POINTER (id));
}

/**
 * swfdec_swf_decoder_create_character:
 * @s: a #SwfdecDecoder
 * @id: id of the character
 * @type: the required type for the character
 *
 * Gets the character of the requested @type and with the given @id from @s.
 * If there is already a different character with the given id, return NULL.
 * If the character doesn't exist yet, create it.
 *
 * Returns: The requested character or NULL on failure;
 **/
gpointer
swfdec_swf_decoder_create_character (SwfdecSwfDecoder * s, guint id, GType type)
{
  SwfdecCharacter *result;

  g_return_val_if_fail (SWFDEC_IS_DECODER (s), NULL);
  g_return_val_if_fail (g_type_is_a (type, SWFDEC_TYPE_CHARACTER), NULL);

  SWFDEC_INFO ("  id = %d", id);
  result = swfdec_swf_decoder_get_character (s, id);
  if (result) {
    SWFDEC_WARNING ("character with id %d already exists", id);
    return NULL;
  }
  result = g_object_new (type, NULL);
  result->id = id;
  g_hash_table_insert (s->characters, GUINT_TO_POINTER (id), result);
  if (SWFDEC_IS_CACHED (result)) {
    swfdec_cached_set_cache (SWFDEC_CACHED (result), SWFDEC_DECODER (s)->player->cache);
  }

  return result;
}

void
swfdec_swf_decoder_add_root_action (SwfdecSwfDecoder *s,
    SwfdecRootActionType type, gpointer data)
{
  SwfdecSprite *sprite;
  GArray *array;
  SwfdecRootAction action;

  g_return_if_fail (SWFDEC_IS_SWF_DECODER (s));
  sprite = s->main_sprite;
  g_return_if_fail (sprite->parse_frame < sprite->n_frames);

  if (s->root_actions == NULL)
    s->root_actions = g_new0 (GArray *, sprite->n_frames);

  array = s->root_actions[sprite->parse_frame];
  if (array == NULL) {
    s->root_actions[sprite->parse_frame] = 
      g_array_new (FALSE, FALSE, sizeof (SwfdecRootAction));
    array = s->root_actions[sprite->parse_frame];
  }
  action.type = type;
  action.data = data;
  g_array_append_val (array, action);
}

void
swfdec_swf_decoder_add_script (SwfdecSwfDecoder *s, SwfdecScript *script)
{
  g_return_if_fail (SWFDEC_IS_SWF_DECODER (s));
  g_return_if_fail (script != NULL);
  g_return_if_fail (script->buffer != NULL);

  g_hash_table_insert (s->scripts, (gpointer) script->main, script);
}

SwfdecScript *
swfdec_swf_decoder_get_script (SwfdecSwfDecoder *s, guint8 *data)
{
  g_return_val_if_fail (SWFDEC_IS_SWF_DECODER (s), NULL);
  g_return_val_if_fail (data != NULL, NULL);

  return g_hash_table_lookup (s->scripts, data);
}

