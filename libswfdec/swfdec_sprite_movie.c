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

#include "swfdec_sprite_movie.h"
#include "swfdec_audio_event.h"
#include "swfdec_audio_stream.h"
#include "swfdec_debug.h"
#include "swfdec_js.h"
#include "swfdec_player_internal.h"
#include "swfdec_ringbuffer.h"
#include "swfdec_root_movie.h"
#include "swfdec_sprite.h"

static SwfdecMovie *
swfdec_sprite_movie_find (GList *movie_list, guint depth)
{
  GList *walk;

  for (walk = movie_list; walk; walk = walk->next) {
    SwfdecMovie *movie = walk->data;

    if (movie->content->depth < depth)
      continue;
    if (movie->content->depth == depth)
      return movie;
    break;
  }
  return NULL;
}

static gboolean
swfdec_sprite_movie_remove_child (SwfdecMovie *movie, guint depth)
{
  SwfdecMovie *child = swfdec_movie_find (movie, depth);

  if (child == NULL)
    return FALSE;

  swfdec_movie_remove (child);
  return TRUE;
}

static void
swfdec_sprite_movie_run_script (SwfdecMovie *movie, gpointer data)
{
  SwfdecPlayer *player = SWFDEC_ROOT_MOVIE (movie->root)->player;
  swfdec_js_execute_script (player, movie, data, NULL);
}

static void
swfdec_sprite_movie_perform_one_action (SwfdecSpriteMovie *movie, SwfdecSpriteAction *action,
    gboolean skip_scripts, GList **movie_list)
{
  SwfdecMovie *mov = SWFDEC_MOVIE (movie);
  SwfdecPlayer *player = SWFDEC_ROOT_MOVIE (mov->root)->player;
  SwfdecMovie *child;
  SwfdecContent *content;

  switch (action->type) {
    case SWFDEC_SPRITE_ACTION_SCRIPT:
      SWFDEC_LOG ("SCRIPT action");
      if (!skip_scripts) {
	swfdec_player_add_action (player, mov, swfdec_sprite_movie_run_script, action->data);
      }
      break;
    case SWFDEC_SPRITE_ACTION_ADD:
      content = action->data;
      SWFDEC_LOG ("ADD action: depth %u", content->depth);
      if (swfdec_sprite_movie_remove_child (mov, content->depth))
	SWFDEC_DEBUG ("removed a child before adding new one");
      child = swfdec_sprite_movie_find (*movie_list, content->depth);
      if (child == NULL || child->content->sequence != content->sequence) {
	child = swfdec_movie_new (mov, content);
      } else {
	swfdec_movie_set_content (child, content);
	*movie_list = g_list_remove (*movie_list, child);
	mov->list = g_list_insert_sorted (mov->list, child, swfdec_movie_compare_depths);
      }
      break;
    case SWFDEC_SPRITE_ACTION_UPDATE:
      content = action->data;
      SWFDEC_LOG ("UPDATE action: depth %u", content->depth);
      child = swfdec_movie_find (mov, content->depth);
      if (child != NULL) {
	swfdec_movie_set_content (child, content);
      } else {
	SWFDEC_WARNING ("supposed to update depth %u, but no child", content->depth);
      }
      break;
    case SWFDEC_SPRITE_ACTION_REMOVE:
      SWFDEC_LOG ("REMOVE action: depth %u", GPOINTER_TO_UINT (action->data));
      if (!swfdec_sprite_movie_remove_child (mov, GPOINTER_TO_UINT (action->data)))
	SWFDEC_WARNING ("could not remove, no child at depth %u", GPOINTER_TO_UINT (action->data));
      break;
    default:
      g_assert_not_reached ();
  }
}

static void
swfdec_sprite_movie_do_goto_frame (SwfdecMovie *mov, gpointer data)
{
  SwfdecSpriteMovie *movie = SWFDEC_SPRITE_MOVIE (mov);
  unsigned int goto_frame = GPOINTER_TO_UINT (data);
  GList *old, *walk;
  guint i, j, start;

  if (mov->will_be_removed)
    return;

  if (goto_frame == movie->current_frame)
    return;

  if (goto_frame < movie->current_frame) {
    start = 0;
    old = mov->list;
    mov->list = NULL;
  } else {
    start = movie->current_frame + 1;
    old = NULL;
  }
  if (movie->current_frame == (guint) -1 || 
      movie->sprite->frames[goto_frame].bg_color != 
      movie->sprite->frames[movie->current_frame].bg_color) {
    swfdec_movie_invalidate (mov);
  }
  movie->current_frame = goto_frame;
  SWFDEC_DEBUG ("performing goto %u -> %u for character %u", 
      start, goto_frame, SWFDEC_CHARACTER (movie->sprite)->id);
  for (i = start; i <= movie->current_frame; i++) {
    SwfdecSpriteFrame *frame = &movie->sprite->frames[i];
    if (frame->actions == NULL)
      continue;
    for (j = 0; j < frame->actions->len; j++) {
      swfdec_sprite_movie_perform_one_action (movie,
	  &g_array_index (frame->actions, SwfdecSpriteAction, j),
	  i != movie->current_frame, &old);
    }
  }
  /* FIXME: not sure about the order here, might be relevant for unload events */
  for (walk = old; walk; walk = walk->next) {
    swfdec_movie_remove (walk->data);
  }
  g_list_free (old);
}

static void
swfdec_movie_tell_about_removal (SwfdecMovie *movie)
{
  GList *walk;
  if (movie->will_be_removed)
    return;
  movie->will_be_removed = TRUE;
  for (walk = movie->list; walk; walk = walk->next) {
    swfdec_movie_tell_about_removal (walk->data);
  }
}

static void
swfdec_sprite_movie_goto (SwfdecMovie *mov, guint frame)
{
  SwfdecPlayer *player;
  GList *walk;

  g_assert (frame < mov->n_frames);

  player = SWFDEC_ROOT_MOVIE (mov->root)->player;
  SWFDEC_LOG ("queueing goto %u for %p %d", frame, mov, 
      SWFDEC_CHARACTER (SWFDEC_SPRITE_MOVIE (mov)->sprite)->id);
  
  g_assert (frame <= G_MAXINT);

  swfdec_player_add_action (player, mov,
      swfdec_sprite_movie_do_goto_frame, GUINT_TO_POINTER (frame));

  /* tell all relevant movies that they won't survive this */
  for (walk = mov->list; walk; walk = walk->next) {
    SwfdecMovie *cur = walk->data;
    if (frame < cur->content->sequence->start || 
	frame >= cur->content->sequence->end)
      swfdec_movie_tell_about_removal (cur);
  }
  mov->frame = frame;
}

/*** MOVIE ***/

G_DEFINE_TYPE (SwfdecSpriteMovie, swfdec_sprite_movie, SWFDEC_TYPE_MOVIE)

static void
swfdec_sprite_movie_dispose (GObject *object)
{
  SwfdecSpriteMovie *movie = SWFDEC_SPRITE_MOVIE (object);

  g_assert (movie->sound_stream == NULL);

  G_OBJECT_CLASS (swfdec_sprite_movie_parent_class)->dispose (object);
}

static void
swfdec_sprite_movie_queue_enter_frame (SwfdecMovie *movie, gpointer unused)
{
  if (movie->will_be_removed)
    return;
  swfdec_movie_queue_script (movie, SWFDEC_EVENT_ENTER);
}

static void
swfdec_sprite_movie_iterate (SwfdecMovie *mov)
{
  SwfdecSpriteMovie *movie = SWFDEC_SPRITE_MOVIE (mov);
  unsigned int goto_frame;

  if (mov->stopped) {
    goto_frame = mov->frame;
  } else {
    goto_frame = swfdec_sprite_get_next_frame (movie->sprite, mov->frame);
  }
  swfdec_player_add_action (SWFDEC_ROOT_MOVIE (mov->root)->player, 
      mov, swfdec_sprite_movie_queue_enter_frame, NULL);
  swfdec_sprite_movie_goto (mov, goto_frame);
}

static gboolean
swfdec_sprite_movie_iterate_end (SwfdecMovie *mov)
{
  SwfdecSpriteMovie *movie = SWFDEC_SPRITE_MOVIE (mov);
  SwfdecSpriteFrame *last;
  SwfdecSpriteFrame *current;
  GSList *walk;
  SwfdecPlayer *player = SWFDEC_ROOT_MOVIE (SWFDEC_MOVIE (movie)->root)->player;

  current = &movie->sprite->frames[movie->current_frame];
  if (!SWFDEC_MOVIE_CLASS (swfdec_sprite_movie_parent_class)->iterate_end (mov)) {
    g_assert (movie->sound_stream == NULL);
    return FALSE;
  }
  
  /* first start all event sounds */
  /* FIXME: is this correct? */
  if (movie->sound_frame != movie->current_frame) {
    for (walk = current->sound; walk; walk = walk->next) {
      swfdec_audio_event_new (player, walk->data);
    }
  }

  /* then do the streaming thing */
  if (current->sound_head == NULL ||
      SWFDEC_MOVIE (movie)->stopped) {
    if (movie->sound_stream) {
      swfdec_audio_remove (movie->sound_stream);
      g_assert (movie->sound_stream == NULL);
    }
    goto exit;
  }
  if (movie->sound_stream == NULL && current->sound_block == NULL)
    goto exit;
  SWFDEC_LOG ("iterating audio (from %u to %u)", movie->sound_frame, movie->current_frame);
  if (movie->sound_frame + 1 != movie->current_frame)
    goto new_decoder;
  if (movie->sound_frame == (guint) -1)
    goto new_decoder;
  if (current->sound_head && movie->sound_stream == NULL)
    goto new_decoder;
  last = &movie->sprite->frames[movie->sound_frame];
  if (last->sound_head != current->sound_head)
    goto new_decoder;
exit:
  movie->sound_frame = movie->current_frame;
  return TRUE;

new_decoder:
  if (movie->sound_stream)
    swfdec_audio_remove (movie->sound_stream);

  g_assert (movie->sound_stream == NULL);
  movie->sound_stream = swfdec_audio_stream_new (player, 
      movie->sprite, movie->current_frame);
  g_object_add_weak_pointer (G_OBJECT (movie->sound_stream), (gpointer *) (void *) &movie->sound_stream);
  movie->sound_frame = movie->current_frame;
  return TRUE;
}

static void
swfdec_sprite_movie_init_movie (SwfdecMovie *mov)
{
  SwfdecSpriteMovie *movie = SWFDEC_SPRITE_MOVIE (mov);

  mov->n_frames = movie->sprite->n_frames;
  swfdec_sprite_movie_do_goto_frame (mov, GUINT_TO_POINTER (0));
  if (!swfdec_sprite_movie_iterate_end (mov)) {
    g_assert_not_reached ();
  }
}

static void
swfdec_sprite_movie_finish_movie (SwfdecMovie *mov)
{
  SwfdecSpriteMovie *movie = SWFDEC_SPRITE_MOVIE (mov);
  SwfdecPlayer *player = SWFDEC_ROOT_MOVIE (mov->root)->player;

  swfdec_player_remove_all_actions (player, mov);
  if (movie->sound_stream) {
    swfdec_audio_remove (movie->sound_stream);
    g_assert (movie->sound_stream == NULL);
  }
}

static void
swfdec_sprite_movie_class_init (SwfdecSpriteMovieClass * g_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (g_class);
  SwfdecMovieClass *movie_class = SWFDEC_MOVIE_CLASS (g_class);

  object_class->dispose = swfdec_sprite_movie_dispose;

  movie_class->init_movie = swfdec_sprite_movie_init_movie;
  movie_class->finish_movie = swfdec_sprite_movie_finish_movie;
  movie_class->goto_frame = swfdec_sprite_movie_goto;
  movie_class->iterate_start = swfdec_sprite_movie_iterate;
  movie_class->iterate_end = swfdec_sprite_movie_iterate_end;
}

static void
swfdec_sprite_movie_init (SwfdecSpriteMovie * movie)
{
  movie->current_frame = (guint) -1;
  movie->sound_frame = (guint) -1;
}

gboolean
swfdec_sprite_movie_paint_background (SwfdecSpriteMovie *movie, cairo_t *cr)
{
  SwfdecSpriteFrame *frame;
  
  if (movie->current_frame >= SWFDEC_MOVIE (movie)->n_frames)
    return FALSE;

  frame = &movie->sprite->frames[movie->current_frame];
  swfdec_color_set_source (cr, frame->bg_color);
  cairo_paint (cr);
  return TRUE;
}

