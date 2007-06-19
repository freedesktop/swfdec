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
#include <stdio.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <glib.h>
#include <glib-object.h>
#include <libswfdec/swfdec.h>
#include <libswfdec/swfdec_button.h>
#include <libswfdec/swfdec_edittext.h>
#include <libswfdec/swfdec_font.h>
#include <libswfdec/swfdec_image.h>
#include <libswfdec/swfdec_movie.h>
#include <libswfdec/swfdec_player_internal.h>
#include <libswfdec/swfdec_sprite.h>
#include <libswfdec/swfdec_shape.h>
#include <libswfdec/swfdec_sound.h>
#include <libswfdec/swfdec_swf_decoder.h>
#include <libswfdec/swfdec_swf_instance.h>
#include <libswfdec/swfdec_tag.h>
#include <libswfdec/swfdec_text.h>

static gboolean verbose = FALSE;

static const char *
get_audio_format_name (SwfdecAudioFormat format)
{
  switch (format) {
    case SWFDEC_AUDIO_FORMAT_ADPCM:
      return "ADPCM";
    case SWFDEC_AUDIO_FORMAT_MP3:
      return "MP3";
    case SWFDEC_AUDIO_FORMAT_UNCOMPRESSED:
      return "uncompressed";
    case SWFDEC_AUDIO_FORMAT_NELLYMOSER:
      return "Nellymoser";
    default:
      return "Unknown";
  }
}

static void
dump_sound (SwfdecSound *sound)
{
  g_print ("  codec: %s\n", get_audio_format_name (sound->format));
  if (verbose) {
    g_print ("  format: %uHz, %s, %ubit\n", 
	SWFDEC_AUDIO_OUT_RATE (sound->original_format),
	SWFDEC_AUDIO_OUT_IS_STEREO (sound->original_format) ? "mono" : "stereo",
	sound->width ? 16 : 8);
    g_print ("  samples: %u (%gs)\n", sound->n_samples, 
	(double) sound->n_samples / SWFDEC_AUDIO_OUT_RATE (sound->original_format));
  }
}

static void
dump_sprite (SwfdecSwfDecoder *dec, SwfdecSprite *s)
{
  if (!verbose) {
    g_print ("  %u frames\n", s->n_frames);
  } else {
    guint i, j, tag;
    SwfdecBuffer *buffer;
    SwfdecSound *sound = NULL;

    for (i = 0; i < s->n_frames; i++) {
      SwfdecSpriteFrame *frame = &s->frames[i];
      if (frame->sound_head != sound &&
	  frame->sound_block != NULL) {
	sound = frame->sound_head;
	for (j = i; j < s->n_frames; j++) {
	  SwfdecSpriteFrame *cur = &s->frames[i];
	  if (cur->sound_head != sound)
	    break;
	}
	if (sound)
	  g_print ("   %4u -%4u  sound: %s %uHz, %s, %ubit\n", i, j, 
	      get_audio_format_name (sound->format),
	      SWFDEC_AUDIO_OUT_RATE (sound->original_format),
	      SWFDEC_AUDIO_OUT_IS_STEREO (sound->original_format) ? "mono" : "stereo",
	      sound->width ? 16 : 8);
      }
    }

    j = 0;
    for (i = 0; ; i++) {
      if (!swfdec_sprite_get_action (s, i, &tag, &buffer))
	break;
      switch (tag) {
	case SWFDEC_TAG_DOACTION:
	  g_print ("   %4u script\n", j);
	  break;
	case SWFDEC_TAG_PLACEOBJECT2:
	case SWFDEC_TAG_PLACEOBJECT3:
	  {
	    SwfdecBits bits;
	    gboolean has_char, is_move;
	    guint depth;

	    swfdec_bits_init (&bits, buffer);
	    swfdec_bits_getbits (&bits, 6);
	    has_char = swfdec_bits_getbit (&bits);
	    is_move = swfdec_bits_getbit (&bits);
	    if (tag == SWFDEC_TAG_PLACEOBJECT3)
	      swfdec_bits_get_u8 (&bits);
	    depth = swfdec_bits_get_u16 (&bits);
	    g_print ("   %4u %5u %s", j, depth, is_move ? "move" : "place");
	    if (has_char) {
	      SwfdecCharacter *c;
	      c = swfdec_swf_decoder_get_character (dec, swfdec_bits_get_u16 (&bits));
	      if (c)
		g_print (" %s %u", G_OBJECT_TYPE_NAME (c), c->id);
	    }
	    g_print ("\n");
	  }
	  break;
	case SWFDEC_TAG_REMOVEOBJECT:
	case SWFDEC_TAG_REMOVEOBJECT2:
	  {
	    SwfdecBits bits;
	    swfdec_bits_init (&bits, buffer);
	    if (tag == SWFDEC_TAG_REMOVEOBJECT)
	      swfdec_bits_get_u16 (&bits);
	    g_print ("   %4u %5u remove\n", j, swfdec_bits_get_u16 (&bits));
	  }
	  break;
	case SWFDEC_TAG_SHOWFRAME:
	  j++;
	  break;
	default:
	  g_assert_not_reached ();
      }
    }
  }
}

static void
dump_path (cairo_path_t *path)
{
  int i;
  cairo_path_data_t *data = path->data;
  const char *name;

  for (i = 0; i < path->num_data; i++) {
    name = NULL;
    switch (data[i].header.type) {
      case CAIRO_PATH_CURVE_TO:
	g_print ("      curve %g %g (%g %g . %g %g)\n",
	    data[i + 3].point.x, data[i + 3].point.y,
	    data[i + 1].point.x, data[i + 1].point.y,
	    data[i + 2].point.x, data[i + 2].point.y);
	i += 3;
	break;
      case CAIRO_PATH_LINE_TO:
	name = "line ";
      case CAIRO_PATH_MOVE_TO:
	if (!name)
	  name = "move ";
	i++;
	g_print ("      %s %g %g\n", name, data[i].point.x, data[i].point.y);
	break;
      case CAIRO_PATH_CLOSE_PATH:
	g_print ("      close\n");
	break;
      default:
	g_assert_not_reached ();
	break;
    }
  }
}

static void
dump_shape (SwfdecShape *shape)
{
  SwfdecShapeVec *shapevec;
  unsigned int i;

  for (i = 0; i < shape->vecs->len; i++) {
    shapevec = &g_array_index (shape->vecs, SwfdecShapeVec, i);

    g_print("   %3u: ", shapevec->last_index);
    if (SWFDEC_IS_PATTERN (shapevec->pattern)) {
      SwfdecPattern *pattern = shapevec->pattern;
      char *str = swfdec_pattern_to_string (pattern);
      g_print ("%s\n", str);
      g_free (str);
      if (verbose) {
	g_print ("        %g %g  %g %g  %g %g\n", 
	    pattern->start_transform.xx, pattern->start_transform.xy,
	    pattern->start_transform.yx, pattern->start_transform.yy,
	    pattern->start_transform.x0, pattern->start_transform.y0);
      }
    } else if (SWFDEC_IS_STROKE (shapevec->pattern)) {
      SwfdecStroke *line = SWFDEC_STROKE (shapevec->pattern);
      g_print ("line (width %u, color #%08X)\n", line->start_width, line->start_color);
    } else {
      g_print ("not filled\n");
    }
    if (verbose) {
      dump_path (&shapevec->path);
    }
  }
}

static void
dump_edit_text (SwfdecEditText *text)
{
  g_print ("  %s\n", text->text ? text->text : "");
  if (verbose) {
    if (text->variable)
      g_print ("  variable %s\n", text->variable);
    else
      g_print ("  no variable\n");
  }
}

static void
dump_text (SwfdecText *text)
{
  guint i;
  gunichar2 uni[text->glyphs->len];
  char *s;

  for (i = 0; i < text->glyphs->len; i++) {
    SwfdecTextGlyph *glyph = &g_array_index (text->glyphs, SwfdecTextGlyph, i);
    uni[i] = g_array_index (glyph->font->glyphs, SwfdecFontEntry, glyph->glyph).value;
    if (uni[i] == 0)
      goto fallback;
  }
  s = g_utf16_to_utf8 (uni, text->glyphs->len, NULL, NULL, NULL);
  if (s == NULL)
    goto fallback;
  g_print ("  text: %s\n", s);
  g_free (s);
  return;

fallback:
  g_print ("  %u characters\n", text->glyphs->len);
}

static void
dump_font (SwfdecFont *font)
{
  unsigned int i;
  if (font->name)
    g_print ("  %s\n", font->name);
  g_print ("  %u characters\n", font->glyphs->len);
  if (verbose) {
    for (i = 0; i < font->glyphs->len; i++) {
      gunichar2 c = g_array_index (font->glyphs, SwfdecFontEntry, i).value;
      char *s;
      if (c == 0 || (s = g_utf16_to_utf8 (&c, 1, NULL, NULL, NULL)) == NULL) {
	g_print (" ");
      } else {
	if (g_str_equal (s, "D"))
	  dump_shape (g_array_index (font->glyphs, SwfdecFontEntry, i).shape);
	g_print ("%s ", s);
	g_free (s);
      }
    }
    g_print ("\n");
  }
}

static void
dump_button (SwfdecButton *button)
{
  GList *walk;

#define SWFDEC_CONTENT_IN_STATE(content, state) \
  ((content)->sequence->start <= state && \
   (content)->sequence->end > state)
  if (verbose) {
    for (walk = button->records; walk; walk = walk->next) {
      SwfdecContent *content = walk->data;

      g_print ("  %s %s %s %s  %s %d\n", 
	  SWFDEC_CONTENT_IN_STATE (content, SWFDEC_BUTTON_UP) ? "U" : " ",
	  SWFDEC_CONTENT_IN_STATE (content, SWFDEC_BUTTON_OVER) ? "O" : " ",
	  SWFDEC_CONTENT_IN_STATE (content, SWFDEC_BUTTON_DOWN) ? "D" : " ",
	  SWFDEC_CONTENT_IN_STATE (content, SWFDEC_BUTTON_HIT) ? "H" : " ",
	  G_OBJECT_TYPE_NAME (content->graphic), SWFDEC_CHARACTER (content->graphic)->id);
    }
  }
}

const char *
get_image_type_name (SwfdecImageType type)
{
  switch (type) {
    case SWFDEC_IMAGE_TYPE_JPEG:
      return "JPEG with global table";
    case SWFDEC_IMAGE_TYPE_JPEG2:
      return "JPEG";
    case SWFDEC_IMAGE_TYPE_JPEG3:
      return "JPEG with alpha";
    case SWFDEC_IMAGE_TYPE_LOSSLESS:
      return "lossless";
    case SWFDEC_IMAGE_TYPE_LOSSLESS2:
      return "lossless with alpha";
    default:
      g_assert_not_reached ();
      return "Unknown";
  }
}

static void
dump_image (SwfdecImage *image)
{
  cairo_surface_destroy (swfdec_image_create_surface (image));
  g_print ("  %s %u x %u\n", get_image_type_name (image->type),
      image->width, image->height);
}

static void 
dump_object (gpointer key, gpointer value, gpointer dec)
{
  SwfdecCharacter *c = value;

  g_print ("%d: %s\n", c->id, G_OBJECT_TYPE_NAME (c));
  if (verbose && SWFDEC_IS_GRAPHIC (c)) {
    SwfdecGraphic *graphic = SWFDEC_GRAPHIC (c);
    g_print ("  extents: %g %g  %g %g\n", graphic->extents.x0, graphic->extents.y0,
	graphic->extents.x1, graphic->extents.y1);
  }
  if (SWFDEC_IS_IMAGE (c)) {
    dump_image (SWFDEC_IMAGE (c));
  }
  if (SWFDEC_IS_SPRITE (c)) {
    dump_sprite (dec, SWFDEC_SPRITE (c));
  }
  if (SWFDEC_IS_SHAPE(c)) {
    dump_shape(SWFDEC_SHAPE(c));
  }
  if (SWFDEC_IS_TEXT (c)) {
    dump_text (SWFDEC_TEXT (c));
  }
  if (SWFDEC_IS_EDIT_TEXT (c)) {
    dump_edit_text (SWFDEC_EDIT_TEXT (c));
  }
  if (SWFDEC_IS_FONT (c)) {
    dump_font (SWFDEC_FONT (c));
  }
  if (SWFDEC_IS_BUTTON (c)) {
    dump_button (SWFDEC_BUTTON (c));
  }
  if (SWFDEC_IS_SOUND (c)) {
    dump_sound (SWFDEC_SOUND (c));
  }
}

int
main (int argc, char *argv[])
{
  char *fn = "it.swf";
  SwfdecSwfDecoder *s;
  SwfdecPlayer *player;
  GError *error = NULL;
  GOptionEntry options[] = {
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "bew verbose", NULL },
    { NULL }
  };
  GOptionContext *ctx;

  ctx = g_option_context_new ("");
  g_option_context_add_main_entries (ctx, options, "options");
  g_option_context_parse (ctx, &argc, &argv, &error);
  g_option_context_free (ctx);
  if (error) {
    g_printerr ("Error parsing command line arguments: %s\n", error->message);
    g_error_free (error);
    return 1;
  }

  swfdec_init();

  if(argc>=2){
	fn = argv[1];
  }

  player = swfdec_player_new_from_file (argv[1]);
  if (player->loader->error) {
    g_printerr ("Couldn't open file \"%s\": %s\n", argv[1], player->loader->error);
    g_object_unref (player);
    return 1;
  }
  s = (SwfdecSwfDecoder *) SWFDEC_MOVIE (player->roots->data)->swf->decoder;
  if (swfdec_player_get_rate (player) == 0 || 
      !SWFDEC_IS_SWF_DECODER (s)) {
    g_printerr ("File \"%s\" is not a SWF file\n", argv[1]);
    g_object_unref (player);
    player = NULL;
    return 1;
  }

  g_print ("file:\n");
  g_print ("  version: %d\n", s->version);
  g_print ("  rate   : %g fps\n",  SWFDEC_DECODER (s)->rate / 256.0);
  g_print ("  size   : %ux%u pixels\n", SWFDEC_DECODER (s)->width, SWFDEC_DECODER (s)->height);
  g_print ("objects:\n");
  g_hash_table_foreach (s->characters, dump_object, s);

  g_print ("main sprite:\n");
  dump_sprite (s, s->main_sprite);
  g_object_unref (player);
  s = NULL;
  player = NULL;

  return 0;
}

