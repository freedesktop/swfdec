
#ifndef _SWFDEC_MOVIE_CLIP_H_
#define _SWFDEC_MOVIE_CLIP_H_

#include <swfdec_types.h>
#include <swfdec_object.h>
#include <color.h>
#include <swfdec_button.h>

G_BEGIN_DECLS

typedef struct _SwfdecDisplayList SwfdecDisplayList;
//typedef struct _SwfdecMovieClip SwfdecMovieClip;
typedef struct _SwfdecMovieClipClass SwfdecMovieClipClass;

#define SWFDEC_TYPE_MOVIE_CLIP                    (swfdec_movie_clip_get_type())
#define SWFDEC_IS_MOVIE_CLIP(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SWFDEC_TYPE_MOVIE_CLIP))
#define SWFDEC_IS_MOVIE_CLIP_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), SWFDEC_TYPE_MOVIE_CLIP))
#define SWFDEC_MOVIE_CLIP(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), SWFDEC_TYPE_MOVIE_CLIP, SwfdecMovieClip))
#define SWFDEC_MOVIE_CLIP_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), SWFDEC_TYPE_MOVIE_CLIP, SwfdecMovieClipClass))

struct _SwfdecMovieClip
{
  SwfdecObject		object;

  JSObject *		jsobj;			/* our object in javascript */
  SwfdecObject *	child;			/* object that we display (may be NULL) */
  GList *		list;			/* our contained movie clips (order by depth) */
  SwfdecEventList *	events;			/* list of events that this sprite should trigger */

  /* parenting information */
  SwfdecMovieClip *	parent;			/* the object that contains us */
  char *		name;			/* the name that this clip is referenced in slash-notation */
  unsigned int	      	depth;			/* depth in parent's display list */
  unsigned int	     	clip_depth;	      	/* clipping depth (determines visibility) */

  /* positioning - the values are applied in this order */
  SwfdecRect		original_extents;	/* the extents from all the children */
  cairo_matrix_t	original_transform;	/* the transform as set by PlaceObject */
  double      		x;			/* x offset in twips */
  double		y;	      		/* y offset in twips */
  double      		xscale;			/* x scaling factor */
  double      		yscale;			/* y scaling factor */
  int			rotation;     		/* rotation in degrees */
  cairo_matrix_t	transform;		/* transformation matrix computed from above */
  cairo_matrix_t	inverse_transform;	/* the inverse of the transformation matrix */

  /* frame information */
  int			ratio;			/* for morph shapes (FIXME: is this the same as current frame?) */
  unsigned int		current_frame;		/* frame that is currently displayed (NB: indexed from 0) */
  unsigned int	      	next_frame;		/* frame that will be displayed next, the current frame to JS */
  gboolean		stopped;		/* if we currently iterate */
  gboolean		visible;		/* whether we currently can be seen or iterate */

  /* mouse handling */
  SwfdecMovieClip *	mouse_grab;		/* the child movie that has grabbed the mouse or self */
  double		mouse_x;		/* x position of mouse */
  double		mouse_y;		/* y position of mouse */
  gboolean		mouse_in;		/* if the mouse is inside this widget */
  int			mouse_button;		/* 1 if button is pressed, 0 otherwise */

  /* color information */
  swf_color		bg_color;		/* the background color for this movie (only used in root movie) */
  SwfdecColorTransform	color_transform;	/* color transform used in this movie */

  /* audio stream handling */
  gpointer		sound_decoder;	      	/* pointer acquired via swfdec_sound_init_decoder */
  unsigned int		sound_frame;		/* up to which frame the queue contains data (starting with this frame) */
  GQueue*		sound_queue;		/* buffer queue with our data */

  /* leftover unimplemented variables from the Actionscript spec */
  int alpha;
  //droptarget
  char *target;
  char *url;

  /* for buton handling (FIXME: want a subclass?) */
  SwfdecButtonState	button_state;		/* state of the displayed button (UP/OVER/DOWN) */
};

struct _SwfdecMovieClipClass
{
  SwfdecObjectClass object_class;
};

GType		swfdec_movie_clip_get_type		(void);

SwfdecMovieClip *swfdec_movie_clip_new			(SwfdecMovieClip *	parent);
unsigned int	swfdec_movie_clip_get_n_frames		(SwfdecMovieClip *      movie);
unsigned int	swfdec_movie_clip_get_next_frame	(SwfdecMovieClip *	movie,
							 unsigned int		current_frame);
void		swfdec_movie_clip_set_child		(SwfdecMovieClip *	movie,
							 SwfdecObject *		child);

void		swfdec_movie_clip_update_matrix		(SwfdecMovieClip *	movie,
							 gboolean		invalidate);
void		swfdec_movie_clips_iterate		(SwfdecDecoder *	dec);
gboolean      	swfdec_movie_clip_handle_mouse		(SwfdecMovieClip *	movie,
							 double			x,
							 double			y,
							 int			button);

void		swfdec_movie_clip_render_audio		(SwfdecMovieClip *	movie,
							 gint16 *		dest,
							 guint			n_samples);

G_END_DECLS
#endif
