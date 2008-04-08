/* Vivified
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

#include "vivi_code_get.h"
#include "vivi_code_constant.h"
#include "vivi_code_printer.h"
#include "vivi_code_compiler.h"

G_DEFINE_TYPE (ViviCodeGet, vivi_code_get, VIVI_TYPE_CODE_VALUE)

static void
vivi_code_get_dispose (GObject *object)
{
  ViviCodeGet *get = VIVI_CODE_GET (object);

  if (get->from)
    g_object_unref (get->from);
  g_object_unref (get->name);

  G_OBJECT_CLASS (vivi_code_get_parent_class)->dispose (object);
}

static void
vivi_code_get_print (ViviCodeToken *token, ViviCodePrinter*printer)
{
  ViviCodeGet *get = VIVI_CODE_GET (token);
  char *varname;

  if (VIVI_IS_CODE_CONSTANT (get->name))
    varname = vivi_code_constant_get_variable_name (VIVI_CODE_CONSTANT (get->name));
  else
    varname = NULL;

  if (get->from) {
    vivi_code_printer_print_value (printer, get->from, VIVI_PRECEDENCE_MEMBER);
    if (varname) {
      vivi_code_printer_print (printer, ".");
      vivi_code_printer_print (printer, varname);
    } else {
      vivi_code_printer_print (printer, "[");
      vivi_code_printer_print_value (printer, get->name, VIVI_PRECEDENCE_MIN);
      vivi_code_printer_print (printer, "]");
    }
  } else {
    if (varname) {
      vivi_code_printer_print (printer, varname);
    } else {
      vivi_code_printer_print (printer, "eval (");
      vivi_code_printer_print_value (printer, get->name, VIVI_PRECEDENCE_MIN);
      vivi_code_printer_print (printer, ")");
    }
  }
  g_free (varname);
}

static void
vivi_code_get_compile (ViviCodeToken *token, ViviCodeCompiler *compiler)
{
  ViviCodeGet *get = VIVI_CODE_GET (token);

  if (get->from) {
    vivi_code_compiler_add_action (compiler, SWFDEC_AS_ACTION_GET_MEMBER);
  } else {
    vivi_code_compiler_add_action (compiler, SWFDEC_AS_ACTION_GET_VARIABLE);
  }

  vivi_code_compiler_compile_value (compiler, get->name);
  if (get->from)
    vivi_code_compiler_compile_value (compiler, get->from);

  vivi_code_compiler_end_action (compiler);
}

static gboolean
vivi_code_get_is_constant (ViviCodeValue *value)
{
  return FALSE;
}

static void
vivi_code_get_class_init (ViviCodeGetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ViviCodeTokenClass *token_class = VIVI_CODE_TOKEN_CLASS (klass);
  ViviCodeValueClass *value_class = VIVI_CODE_VALUE_CLASS (klass);

  object_class->dispose = vivi_code_get_dispose;

  token_class->print = vivi_code_get_print;
  token_class->compile = vivi_code_get_compile;

  value_class->is_constant = vivi_code_get_is_constant;
}

static void
vivi_code_get_init (ViviCodeGet *get)
{
  ViviCodeValue *value = VIVI_CODE_VALUE (get);

  vivi_code_value_set_precedence (value, VIVI_PRECEDENCE_MEMBER);
}

char *
vivi_code_get_get_variable_name (ViviCodeGet *get)
{
  if (VIVI_IS_CODE_CONSTANT (get->name)) {
    return
      vivi_code_constant_get_variable_name (VIVI_CODE_CONSTANT (get->name));
  } else if (VIVI_IS_CODE_GET (get->name)) {
    return vivi_code_get_get_variable_name (VIVI_CODE_GET (get->name));
  } else {
    return NULL;
  }
}

ViviCodeValue *
vivi_code_get_new (ViviCodeValue *from, ViviCodeValue *name)
{
  ViviCodeGet *get;

  g_return_val_if_fail (from == NULL || VIVI_IS_CODE_VALUE (from), NULL);
  g_return_val_if_fail (VIVI_IS_CODE_VALUE (name), NULL);

  get = g_object_new (VIVI_TYPE_CODE_GET, NULL);
  if (from)
    get->from = g_object_ref (from);
  get->name = g_object_ref (name);

  return VIVI_CODE_VALUE (get);
}

ViviCodeValue *
vivi_code_get_new_name (const char *name)
{
  ViviCodeValue *result, *constant;

  g_return_val_if_fail (name != NULL, NULL);

  constant = vivi_code_constant_new_string (name);
  result = vivi_code_get_new (NULL, constant);
  g_object_unref (constant);
  return result;
}
