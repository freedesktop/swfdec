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

#include "swfdec_abc_traits.h"
#include "swfdec_as_context.h"
#include "swfdec_debug.h"

G_DEFINE_TYPE (SwfdecAbcTraits, swfdec_abc_traits, SWFDEC_TYPE_GC_OBJECT)

static void
swfdec_abc_traits_dispose (GObject *object)
{
  SwfdecAbcTraits *traits = SWFDEC_ABC_TRAITS (object);
  SwfdecAsContext *context = swfdec_gc_object_get_context (traits);

  if (traits->n_traits) {
    swfdec_as_context_unuse_mem (context, traits->n_traits * sizeof (SwfdecAbcTrait));
    g_free (traits->traits);
  }

  G_OBJECT_CLASS (swfdec_abc_traits_parent_class)->dispose (object);
}

static void
swfdec_abc_traits_mark (SwfdecGcObject *object)
{
  SwfdecAbcTraits *traits = SWFDEC_ABC_TRAITS (object);

  swfdec_gc_object_mark (traits->ns);
  swfdec_as_string_mark (traits->name);
  if (traits->base)
    swfdec_gc_object_mark (traits->base);
  if (traits->construct)
    swfdec_gc_object_mark (traits->construct);
  if (traits->protected_ns)
    swfdec_gc_object_mark (traits->protected_ns);

  SWFDEC_GC_OBJECT_CLASS (swfdec_abc_traits_parent_class)->mark (object);
}

static void
swfdec_abc_traits_class_init (SwfdecAbcTraitsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecGcObjectClass *gc_class = SWFDEC_GC_OBJECT_CLASS (klass);

  object_class->dispose = swfdec_abc_traits_dispose;

  gc_class->mark = swfdec_abc_traits_mark;
}

static void
swfdec_abc_traits_init (SwfdecAbcTraits *date)
{
}

