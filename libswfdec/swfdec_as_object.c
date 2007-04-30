/* Swfdec
 * Copyright (C) 2007 Benjamin Otte <otte@gnome.org>
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

#include "swfdec_as_object.h"
#include "swfdec_as_context.h"
#include "swfdec_as_frame.h"
#include "swfdec_as_function.h"
#include "swfdec_debug.h"


typedef struct _SwfdecAsVariable SwfdecAsVariable;
struct _SwfdecAsVariable {
  guint			flags;		/* SwfdecAsVariableFlag values */
  SwfdecAsValue     	value;		/* value of property */
};

G_DEFINE_TYPE (SwfdecAsObject, swfdec_as_object, G_TYPE_OBJECT)

static void
swfdec_as_object_dispose (GObject *gobject)
{
  SwfdecAsObject *object = SWFDEC_AS_OBJECT (gobject);

  g_assert (!SWFDEC_AS_OBJECT_HAS_CONTEXT (object));

  G_OBJECT_CLASS (swfdec_as_object_parent_class)->dispose (gobject);
}

static void
swfdec_as_object_mark_property (gpointer key, gpointer value, gpointer unused)
{
  SwfdecAsVariable *var = value;

  swfdec_as_string_mark (key);
  swfdec_as_value_mark (&var->value);
}

static void
swfdec_as_object_do_mark (SwfdecAsObject *object)
{
  swfdec_as_object_mark (object->prototype);
  g_hash_table_foreach (object->properties, swfdec_as_object_mark_property, NULL);
}

static void
swfdec_as_object_do_add (SwfdecAsObject *object)
{
}

static gboolean
swfdec_as_object_do_get (SwfdecAsObject *object, const char *variable, 
    SwfdecAsValue *val, guint *flags)
{
  SwfdecAsVariable *var = g_hash_table_lookup (object->properties, variable);

  if (var) {
    *val = var->value;
    *flags = var->flags;
    return TRUE;
  }
  return FALSE;
}

static void
swfdec_as_object_do_set (SwfdecAsObject *object, const char *variable, 
    const SwfdecAsValue *val)
{
  SwfdecAsVariable *var;

  if (variable == SWFDEC_AS_STR___proto__) {
    if (SWFDEC_AS_VALUE_IS_OBJECT (val)) {
      object->prototype = SWFDEC_AS_VALUE_GET_OBJECT (val);
    } else {
      object->prototype = NULL;
    }
  }

  var = g_hash_table_lookup (object->properties, variable);
  if (var == NULL) {
    if (!swfdec_as_context_use_mem (object->context, sizeof (SwfdecAsVariable)))
      return;
    var = g_slice_new0 (SwfdecAsVariable);
    g_hash_table_insert (object->properties, (gpointer) variable, var);
  }
  var->value = *val;
}

static void
swfdec_as_object_do_set_flags (SwfdecAsObject *object, const char *variable, guint flags, guint mask)
{
  SwfdecAsVariable *var = g_hash_table_lookup (object->properties, variable);

  if (var)
    var->flags = (var->flags & ~mask) | flags;
}

static void
swfdec_as_object_free_property (gpointer key, gpointer value, gpointer data)
{
  SwfdecAsObject *object = data;

  swfdec_as_context_unuse_mem (object->context, sizeof (SwfdecAsVariable));
  g_slice_free (SwfdecAsVariable, value);
}

static void
swfdec_as_object_do_delete (SwfdecAsObject *object, const char *variable)
{
  SwfdecAsVariable *var;

  var = g_hash_table_lookup (object->properties, variable);
  g_assert (var);
  swfdec_as_object_free_property (NULL, var, object);
}

typedef struct {
  SwfdecAsObject *		object;
  SwfdecAsVariableForeach	func;
  gpointer			data;
  gboolean			retval;
} ForeachData;

static void
swfdec_as_object_hash_foreach (gpointer key, gpointer value, gpointer data)
{
  ForeachData *fdata = data;
  SwfdecAsVariable *var = value;

  if (!fdata->retval)
    return;

  fdata->retval = fdata->func (fdata->object, key, &var->value, var->flags, fdata->data);
}

/* FIXME: does not do Adobe Flash's order for Enumerate actions */
static gboolean
swfdec_as_object_do_foreach (SwfdecAsObject *object, SwfdecAsVariableForeach func, gpointer data)
{
  ForeachData fdata = { object, func, data, TRUE };

  g_hash_table_foreach (object->properties, swfdec_as_object_hash_foreach, &fdata);
  return fdata.retval;
}

static void
swfdec_as_object_class_init (SwfdecAsObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_as_object_dispose;

  klass->mark = swfdec_as_object_do_mark;
  klass->add = swfdec_as_object_do_add;
  klass->get = swfdec_as_object_do_get;
  klass->set = swfdec_as_object_do_set;
  klass->set_flags = swfdec_as_object_do_set_flags;
  klass->delete = swfdec_as_object_do_delete;
  klass->foreach = swfdec_as_object_do_foreach;
}

static void
swfdec_as_object_init (SwfdecAsObject *object)
{
}

/**
 * swfdec_as_object_new:
 * @context: a #SwfdecAsContext
 *
 * Allocates a new Object. This does the same as the Actionscript code 
 * "new Object()".
 * <warning>This function may run the garbage collector.</warning>
 *
 * Returns: the new object or NULL on out of memory.
 **/
SwfdecAsObject *
swfdec_as_object_new (SwfdecAsContext *context)
{
  SwfdecAsObject *object;

  g_return_val_if_fail (SWFDEC_IS_AS_CONTEXT (context), NULL);
  
  if (!swfdec_as_context_use_mem (context, sizeof (SwfdecAsObject)))
    return NULL;
  object = g_object_new (SWFDEC_TYPE_AS_OBJECT, NULL);
  swfdec_as_object_add (object, context, sizeof (SwfdecAsObject));
  if (context->Object) {
    SwfdecAsValue val;
    swfdec_as_object_root (object);
    g_assert (context->Object_prototype);
    SWFDEC_AS_VALUE_SET_OBJECT (&val, context->Object_prototype);
    swfdec_as_object_set_variable (object, SWFDEC_AS_STR___proto__, &val);
    SWFDEC_AS_VALUE_SET_OBJECT (&val, context->Object);
    swfdec_as_object_set_variable (object, SWFDEC_AS_STR_constructor, &val);
    swfdec_as_object_unroot (object);
  }
  return object;
}

/**
 * swfdec_as_object_add:
 * @object: #SwfdecAsObject to make garbage-collected
 * @context: #SwfdecAsContext that should manage the object
 * @size: size the object currently uses
 *
 * Takes over the reference to @object for the garbage collector of @context. 
 * The object may not already be part of a different context. The given @size 
 * must have been allocated before with swfdec_as_context_use_mem ().
 * Note that after swfdec_as_object_add() the garbage collector might hold the
 * only reference to @object.
 **/
void
swfdec_as_object_add (SwfdecAsObject *object, SwfdecAsContext *context, gsize size)
{
  SwfdecAsObjectClass *klass;

  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail (SWFDEC_IS_AS_CONTEXT (context));
  g_return_if_fail (!SWFDEC_AS_OBJECT_HAS_CONTEXT (object));

  object->context = context;
  object->size = size;
  g_hash_table_insert (context->objects, object, object);
  object->properties = g_hash_table_new (g_direct_hash, g_direct_equal);
  klass = SWFDEC_AS_OBJECT_GET_CLASS (object);
  g_return_if_fail (klass->add);
  klass->add (object);
}

void
swfdec_as_object_collect (SwfdecAsObject *object)
{
  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail (SWFDEC_AS_OBJECT_HAS_CONTEXT (object));

  g_hash_table_foreach (object->properties, swfdec_as_object_free_property, object);
  g_hash_table_destroy (object->properties);
  object->properties = NULL;
  swfdec_as_context_unuse_mem (object->context, object->size);
  g_object_unref (object);
}

void
swfdec_as_object_root (SwfdecAsObject *object)
{
  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail ((object->flags & SWFDEC_AS_GC_ROOT) == 0);

  object->flags |= SWFDEC_AS_GC_ROOT;
}

void
swfdec_as_object_unroot (SwfdecAsObject *object)
{
  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail ((object->flags & SWFDEC_AS_GC_ROOT) != 0);

  object->flags &= ~SWFDEC_AS_GC_ROOT;
}

void
swfdec_as_object_set_variable (SwfdecAsObject *object,
    const char *variable, const SwfdecAsValue *value)
{
  SwfdecAsObjectClass *klass;

  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail (variable != NULL);
  g_return_if_fail (SWFDEC_IS_AS_VALUE (value));

  klass = SWFDEC_AS_OBJECT_GET_CLASS (object);
  klass->set (object, variable, value);
}

static inline gboolean
swfdec_as_object_lookup (SwfdecAsObject *object, const char *variable, 
    SwfdecAsValue *value, guint *flags)
{
  SwfdecAsObjectClass *klass;
  SwfdecAsValue tmp_val;
  guint tmp_flags;

  if (value == NULL)
    value = &tmp_val;
  if (flags == NULL)
    flags = &tmp_flags;

  klass = SWFDEC_AS_OBJECT_GET_CLASS (object);
  return klass->get (object, variable, value, flags);
}

void
swfdec_as_object_get_variable (SwfdecAsObject *object, 
    const char *variable, SwfdecAsValue *value)
{
  guint i;

  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail (variable != NULL);
  g_return_if_fail (value != NULL);

  for (i = 0; i < 256 && object != NULL; i++) {
    if (swfdec_as_object_lookup (object, variable, value, NULL))
      return;
    object = object->prototype;
  }
  if (i == 256) {
    swfdec_as_context_abort (object->context, "Prototype recursion limit exceeded");
    return;
  }
  //SWFDEC_WARNING ("no such variable %s", variable);
  SWFDEC_AS_VALUE_SET_UNDEFINED (value);
}

void
swfdec_as_object_delete_variable (SwfdecAsObject *object, 
    const char *variable)
{
  SwfdecAsObjectClass *klass;
  guint i, flags;

  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail (variable != NULL);

  for (i = 0; i < 256 && object != NULL; i++) {
    if (!swfdec_as_object_lookup (object, variable, NULL, &flags)) {
      object = object->prototype;
      continue;
    }
    if (!(flags & SWFDEC_AS_VARIABLE_PERMANENT)) {
      klass = SWFDEC_AS_OBJECT_GET_CLASS (object);
      klass->delete (object, variable);
    }
    return;
  }
  if (i == 256) {
    swfdec_as_context_abort (object->context, "Prototype recursion limit exceeded");
    return;
  }
}

SwfdecAsObject *
swfdec_as_object_find_variable (SwfdecAsObject *object,
    const char *variable)
{
  guint i;

  g_return_val_if_fail (SWFDEC_IS_AS_OBJECT (object), NULL);
  g_return_val_if_fail (variable != NULL, NULL);

  for (i = 0; i < 256 && object != NULL; i++) {
    if (swfdec_as_object_lookup (object, variable, NULL, NULL))
      return object;
    object = object->prototype;
  }
  if (i == 256) {
    swfdec_as_context_abort (object->context, "Prototype recursion limit exceeded");
    return NULL;
  }
  return NULL;
}

/**
 * swfdec_as_object_set_variable_flags:
 * @object: a #SwfdecAsObject
 * @variable: the variable to modify
 * @flags: flags to set
 *
 * Sets the given flags for the given variable.
 **/
void
swfdec_as_object_set_variable_flags (SwfdecAsObject *object, 
    const char *variable, SwfdecAsVariableFlag flags)
{
  SwfdecAsObjectClass *klass;

  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail (variable != NULL);

  klass = SWFDEC_AS_OBJECT_GET_CLASS (object);
  klass->set_flags (object, variable, flags, flags);
}

/**
 * swfdec_as_object_unset_variable_flags:
 * @object: a #SwfdecAsObject
 * @variable: the variable to modify
 * @flags: flags to unset
 *
 * Unsets the given flags for the given variable. The variable must exist in 
 * @object.
 **/
void
swfdec_as_object_unset_variable_flags (SwfdecAsObject *object,
    const char *variable, SwfdecAsVariableFlag flags)
{
  SwfdecAsObjectClass *klass;

  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail (variable != NULL);

  klass = SWFDEC_AS_OBJECT_GET_CLASS (object);
  klass->set_flags (object, variable, 0, flags);
}

gboolean
swfdec_as_object_foreach (SwfdecAsObject *object, SwfdecAsVariableForeach func,
    gpointer data)
{
  SwfdecAsObjectClass *klass;

  g_return_val_if_fail (SWFDEC_IS_AS_OBJECT (object), FALSE);
  g_return_val_if_fail (func != NULL, FALSE);

  klass = SWFDEC_AS_OBJECT_GET_CLASS (object);
  g_return_val_if_fail (klass->foreach != NULL, FALSE);
  return klass->foreach (object, func, data);
}

/*** SIMPLIFICATIONS ***/

static void
swfdec_as_object_do_nothing (SwfdecAsObject *object, guint argc, SwfdecAsValue *argv, SwfdecAsValue *retval)
{
}

/**
 * swfdec_as_object_add_function:
 * @object: a #SwfdecAsObject
 * @name: name of the function. The string does not have to be 
 *        garbage-collected.
 * @type: the required type of the this Object to make this function execute.
 *        May be 0 to accept any type.
 * @native: a native function or %NULL to just not do anything
 * @min_args: minimum number of arguments to pass to @native
 *
 * Adds @native as a variable named @name to @object. The newly added variable
 * will not be enumerated.
 *
 * Returns: the newly created #SwfdecAsFunction or %NULL on error.
 **/
SwfdecAsFunction *
swfdec_as_object_add_function (SwfdecAsObject *object, const char *name, GType type,
    SwfdecAsNative native, guint min_args)
{
  SwfdecAsFunction *function;
  SwfdecAsValue val;

  g_return_val_if_fail (SWFDEC_IS_AS_OBJECT (object), NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (type == 0 || g_type_is_a (type, SWFDEC_TYPE_AS_OBJECT), NULL);

  if (!native)
    native = swfdec_as_object_do_nothing;
  function = swfdec_as_function_new_native (object->context, name, native, min_args);
  if (function == NULL)
    return NULL;
  if (type != 0)
    swfdec_as_function_set_object_type (function, type);
  name = swfdec_as_context_get_string (object->context, name);
  SWFDEC_AS_VALUE_SET_OBJECT (&val, SWFDEC_AS_OBJECT (function));
  /* FIXME: I'd like to make sure no such property exists yet */
  swfdec_as_object_set_variable (object, name, &val);
  swfdec_as_object_set_variable_flags (object, name, SWFDEC_AS_VARIABLE_DONT_ENUM);
  return function;
}

/**
 * swfdec_as_object_run:
 * @object: a #SwfdecAsObject
 * @script: script to execute
 *
 * Executes the given @script with @object as this pointer.
 **/
void
swfdec_as_object_run (SwfdecAsObject *object, SwfdecScript *script)
{
  SwfdecAsFrame *frame;

  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail (SWFDEC_AS_OBJECT_HAS_CONTEXT (object));
  g_return_if_fail (script != NULL);

  frame = swfdec_as_frame_new (object, script);
  if (frame == NULL)
    return;
  swfdec_as_frame_preload (frame);
  swfdec_as_context_run (object->context);
}

/**
 * swfdec_as_object_call:
 * @object: a #SwfdecAsObject
 * @name: garbage-collected string naming the function to call. 
 * @argc: number of arguments to provide to function
 * @argv: arguments or %NULL when @argc is 0
 * @return_value: location to take the return value of the call or %NULL to 
 *                ignore the return value.
 *
 * Calls the function named @name on the given object. This function is 
 * essentially equal to the folloeing Actionscript code: 
 * <informalexample><programlisting>
 * @return_value = @object.@name (@argv[0], ..., @argv[argc-1]);
 * </programlisting></informalexample>
 **/
void
swfdec_as_object_call (SwfdecAsObject *object, const char *name, guint argc, 
    SwfdecAsValue *argv, SwfdecAsValue *return_value)
{
  static SwfdecAsValue tmp; /* ignored */
  SwfdecAsFunction *fun;

  g_return_if_fail (SWFDEC_IS_AS_OBJECT (object));
  g_return_if_fail (name != NULL);
  g_return_if_fail (argc == 0 || argv != NULL);

  if (return_value)
    SWFDEC_AS_VALUE_SET_UNDEFINED (return_value);
  swfdec_as_object_get_variable (object, name, &tmp);
  if (!SWFDEC_AS_VALUE_IS_OBJECT (&tmp))
    return;
  fun = (SwfdecAsFunction *) SWFDEC_AS_VALUE_GET_OBJECT (&tmp);
  if (!SWFDEC_IS_AS_FUNCTION (fun))
    return;
  swfdec_as_function_call (fun, object, argc, argv, return_value ? return_value : &tmp);
  swfdec_as_context_run (object->context);
}

gboolean
swfdec_as_object_has_function (SwfdecAsObject *object, const char *name)
{
  SwfdecAsValue val;

  g_return_val_if_fail (SWFDEC_IS_AS_OBJECT (object), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  swfdec_as_object_get_variable (object, name, &val);
  if (!SWFDEC_AS_VALUE_IS_OBJECT (&val))
    return FALSE;
  return SWFDEC_IS_AS_FUNCTION (SWFDEC_AS_VALUE_GET_OBJECT (&val));
}

/**
 * swfdec_as_object_create:
 * @construct: constructor
 * @n_args: number of arguments
 * @args: arguments to pass to constructor
 *
 * Creates a new object for the given constructor and pushes the constructor on
 * top of the stack. To actually run the constructor, you need to call 
 * swfdec_as_context_run() yourself.
 *
 * Returns: The newly created object. On OOM, the prototype will be returned, 
 *          so the returned object will always be valid.
 **/
SwfdecAsObject *
swfdec_as_object_create (SwfdecAsFunction *construct, guint n_args, SwfdecAsValue *args)
{
  static SwfdecAsValue val; /* ignored */
  SwfdecAsObject *new, *proto;
  SwfdecAsContext *context;

  g_return_val_if_fail (SWFDEC_IS_AS_FUNCTION (construct), NULL);
  g_return_val_if_fail (n_args == 0 || args != NULL, NULL);

  context = SWFDEC_AS_OBJECT (construct)->context;
  swfdec_as_object_get_variable (SWFDEC_AS_OBJECT (construct), SWFDEC_AS_STR_prototype, &val);
  if (SWFDEC_AS_VALUE_IS_OBJECT (&val)) {
    proto = SWFDEC_AS_VALUE_GET_OBJECT (&val);
  } else {
    SWFDEC_WARNING ("constructor has no prototype, using Object.prototype");
    proto = context->Object_prototype;
  }
  if (!swfdec_as_context_use_mem (context, proto->size))
    return proto;
  new = g_object_new (G_OBJECT_TYPE (proto), NULL);
  swfdec_as_object_add (new, context, proto->size);
  swfdec_as_object_root (new);
  SWFDEC_AS_VALUE_SET_OBJECT (&val, proto);
  swfdec_as_object_set_variable (new, SWFDEC_AS_STR___proto__, &val);
  SWFDEC_AS_VALUE_SET_OBJECT (&val, SWFDEC_AS_OBJECT (construct));
  swfdec_as_object_set_variable (new, SWFDEC_AS_STR_constructor, &val);
  swfdec_as_function_call (construct, new, n_args, args, &val);
  context->frame->construct = TRUE;
  swfdec_as_object_unroot (new);
  return new;
}

/*** AS CODE ***/

static void
swfdec_as_object_hasOwnProperty (SwfdecAsObject *object, guint argc, SwfdecAsValue *argv, SwfdecAsValue *retval)
{
  const char *name;
  guint flags;

  name = swfdec_as_value_to_string (object->context, &argv[0]);
  
  if (swfdec_as_object_lookup (object, name, NULL, &flags) &&
      (flags & SWFDEC_AS_VARIABLE_NATIVE) == 0)
    SWFDEC_AS_VALUE_SET_BOOLEAN (retval, TRUE);
  else
    SWFDEC_AS_VALUE_SET_BOOLEAN (retval, FALSE);
}

static void
swfdec_as_object_valueOf (SwfdecAsObject *object, guint argc, SwfdecAsValue *argv, SwfdecAsValue *retval)
{
  SWFDEC_AS_VALUE_SET_OBJECT (retval, object);
}

static void
swfdec_as_object_toString (SwfdecAsObject *object, guint argc, SwfdecAsValue *argv, SwfdecAsValue *retval)
{
  SWFDEC_AS_VALUE_SET_STRING (retval, SWFDEC_AS_STR_object_Object);
}

void
swfdec_as_object_init_context (SwfdecAsContext *context, guint version)
{
  SwfdecAsValue val;
  SwfdecAsObject *object, *proto;

  object = SWFDEC_AS_OBJECT (swfdec_as_object_add_function (context->global, 
      SWFDEC_AS_STR_Object, 0, NULL, 0));
  if (!object)
    return;
  proto = swfdec_as_object_new (context);
  if (!proto)
    return;
  context->Object = object;
  context->Object_prototype = proto;
  SWFDEC_AS_VALUE_SET_OBJECT (&val, proto);
  if (context->Function_prototype) {
    /* first, finish the function prototype */
    swfdec_as_object_set_variable (context->Function_prototype, SWFDEC_AS_STR___proto__, &val);
  }
  /* now, set our own */
  swfdec_as_object_set_variable (object, SWFDEC_AS_STR_prototype, &val);

  swfdec_as_object_add_function (proto, SWFDEC_AS_STR_hasOwnProperty, 0, swfdec_as_object_hasOwnProperty, 1);
  swfdec_as_object_add_function (proto, SWFDEC_AS_STR_valueOf, 0, swfdec_as_object_valueOf, 0);
  swfdec_as_object_add_function (proto, SWFDEC_AS_STR_toString, 0, swfdec_as_object_toString, 0);
}

void
swfdec_as_variable_set (SwfdecAsVariable *var, const SwfdecAsValue *value)
{
  g_return_if_fail (var != NULL);
  g_return_if_fail (SWFDEC_IS_AS_VALUE (value));

  if (var->flags & SWFDEC_AS_VARIABLE_READONLY)
    return;
  var->value = *value;
}

