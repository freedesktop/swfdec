/* Swfdec
 * Copyright (C) 2007 Benjamin Otte <otte@gnome.org>
 *               2007 Pekka Lampila <pekka.lampila@iki.fi>
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

#include <stdlib.h>
#include <string.h>

#include "swfdec_as_array.h"
#include "swfdec_as_context.h"
#include "swfdec_as_frame_internal.h"
#include "swfdec_as_function.h"
#include "swfdec_as_internal.h"
#include "swfdec_as_native_function.h"
#include "swfdec_as_string.h"
#include "swfdec_as_strings.h"
#include "swfdec_movie.h"
#include "swfdec_debug.h"

G_DEFINE_TYPE (SwfdecAsArray, swfdec_as_array, SWFDEC_TYPE_AS_OBJECT)

/**
 * SECTION:SwfdecAsArray
 * @title: SwfdecAsArray
 * @short_description: the array object
 *
 * The array object provides some convenience functions for creating and
 * modifying arrays.
 */

/**
 * SwfdecAsArray
 *
 * This is the type of the array object.
 */

/*
 * Internal helper functions
 */

/* NB: type is important for overflow */
static inline gint32
swfdec_as_array_to_index (const char *str)
{
  char *end;
  gulong l;

  g_return_val_if_fail (str != NULL, -1);

  l = strtoul (str, &end, 10);

  if (*end != 0 || l > G_MAXINT32)
    return -1;

  return l;
}

static gint32
swfdec_as_array_length_as_integer (SwfdecAsObject *object)
{
  SwfdecAsValue val;
  gint32 length;

  g_return_val_if_fail (object != NULL, 0);

  swfdec_as_object_get_variable (object, SWFDEC_AS_STR_length, &val);
  length = swfdec_as_value_to_integer (object->context, &val);

  return length;
}

static gint32
swfdec_as_array_length (SwfdecAsObject *object)
{
  gint32 length;

  length = swfdec_as_array_length_as_integer (object);

  if (length < 0)
    return 0;

  return length;
}

/**
 * swfdec_as_array_get_length:
 * @array: a #SwfdecAsArray
 *
 * Gets the current length of the @array.
 *
 * Returns: Current length of the @array, always >= 0
 **/
gint32
swfdec_as_array_get_length (SwfdecAsArray *array)
{
  return swfdec_as_array_length (SWFDEC_AS_OBJECT (array));
}

static void
swfdec_as_array_set_length_object (SwfdecAsObject *object, gint32 length)
{
  SwfdecAsValue val;

  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));

  SWFDEC_AS_VALUE_SET_INT (&val, length);
  swfdec_as_object_set_variable_and_flags (object, SWFDEC_AS_STR_length, &val,
      SWFDEC_AS_VARIABLE_HIDDEN | SWFDEC_AS_VARIABLE_PERMANENT);
}

/**
 * swfdec_as_array_set_length:
 * @array: a #SwfdecAsArray
 * @length: the new length
 *
 * Sets the length of the @array. Values outside the new length will be
 * removed.
 **/
void
swfdec_as_array_set_length (SwfdecAsArray *array, gint32 length)
{
  g_return_if_fail (SWFDEC_IS_AS_ARRAY (array));
  g_return_if_fail (length >= 0);

  swfdec_as_array_set_length_object (SWFDEC_AS_OBJECT (array), length);
}

typedef struct {
  gint32	start_index;
  gint32	num;
} ForeachRemoveRangeData;

static gboolean
swfdec_as_array_foreach_remove_range (SwfdecAsObject *object,
    const char *variable, SwfdecAsValue *value, guint flags, gpointer data)
{
  ForeachRemoveRangeData *fdata = data;
  gint32 idx;

  idx = swfdec_as_array_to_index (variable);
  if (idx == -1)
    return FALSE;

  if (idx >= fdata->start_index && idx < fdata->start_index + fdata->num)
    return TRUE;

  return FALSE;
}

static void
swfdec_as_array_remove_range (SwfdecAsArray *array, gint32 start_index,
    gint32 num)
{
  SwfdecAsObject *object = SWFDEC_AS_OBJECT (array);

  g_return_if_fail (SWFDEC_IS_AS_ARRAY (array));
  g_return_if_fail (start_index >= 0);
  g_return_if_fail (num >= 0);
  g_return_if_fail (start_index + num <= swfdec_as_array_length (object));

  if (num == 0)
    return;

  // to avoid foreach loop, use special case when removing just one variable
  if (num == 1) {
    swfdec_as_object_delete_variable (object,
	swfdec_as_integer_to_string (object->context, start_index));
  } else {
    ForeachRemoveRangeData fdata = { start_index, num };
    swfdec_as_object_foreach_remove (object,
	swfdec_as_array_foreach_remove_range, &fdata);
  }
}

typedef struct {
  gint32	start_index;
  gint32	num;
  gint32	to_index;
} ForeachMoveRangeData;

static const char *
swfdec_as_array_foreach_move_range (SwfdecAsObject *object,
    const char *variable, SwfdecAsValue *value, guint flags, gpointer data)
{
  ForeachMoveRangeData *fdata = data;
  gint32 idx;

  idx = swfdec_as_array_to_index (variable);
  if (idx == -1)
    return variable;

  if (idx >= fdata->start_index && idx < fdata->start_index + fdata->num) {
    return swfdec_as_integer_to_string (object->context,
	fdata->to_index + idx - fdata->start_index);
  } else if (idx >= fdata->to_index && idx < fdata->to_index + fdata->num) {
    return NULL;
  } else {
    return variable;
  }
}

static void
swfdec_as_array_move_range (SwfdecAsObject *object, gint32 from_index,
    gint32 num, gint32 to_index)
{
  ForeachMoveRangeData fdata = { from_index, num, to_index };

  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail (from_index >= 0);
  g_return_if_fail (num >= 0);
  g_return_if_fail (from_index + num <= swfdec_as_array_length (object));
  g_return_if_fail (to_index >= 0);

  if (num == 0 || from_index == to_index)
    return;

  swfdec_as_object_foreach_rename (object, swfdec_as_array_foreach_move_range,
      &fdata);

  // only changes length if it becomes bigger, not if it becomes smaller
  if (to_index + num > swfdec_as_array_length (object))
    swfdec_as_array_set_length_object (object, to_index + num);
}

static void
swfdec_as_array_set_range_with_flags (SwfdecAsObject *object,
    gint32 start_index, gint32 num, const SwfdecAsValue *value,
    SwfdecAsVariableFlag flags)
{
  gint32 i;
  const char *var;

  // allow negative indexes
  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail (num >= 0);
  g_return_if_fail (num == 0 || value != NULL);

  for (i = 0; i < num; i++) {
    var = swfdec_as_integer_to_string (object->context, start_index + i);
    swfdec_as_object_set_variable_and_flags (object, var, &value[i], flags);
  }
}

static void
swfdec_as_array_set_range (SwfdecAsObject *object, gint32 start_index,
    gint32 num, const SwfdecAsValue *value)
{
  swfdec_as_array_set_range_with_flags (object, start_index, num, value, 0);
}

static void
swfdec_as_array_append_internal (SwfdecAsObject *object, guint n,
    const SwfdecAsValue *value)
{
  // allow negative length
  swfdec_as_array_set_range (object,
      swfdec_as_array_length_as_integer (object), n, value);
}

/**
 * swfdec_as_array_push:
 * @array: a #SwfdecAsArray
 * @value: the value to add
 *
 * Adds the given @value to the @array. This is a macro that just calls
 * swfdec_as_array_append_with_flags().
 */

/**
 * swfdec_as_array_push_with_flags:
 * @array: a #SwfdecAsArray
 * @value: the value to add
 * @flags: the #SwfdecAsVariableFlag flags to use
 *
 * Adds the given @value to the @array with the given @flags. This is a macro
 * that just calls swfdec_as_array_append_with_flags().
 */

/**
 * swfdec_as_array_append:
 * @array: a #SwfdecAsArray
 * @n: number of values to add
 * @values: the values to add
 *
 * Appends the given @values to the @array. This is a macro that just calls
 * swfdec_as_array_append_with_flags().
 **/

/**
 * swfdec_as_array_append_with_flags:
 * @array: a #SwfdecAsArray
 * @n: number of values to add
 * @values: the values to add
 * @flags: the #SwfdecAsVariableFlag flags to use
 *
 * Appends the given @values to the @array using the given @flags.
 **/
void
swfdec_as_array_append_with_flags (SwfdecAsArray *array, guint n,
    const SwfdecAsValue *value, SwfdecAsVariableFlag flags)
{
  g_return_if_fail (SWFDEC_IS_AS_ARRAY (array));
  g_return_if_fail (n == 0 || value != NULL);

  // don't allow negative length
  swfdec_as_array_set_range_with_flags (SWFDEC_AS_OBJECT (array),
      swfdec_as_array_length (SWFDEC_AS_OBJECT (array)), n, value, flags);
}

/**
 * swfdec_as_array_insert:
 * @array: a #SwfdecAsArray
 * @idx: index to insert the value to
 * @value: a #SwfdecAsValue
 *
 * Inserts @value to @array at given index, making room for it by moving
 * elements to bigger indexes if necessary. This is a macro that just calls
 * swfdec_as_array_insert_with_flags().
 **/
/**
 * swfdec_as_array_insert_with_flags:
 * @array: a #SwfdecAsArray
 * @idx: index to insert the value to
 * @value: a #SwfdecAsValue
 * @flags: the #SwfdecAsVariableFlag flags to use
 *
 * Inserts @value to @array at given index using given @flags, making room for
 * it by moving elements to bigger indexes if necessary.
 **/
void
swfdec_as_array_insert_with_flags (SwfdecAsArray *array, gint32 idx,
    const SwfdecAsValue *value, SwfdecAsVariableFlag flags)
{
  gint32 length;
  SwfdecAsObject *object;

  g_return_if_fail (SWFDEC_IS_AS_ARRAY (array));
  g_return_if_fail (idx >= 0);
  g_return_if_fail (SWFDEC_IS_AS_VALUE (value));

  object = SWFDEC_AS_OBJECT (array);
  length = swfdec_as_array_length (object);

  if (idx < length)
    swfdec_as_array_move_range (object, idx, length - idx, idx + 1);
  swfdec_as_array_set_range_with_flags (object, idx, 1, value, flags);
}

/**
 * swfdec_as_array_remove:
 * @array: a #SwfdecAsArray
 * @idx: index of the value to remove
 *
 * Removes value at given index from the @array, elements with higher indexes
 * will be moved towards the start of the @array.
 **/
void
swfdec_as_array_remove (SwfdecAsArray *array, gint32 idx)
{
  gint32 length;
  SwfdecAsObject *object;

  g_return_if_fail (SWFDEC_IS_AS_ARRAY (array));
  g_return_if_fail (idx >= 0);

  object = SWFDEC_AS_OBJECT (array);
  length = swfdec_as_array_length (object);

  if (idx >= length)
    return;

  swfdec_as_array_move_range (object, idx + 1, length - (idx + 1), idx);
  swfdec_as_array_set_length (array, length - 1);
}

/**
 * swfdec_as_array_get_value:
 * @array: a #SwfdecAsArray
 * @idx: index of the value to get
 * @value: a pointer to #SwfdecAsValue that will be set
 *
 * Gets a value from given index, if the value doesn't exists an undefined
 * value is set.
 **/
void
swfdec_as_array_get_value (SwfdecAsArray *array, gint32 idx,
    SwfdecAsValue *value)
{
  const char *var;

  g_assert (SWFDEC_IS_AS_ARRAY (array));
  g_assert (idx >= 0);
  g_assert (value != NULL);

  var = swfdec_as_integer_to_string (SWFDEC_AS_OBJECT (array)->context, idx);
  swfdec_as_object_get_variable (SWFDEC_AS_OBJECT (array), var, value);
}

/**
 * swfdec_as_array_set_value:
 * @array: a #SwfdecAsArray
 * @idx: index of the value to set
 * @value: a pointer to #SwfdecAsValue
 *
 * Sets a @value to given index. The @array's length will be increased if
 * necessary.
 **/
void
swfdec_as_array_set_value (SwfdecAsArray *array, gint32 idx,
    SwfdecAsValue *value)
{
  const char *var;

  g_assert (SWFDEC_IS_AS_ARRAY (array));
  g_assert (idx >= 0);
  g_assert (SWFDEC_IS_AS_VALUE (value));

  var = swfdec_as_integer_to_string (SWFDEC_AS_OBJECT (array)->context, idx);
  swfdec_as_object_set_variable (SWFDEC_AS_OBJECT (array), var, value);
}

typedef struct {
  SwfdecAsObject		*object_to;
  gint32			offset;
  gint32			start_index;
  gint32			num;
} ForeachAppendArrayRangeData;

static gboolean
swfdec_as_array_foreach_append_array_range (SwfdecAsObject *object,
    const char *variable, SwfdecAsValue *value, guint flags, gpointer data)
{
  ForeachAppendArrayRangeData *fdata = data;
  gint32 idx;
  const char *var;

  idx = swfdec_as_array_to_index (variable);
  if (idx >= fdata->start_index && idx < fdata->start_index + fdata->num) {
    var = swfdec_as_integer_to_string (fdata->object_to->context,
	fdata->offset + (idx - fdata->start_index));
    swfdec_as_object_set_variable (fdata->object_to, var, value);
  }

  return TRUE;
}

static void
swfdec_as_array_append_array_range (SwfdecAsArray *array_to,
    SwfdecAsObject *object_from, gint32 start_index, gint32 num)
{
  ForeachAppendArrayRangeData fdata;

  g_return_if_fail (SWFDEC_IS_AS_ARRAY (array_to));
  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object_from));
  g_return_if_fail (start_index >= 0);
  g_return_if_fail (
      start_index + num <= swfdec_as_array_length (object_from));

  if (num == 0)
    return;

  fdata.object_to = SWFDEC_AS_OBJECT (array_to);
  fdata.offset = swfdec_as_array_length (SWFDEC_AS_OBJECT (array_to));
  fdata.start_index = start_index;
  fdata.num = num;

  swfdec_as_array_set_length_object (fdata.object_to,
      fdata.offset + fdata.num);
  swfdec_as_object_foreach (object_from,
      swfdec_as_array_foreach_append_array_range, &fdata);
}

static void
swfdec_as_array_append_array (SwfdecAsArray *array_to,
    SwfdecAsObject *object_from)
{
  swfdec_as_array_append_array_range (array_to, object_from, 0,
      swfdec_as_array_length (object_from));
}

/*
 * Class functions
 */

static void
swfdec_as_array_set (SwfdecAsObject *object, const char *variable,
    const SwfdecAsValue *val, guint flags)
{
  char *end;
  gboolean indexvar = TRUE;
  gint32 l = strtoul (variable, &end, 10);

  if (*end != 0 || l > G_MAXINT32)
    indexvar = FALSE;

  // if we changed to smaller length, destroy all values that are outside it
  if (!strcmp (variable, SWFDEC_AS_STR_length)) {
    gint32 length_old = swfdec_as_array_length (object);
    gint32 length_new = MAX (0,
	swfdec_as_value_to_integer (object->context, val));
    if (length_old > length_new) {
      swfdec_as_array_remove_range (SWFDEC_AS_ARRAY (object), length_new,
	  length_old - length_new);
    }
  }

  SWFDEC_AS_OBJECT_CLASS (swfdec_as_array_parent_class)->set (object, variable,
      val, flags);

  // if we added new value outside the current length, set a bigger length
  if (indexvar) {
    if (++l > swfdec_as_array_length_as_integer (object))
      swfdec_as_array_set_length_object (object, l);
  }
}

static void
swfdec_as_array_class_init (SwfdecAsArrayClass *klass)
{
  SwfdecAsObjectClass *asobject_class = SWFDEC_AS_OBJECT_CLASS (klass);

  asobject_class->set = swfdec_as_array_set;
}

static void
swfdec_as_array_init (SwfdecAsArray *array)
{
}

/*
 * The rest
 */

/**
 * swfdec_as_array_new:
 * @context: a #SwfdecAsContext
 *
 * Creates a new #SwfdecAsArray. 
 *
 * Returns: the new array or %NULL on OOM.
 **/
SwfdecAsObject *
swfdec_as_array_new (SwfdecAsContext *context)
{
  SwfdecAsObject *ret;
  SwfdecAsValue val;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);

  if (!swfdec_as_context_use_mem (context, sizeof (SwfdecAsArray)))
    return NULL;

  ret = g_object_new (SWFDEC_TYPE_AS_ARRAY, NULL);
  swfdec_as_object_add (ret, context, sizeof (SwfdecAsArray));
  swfdec_as_object_get_variable (context->global, SWFDEC_AS_STR_Array, &val);
  if (SWFDEC_AS_VALUE_IS_OBJECT (&val))
    swfdec_as_object_set_constructor (ret, SWFDEC_AS_VALUE_GET_OBJECT (&val));

  swfdec_as_array_set_length (SWFDEC_AS_ARRAY (ret), 0);

  return ret;
}

/*** AS CODE ***/

SWFDEC_AS_NATIVE (252, 7, swfdec_as_array_join)
void
swfdec_as_array_join (SwfdecAsContext *cx, SwfdecAsObject *object, guint argc,
    SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  int i, length;
  const char *var, *str, *sep;
  SwfdecAsValue val;

  if (object == NULL || SWFDEC_IS_MOVIE (object))
    return;

  if (argc > 0) {
    sep = swfdec_as_value_to_string (cx, &argv[0]);
  } else {
    sep = SWFDEC_AS_STR_COMMA;
  }

  length = swfdec_as_array_length (object);
  if (length > 0) {
    /* FIXME: implement this with the StringBuilder class */
    GString *string;
    swfdec_as_object_get_variable (object, SWFDEC_AS_STR_0, &val);
    str = swfdec_as_value_to_string (cx, &val);
    string = g_string_new (str);
    for (i = 1; i < length; i++) {
      var = swfdec_as_integer_to_string (cx, i);
      swfdec_as_object_get_variable (object, var, &val);
      var = swfdec_as_value_to_string (cx, &val);
      g_string_append (string, sep);
      g_string_append (string, var);
    }
    str = swfdec_as_context_give_string (cx, g_string_free (string, FALSE));
  } else {
    str = SWFDEC_AS_STR_EMPTY;
  }

  SWFDEC_AS_VALUE_SET_STRING (ret, str);
}

SWFDEC_AS_NATIVE (252, 9, swfdec_as_array_toString)
void
swfdec_as_array_toString (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  if (object == NULL || SWFDEC_IS_MOVIE (object))
    return;

  swfdec_as_array_join (cx, object, 0, NULL, ret);
}

SWFDEC_AS_NATIVE (252, 1, swfdec_as_array_do_push)
void
swfdec_as_array_do_push (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  if (object == NULL || SWFDEC_IS_MOVIE (object))
    return;

  // if 0 args, just return the length
  // manually set the length here to make the function work on non-Arrays
  if (argc > 0) {
    gint32 length = swfdec_as_array_length_as_integer (object);
    swfdec_as_array_append_internal (object, argc, argv);
    swfdec_as_array_set_length_object (object, length + argc);
  }

  SWFDEC_AS_VALUE_SET_INT (ret, swfdec_as_array_length_as_integer (object));
}

SWFDEC_AS_NATIVE (252, 2, swfdec_as_array_do_pop)
void
swfdec_as_array_do_pop (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  gint32 length;
  const char *var;

  if (object == NULL || SWFDEC_IS_MOVIE (object))
    return;

  // we allow negative indexes here, but not 0
  length = swfdec_as_array_length_as_integer (object);
  if (length == 0)
    return;

  var = swfdec_as_integer_to_string (object->context, length - 1);
  swfdec_as_object_get_variable (object, var, ret);

  // if Array, the length is reduced by one (which destroys the variable also)
  // else the length is not reduced at all, but the variable is still deleted
  if (SWFDEC_IS_AS_ARRAY (object)) {
    swfdec_as_array_set_length_object (object, length - 1);
  } else {
    swfdec_as_object_delete_variable (object, var);
  }
}

SWFDEC_AS_NATIVE (252, 5, swfdec_as_array_do_unshift)
void
swfdec_as_array_do_unshift (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  gint32 length;

  if (object == NULL || SWFDEC_IS_MOVIE (object))
    return;

  if (argc) {
    // don't allow negative length
    length = swfdec_as_array_length (object);
    swfdec_as_array_move_range (object, 0, length, argc);
    swfdec_as_array_set_range (object, 0, argc, argv);
    // if not Array, leave the length unchanged
    if (!SWFDEC_IS_AS_ARRAY (object))
      swfdec_as_array_set_length_object (object, length);
  }

  SWFDEC_AS_VALUE_SET_INT (ret, swfdec_as_array_length (object));
}

SWFDEC_AS_NATIVE (252, 4, swfdec_as_array_do_shift)
void
swfdec_as_array_do_shift (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  gint32 length;
  const char *var;

  if (object == NULL || SWFDEC_IS_MOVIE (object))
    return;

  // don't allow negative length
  length = swfdec_as_array_length (object);
  if (length <= 0)
    return;

  swfdec_as_object_get_variable (object, SWFDEC_AS_STR_0, ret);

  swfdec_as_array_move_range (object, 1, length - 1, 0);

  // if not Array, leave the length unchanged, and don't remove the element
  if (SWFDEC_IS_AS_ARRAY (object)) {
    swfdec_as_array_set_length_object (object, length - 1);
  } else {
    // we have to put the last element back, because we used move, not copy
    SwfdecAsValue val;
    if (length > 1) {
      var = swfdec_as_integer_to_string (object->context, length - 2);
      swfdec_as_object_get_variable (object, var, &val);
    } else {
      val = *ret;
    }
    var = swfdec_as_integer_to_string (object->context, length - 1);
    swfdec_as_object_set_variable (object, var, &val);
  }
}

static const char *
swfdec_as_array_foreach_reverse (SwfdecAsObject *object, const char *variable,
    SwfdecAsValue *value, guint flags, gpointer data)
{
  gint32 *length = data;
  gint32 idx;

  idx = swfdec_as_array_to_index (variable);
  if (idx == -1 || idx >= *length)
    return variable;

  return swfdec_as_integer_to_string (object->context, *length - 1 - idx);
}

SWFDEC_AS_NATIVE (252, 11, swfdec_as_array_reverse)
void
swfdec_as_array_reverse (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  gint32 length;

  if (object == NULL || SWFDEC_IS_MOVIE (object))
    return;

  length = swfdec_as_array_length (object);
  swfdec_as_object_foreach_rename (object, swfdec_as_array_foreach_reverse,
      &length);

  SWFDEC_AS_VALUE_SET_OBJECT (ret, object);
}

SWFDEC_AS_NATIVE (252, 3, swfdec_as_array_concat)
void
swfdec_as_array_concat (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  guint j;
  SwfdecAsObject *object_new;
  SwfdecAsArray *array_new;
  const char *var;

  if (object == NULL || SWFDEC_IS_MOVIE (object))
    return;

  object_new = swfdec_as_array_new (cx);
  if (object_new == NULL)
    return;
  array_new = SWFDEC_AS_ARRAY (object_new);

  swfdec_as_array_append_array (array_new, object);

  for (j = 0; j < argc; j++) {
    if (SWFDEC_AS_VALUE_IS_OBJECT (&argv[j]) &&
	SWFDEC_IS_AS_ARRAY (SWFDEC_AS_VALUE_GET_OBJECT (&argv[j])))
    {
      swfdec_as_array_append_array (array_new,
	  SWFDEC_AS_VALUE_GET_OBJECT (&argv[j]));
    }
    else
    {
      var = swfdec_as_integer_to_string (object->context,
	  swfdec_as_array_length (object_new));
      swfdec_as_object_set_variable (object_new, var, &argv[j]);
    }
  }

  SWFDEC_AS_VALUE_SET_OBJECT (ret, object_new);
}

SWFDEC_AS_NATIVE (252, 6, swfdec_as_array_slice)
void
swfdec_as_array_slice (SwfdecAsContext *cx, SwfdecAsObject *object, guint argc,
    SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  gint32 length, start_index, num;
  SwfdecAsArray *array_new;

  if (object == NULL || SWFDEC_IS_MOVIE (object))
    return;

  length = swfdec_as_array_length (object);

  if (argc > 0) {
    start_index = swfdec_as_value_to_integer (cx, &argv[0]);
    if (start_index < 0)
      start_index = length + start_index;
    start_index = CLAMP (start_index, 0, length);
  } else {
    start_index = 0;
  }

  if (argc > 1) {
    gint32 endIndex = swfdec_as_value_to_integer (cx, &argv[1]);
    if (endIndex < 0)
      endIndex = length + endIndex;
    endIndex = CLAMP (endIndex, start_index, length);
    num = endIndex - start_index;
  } else {
    num = length - start_index;
  }

  array_new = SWFDEC_AS_ARRAY (swfdec_as_array_new (cx));
  if (array_new == NULL)
    return;

  swfdec_as_array_append_array_range (array_new, object, start_index, num);

  SWFDEC_AS_VALUE_SET_OBJECT (ret, SWFDEC_AS_OBJECT (array_new));
}

SWFDEC_AS_NATIVE (252, 8, swfdec_as_array_splice)
void
swfdec_as_array_splice (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  gint32 length, start_index, num_remove, num_add;
  SwfdecAsArray *array_new;

  if (object == NULL || SWFDEC_IS_MOVIE (object) || argc == 0)
    return;

  length = swfdec_as_array_length (object);

  start_index = swfdec_as_value_to_integer (cx, &argv[0]);
  if (start_index < 0)
    start_index = length + start_index;
  start_index = CLAMP (start_index, 0, length);

  if (argc > 1) {
    num_remove = CLAMP (swfdec_as_value_to_integer (cx, &argv[1]), 0,
	length - start_index);
  } else {
    num_remove = length - start_index;
  }

  num_add = (argc > 2 ? argc - 2 : 0);

  array_new = SWFDEC_AS_ARRAY (swfdec_as_array_new (cx));
  if (array_new == NULL)
    return;

  swfdec_as_array_append_array_range (array_new, object, start_index,
      num_remove);
  swfdec_as_array_move_range (object, start_index + num_remove,
      length - (start_index + num_remove), start_index + num_add);
  if (num_remove > num_add)
    swfdec_as_array_set_length_object (object, length - (num_remove - num_add));
  if (argc > 2)
    swfdec_as_array_set_range (object, start_index, argc - 2, argv + 2);

  SWFDEC_AS_VALUE_SET_OBJECT (ret, SWFDEC_AS_OBJECT (array_new));
}

// Sorting

typedef enum {
  SORT_OPTION_CASEINSENSITIVE = 1 << 0,
  SORT_OPTION_DESCENDING = 1 << 1,
  SORT_OPTION_UNIQUESORT = 1 << 2,
  SORT_OPTION_RETURNINDEXEDARRAY = 1 << 3,
  SORT_OPTION_NUMERIC = 1 << 4
} SortOption;
#define MASK_SORT_OPTION ((1 << 5) - 1)

typedef struct {
  gint32		index_;
  SwfdecAsValue		value;
} SortEntry;

// inner function for swfdec_as_array_sort_compare
static int
swfdec_as_array_sort_compare_values (SwfdecAsContext *cx,
    const SwfdecAsValue *a, const SwfdecAsValue *b, SortOption options,
    SwfdecAsFunction *custom_function)
{
  int retval;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (cx), 0);
  g_return_val_if_fail (SWFDEC_IS_AS_VALUE (a), 0);
  g_return_val_if_fail (SWFDEC_IS_AS_VALUE (b), 0);
  g_return_val_if_fail (custom_function == NULL ||
      SWFDEC_IS_AS_FUNCTION (custom_function), 0);

  if (custom_function != NULL)
  {
    SwfdecAsValue argv[2] = { *a, *b };
    SwfdecAsValue ret;
    swfdec_as_function_call (custom_function, NULL, 2, argv, &ret);
    swfdec_as_context_run (custom_function->object.context);
    retval = swfdec_as_value_to_integer (cx, &ret);
  }
  else if (options & SORT_OPTION_NUMERIC &&
      (SWFDEC_AS_VALUE_IS_NUMBER (a) ||
       SWFDEC_AS_VALUE_IS_NUMBER (b)) &&
      !SWFDEC_AS_VALUE_IS_UNDEFINED (a) &&
      !SWFDEC_AS_VALUE_IS_UNDEFINED (b))
  {
    if (!SWFDEC_AS_VALUE_IS_NUMBER (a)) {
      retval = 1;
    } else if (!SWFDEC_AS_VALUE_IS_NUMBER (b)) {
      retval = -1;
    } else {
      double an = swfdec_as_value_to_number (cx, a);
      double bn = swfdec_as_value_to_number (cx, b);
      retval = (an < bn ? -1 : (an > bn ? 1 : 0));
    }
  }
  else if (options & SORT_OPTION_CASEINSENSITIVE)
  {
    retval = g_strcasecmp (swfdec_as_value_to_string (cx, a),
	swfdec_as_value_to_string (cx, b));
  }
  else
  {
    retval = strcmp (swfdec_as_value_to_string (cx, a),
	swfdec_as_value_to_string (cx, b));
  }

  if (options & SORT_OPTION_DESCENDING) {
    return -retval;
  } else {
    return retval;
  }
}

typedef struct {
  SwfdecAsContext *	context;
  const char **		fields;
  SortOption *		options;
  SwfdecAsFunction *	custom_function;
} SortCompareData;

static int
swfdec_as_array_sort_compare (gconstpointer a_ptr, gconstpointer b_ptr,
    gpointer user_data)
{
  const SwfdecAsValue *a = &((SortEntry *)a_ptr)->value;
  const SwfdecAsValue *b = &((SortEntry *)b_ptr)->value;
  const SortCompareData *data = user_data;
  int i, retval;
  SwfdecAsValue a_comp, b_comp;
  SwfdecAsObject *object;

  g_return_val_if_fail (SWFDEC_IS_AS_VALUE (a), 0);
  g_return_val_if_fail (SWFDEC_IS_AS_VALUE (b), 0);
  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (data->context), 0);
  g_return_val_if_fail (data->options != NULL, 0);
  g_return_val_if_fail (data->custom_function == NULL ||
      SWFDEC_IS_AS_FUNCTION (data->custom_function), 0);
  g_return_val_if_fail (data->fields == NULL || data->fields[0] != NULL, 0);

  if (data->fields == NULL)
    return swfdec_as_array_sort_compare_values (data->context, a, b,
	data->options[0], data->custom_function);

  i = 0;
  do {
    object = swfdec_as_value_to_object (data->context, a);
    if (object) {
      swfdec_as_object_get_variable (object, data->fields[i], &a_comp);
    } else {
      SWFDEC_AS_VALUE_SET_UNDEFINED (&a_comp);
    }

    object = swfdec_as_value_to_object (data->context, b);
    if (object) {
      swfdec_as_object_get_variable (object, data->fields[i], &b_comp);
    } else {
      SWFDEC_AS_VALUE_SET_UNDEFINED (&b_comp);
    }

    retval =
      swfdec_as_array_sort_compare_values (data->context, &a_comp, &b_comp,
	  data->options[i], data->custom_function);
  } while (retval == 0 && data->fields[++i] != NULL);

  return retval;
}

typedef struct {
  GArray *		array;
  gint32		length;
} SortCollectData;

static gboolean
swfdec_as_array_foreach_sort_collect (SwfdecAsObject *object,
    const char *variable, SwfdecAsValue *value, guint flags, gpointer data)
{
  SortCollectData *collect_data = data;
  SortEntry entry;
  gint32 index_;

  index_ = swfdec_as_array_to_index (variable);
  if (index_ == -1 || index_ >= collect_data->length)
    return TRUE;

  if (SWFDEC_AS_VALUE_IS_UNDEFINED (value))
    return TRUE;

  entry.index_ = index_;
  entry.value = *value;

  g_array_append_val (collect_data->array, entry);

  return TRUE;
}

static void
swfdec_as_array_do_sort (SwfdecAsContext *cx, SwfdecAsObject *object,
    SortOption *options, SwfdecAsFunction *custom_function, const char **fields,
    SwfdecAsValue *ret)
{
  GArray *array;
  SortCollectData collect_data;
  SortCompareData compare_data;
  gint32 i, offset, length;
  const char *var;

  g_return_if_fail (SWFDEC_IS_AS_CONTEXT (cx));
  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail (options != NULL);
  g_return_if_fail (custom_function == NULL ||
      SWFDEC_IS_AS_FUNCTION (custom_function));
  g_return_if_fail (fields == NULL || fields[0] != NULL);

  length = swfdec_as_array_length (object);
  array = g_array_new (TRUE, TRUE, sizeof (SortEntry));

  // collect values and indexes to the array
  collect_data.length = length;
  collect_data.array = array;

  swfdec_as_object_foreach (object, swfdec_as_array_foreach_sort_collect,
	&collect_data);

  // if there are missing properties, add one to the array with special index
  if ((gint32)array->len < length) {
    SortEntry entry;

    entry.index_ = -1;
    SWFDEC_AS_VALUE_SET_UNDEFINED (&entry.value);

    g_array_append_val (array, entry);
  }

  // sort the array
  compare_data.context = cx;
  compare_data.fields = fields;
  compare_data.options = (SortOption *)options;
  compare_data.custom_function = custom_function;

  g_array_sort_with_data (array, swfdec_as_array_sort_compare, &compare_data);

  // set the values that have new indexes
  offset = 0;
  for (i = 0; i < (gint32)array->len; i++) {
    SortEntry *entry = &g_array_index (array, SortEntry, i);

    if (entry->index_ == i + offset)
      continue;

    var = swfdec_as_integer_to_string (cx, i + offset);
    swfdec_as_object_set_variable (object, var, &entry->value);

    // special element for missing properties, add all missing properties here
    if (entry->index_ == -1) {
      g_assert (offset == 0);
      for (offset = 0; offset < length - (gint32)array->len; offset++) {
	var = swfdec_as_integer_to_string (cx, i + offset + 1);
	swfdec_as_object_set_variable (object, var, &entry->value);
      }
    }
  }

  g_array_free (array, TRUE);

  SWFDEC_AS_VALUE_SET_OBJECT (ret, object);
}

SWFDEC_AS_NATIVE (252, 10, swfdec_as_array_sort)
void
swfdec_as_array_sort (SwfdecAsContext *cx, SwfdecAsObject *object, guint argc,
    SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  guint pos;
  SortOption options;
  SwfdecAsFunction *custom_function;

  pos = 0;

  if (argc > pos && !SWFDEC_AS_VALUE_IS_NUMBER (&argv[pos])) {
    SwfdecAsFunction *fun;
    if (!SWFDEC_AS_VALUE_IS_OBJECT (&argv[pos]) ||
	!SWFDEC_IS_AS_FUNCTION (
	  fun = (SwfdecAsFunction *) SWFDEC_AS_VALUE_GET_OBJECT (&argv[pos])))
	return;
    custom_function = fun;
    pos++;
  } else {
    custom_function = NULL;
  }

  if (argc > pos) {
    options = swfdec_as_value_to_integer (cx, &argv[pos]) & MASK_SORT_OPTION;
  } else {
    options = 0;
  }

  swfdec_as_array_do_sort (cx, object, &options, custom_function, NULL, ret);
}

SWFDEC_AS_NATIVE (252, 12, swfdec_as_array_sortOn)
void
swfdec_as_array_sortOn (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  const char **fields;
  SortOption options;

  if (object == NULL || SWFDEC_IS_MOVIE (object))
    return;

  if (argc < 1)
    return;

  if (SWFDEC_AS_VALUE_IS_OBJECT (&argv[0])) {
    gint32 length, i;
    SwfdecAsValue val;
    SwfdecAsObject *array;

    array = SWFDEC_AS_VALUE_GET_OBJECT (&argv[0]);
    if (!SWFDEC_IS_AS_ARRAY (array)) {
      SWFDEC_AS_VALUE_SET_OBJECT (ret, object);
      return;
    }

    length = swfdec_as_array_get_length (SWFDEC_AS_ARRAY (array));
    if (length <= 0) {
      SWFDEC_AS_VALUE_SET_OBJECT (ret, object);
      return;
    }

    fields = g_malloc (sizeof (const char *) * (length + 1));
    for (i = 0; i < length; i++) {
      swfdec_as_array_get_value (SWFDEC_AS_ARRAY (array), i, &val);
      if (SWFDEC_AS_VALUE_IS_OBJECT (&val) &&
	  SWFDEC_IS_AS_STRING (SWFDEC_AS_VALUE_GET_OBJECT (&val))) {
	fields[i] =
	  SWFDEC_AS_STRING (SWFDEC_AS_VALUE_GET_OBJECT (&val))->string;
      } else {
	fields[i] = swfdec_as_value_to_string (cx, &val);
      }
    }

    fields[i] = NULL;
  } else {
    fields = g_malloc (sizeof (const char *) * 2);
    fields[0] = swfdec_as_value_to_string (cx, &argv[0]);
    fields[1] = NULL;
  }

  if (argc > 1) {
    options = swfdec_as_value_to_integer (cx, &argv[1]) & MASK_SORT_OPTION;
  } else {
    options = 0;
  }

  swfdec_as_array_do_sort (cx, object, &options, NULL, fields, ret);

  g_free (fields);
}

// Constructor

SWFDEC_AS_CONSTRUCTOR (252, 0, swfdec_as_array_construct, swfdec_as_array_get_type)
void
swfdec_as_array_construct (SwfdecAsContext *cx, SwfdecAsObject *object,
    guint argc, SwfdecAsValue *argv, SwfdecAsValue *ret)
{
  SwfdecAsArray *array;

  if (!cx->frame->construct) {
    SwfdecAsValue val;
    if (!swfdec_as_context_use_mem (cx, sizeof (SwfdecAsArray)))
      return;
    object = g_object_new (SWFDEC_TYPE_AS_ARRAY, NULL);
    swfdec_as_object_add (object, cx, sizeof (SwfdecAsArray));
    swfdec_as_object_get_variable (cx->global, SWFDEC_AS_STR_Array, &val);
    if (SWFDEC_AS_VALUE_IS_OBJECT (&val)) {
      swfdec_as_object_set_constructor (object,
	  SWFDEC_AS_VALUE_GET_OBJECT (&val));
    } else {
      SWFDEC_INFO ("\"Array\" is not an object");
    }
  }

  array = SWFDEC_AS_ARRAY (object);
  if (argc == 1 && SWFDEC_AS_VALUE_IS_NUMBER (&argv[0])) {
    int l = swfdec_as_value_to_integer (cx, &argv[0]);
    swfdec_as_array_set_length (array, l < 0 ? 0 : l);
  } else if (argc > 0) {
    swfdec_as_array_append (array, argc, argv);
  } else {
    swfdec_as_array_set_length (array, 0);
  }

  SWFDEC_AS_VALUE_SET_OBJECT (ret, object);
}
