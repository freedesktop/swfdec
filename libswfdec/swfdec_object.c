#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "swfdec_object.h"
#include "swfdec_debug.h"
#include "swfdec_decoder.h"


static void swfdec_object_base_init (gpointer g_class);
static void swfdec_object_class_init (gpointer g_class, gpointer user_data);
static void swfdec_object_init (GTypeInstance * instance, gpointer g_class);
static void swfdec_object_dispose (GObject * object);

static GType _swfdec_object_type;

static GObjectClass *parent_class = NULL;

GType
swfdec_object_get_type (void)
{
  if (!_swfdec_object_type) {
    static const GTypeInfo object_info = {
      sizeof (SwfdecObjectClass),
      swfdec_object_base_init,
      NULL,
      swfdec_object_class_init,
      NULL,
      NULL,
      sizeof (SwfdecObject),
      32,
      swfdec_object_init,
      NULL
    };
    _swfdec_object_type = g_type_register_static (G_TYPE_OBJECT,
        "SwfdecObject", &object_info, 0);
  }
  return _swfdec_object_type;
}

static void
swfdec_object_base_init (gpointer g_class)
{
  //GObjectClass *object_class = G_OBJECT_CLASS (g_class);

}

static void
swfdec_object_class_init (gpointer g_class, gpointer class_data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);

  gobject_class->dispose = swfdec_object_dispose;

  parent_class = g_type_class_peek_parent (gobject_class);
}

static void
swfdec_object_init (GTypeInstance * instance, gpointer g_class)
{
  SwfdecObject *object = SWFDEC_OBJECT (instance);

  SWFDEC_LOG ("ref: created %s %p", G_OBJECT_CLASS_NAME (g_class), instance);

  swfdec_rect_init_whole (&object->extents);
}

static void
swfdec_object_dispose (GObject * object)
{
  SWFDEC_LOG ("ref: disposed %s %p", G_OBJECT_TYPE_NAME (object), object);
  
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

gpointer
swfdec_object_new (SwfdecDecoder *dec, GType type)
{
  SwfdecObject *object;

  g_return_val_if_fail (dec != NULL, NULL);
  g_return_val_if_fail (g_type_is_a (type, SWFDEC_TYPE_OBJECT), NULL);
  
  object = g_object_new (type, NULL);
  object->decoder = dec;

  return object;
}

gpointer
swfdec_object_get (SwfdecDecoder * s, int id)
{
  SwfdecObject *object;
  GList *g;

  for (g = s->characters; g; g = g_list_next (g)) {
    object = SWFDEC_OBJECT (g->data);
    if (object->id == id)
      return object;
  }

  return NULL;
}

/**
 * swfdec_object_create:
 * @s: a #SwfdecDecoder
 * @id: id of the object
 * @type: the required type for the object
 *
 * Gets the object of the requested @type and with the given @id from @s.
 * If there is already a different object with the given id, return NULL.
 * If the object doesn't exist yet, create it.
 *
 * Returns: The requested object or NULL on failure;
 **/
gpointer
swfdec_object_create (SwfdecDecoder * s, int id, GType type)
{
  SwfdecObject *result;

  g_return_val_if_fail (SWFDEC_IS_DECODER (s), NULL);
  g_return_val_if_fail (id >= 0, NULL);
  g_return_val_if_fail (g_type_is_a (type, SWFDEC_TYPE_OBJECT), NULL);

  SWFDEC_INFO ("  id = %d", id);
  result = swfdec_object_get (s, id);
  if (result) {
    if (G_OBJECT_TYPE (result) == type) {
      /* FIXME: is this right? */
      SWFDEC_WARNING ("object with id %d already exists, reusing", id);
      return result;
    } else {
      SWFDEC_WARNING ("requested object type %s for id %d doesn't match real type %s",
	  G_OBJECT_TYPE_NAME (result), id, g_type_name (type));
      return NULL;
    }
  }
  result = swfdec_object_new (s, type);
  result->id = id;
  s->characters = g_list_prepend (s->characters, result);

  return result;
}

/**
 * swfdec_object_render:
 * @object: the #SwfdecObject to render
 * @cr: the cairo context to render to
 * @color: a color transformation to apply
 * @inval: the reegion that was invalidated
 * @fill: TRUE if the area should be painted, FALSE to just append the path to @cr.
 *        This will only be set to FALSE for clipping.
 *
 * Renders @object. Depending on the @fill parameter this will just append the 
 * path to @cr or draw the contents.
 **/
void
swfdec_object_render (SwfdecObject *object, cairo_t *cr, 
    const SwfdecColorTransform *color, const SwfdecRect *inval, gboolean fill)
{
  SwfdecObjectClass *klass;

  g_return_if_fail (SWFDEC_IS_OBJECT (object));
  g_return_if_fail (cr != NULL);
  if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
    g_warning ("%s", cairo_status_to_string (cairo_status (cr)));
  }
  g_return_if_fail (color != NULL);
  g_return_if_fail (inval != NULL);
  
  klass = SWFDEC_OBJECT_GET_CLASS (object);
  if (klass->render == NULL)
    return;

  cairo_save (cr);

  if (swfdec_rect_intersect (NULL, &object->extents, inval)) {
    SWFDEC_LOG ("really rendering %s %p (id %d)", G_OBJECT_TYPE_NAME (object), object, object->id);
    klass->render (object, cr, color, inval, fill);
  } else {
    SWFDEC_LOG ("not rendering %s %p (id %d), extents %g %g  %g %g are not in invalid area %g %g  %g %g",
	G_OBJECT_TYPE_NAME (object), object, object->id,
	object->extents.x0, object->extents.y0, object->extents.x1, object->extents.y1,
	inval->x0, inval->y0, inval->x1, inval->y1);
  }
  if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
    g_warning ("%s", cairo_status_to_string (cairo_status (cr)));
  }
  cairo_restore (cr);
}

/**
 * swfdec_object_mouse_in:
 * @object: the object that should handle the mouse
 * @x: x position of mouse
 * @y: y position of mouse
 * @button: 1 if the mouse button was pressed, 0 otherwise
 *
 * Handles a mouse button update. This function can also be used for collision 
 * detection.
 *
 * Returns: TRUE if the mouse is inside this object, FALSE otherwise 
 **/
gboolean
swfdec_object_mouse_in (SwfdecObject *object, double x, double y, 
    int button)
{
  SwfdecObjectClass *klass;

  g_return_val_if_fail (SWFDEC_IS_OBJECT (object), FALSE);
  g_return_val_if_fail (button == 0 || button == 1, FALSE);

  SWFDEC_LOG ("%s %d mouse check at %g %g", G_OBJECT_TYPE_NAME (object), object->id,
      x, y);
  klass = SWFDEC_OBJECT_GET_CLASS (object);
  if (klass->mouse_in == NULL)
    return FALSE;
  if (!swfdec_rect_contains (&object->extents, x, y))
    return FALSE;

  if (klass->mouse_in (object, x, y, button)) {
    SWFDEC_DEBUG ("%s %d has the mouse!", G_OBJECT_TYPE_NAME (object), object->id);
    return TRUE;
  }
  return FALSE;
}

