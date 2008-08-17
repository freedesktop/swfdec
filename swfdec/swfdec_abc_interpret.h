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

#ifndef _SWFDEC_ABC_INTERPRET_H_
#define _SWFDEC_ABC_INTERPRET_H_

#include <swfdec_abc_types.h>

G_BEGIN_DECLS


typedef enum {
  SWFDEC_ABC_OPCODE_BREAKPOINT = 0x01,
  SWFDEC_ABC_OPCODE_NOP = 0x02,
  SWFDEC_ABC_OPCODE_THROW = 0x03,
  SWFDEC_ABC_OPCODE_GET_SUPER = 0x04,
  SWFDEC_ABC_OPCODE_SET_SUPER = 0x05,
  SWFDEC_ABC_OPCODE_DXNS = 0x06,
  SWFDEC_ABC_OPCODE_DXNS_LATE = 0x07,
  SWFDEC_ABC_OPCODE_KILL = 0x08,
  SWFDEC_ABC_OPCODE_LABEL = 0x09,
  SWFDEC_ABC_OPCODE_IFNLT = 0x0C,
  SWFDEC_ABC_OPCODE_IFNLE = 0x0D,
  SWFDEC_ABC_OPCODE_IFNGT = 0x0E,
  SWFDEC_ABC_OPCODE_IFNGE = 0x0F,
  SWFDEC_ABC_OPCODE_JUMP = 0x10,
  SWFDEC_ABC_OPCODE_IFTRUE = 0x11,
  SWFDEC_ABC_OPCODE_IFFALSE = 0x12,
  SWFDEC_ABC_OPCODE_IFEQ = 0x13,
  SWFDEC_ABC_OPCODE_IFNE = 0x14,
  SWFDEC_ABC_OPCODE_IFLT = 0x15,
  SWFDEC_ABC_OPCODE_IFLE = 0x16,
  SWFDEC_ABC_OPCODE_IFGT = 0x17,
  SWFDEC_ABC_OPCODE_IFGE = 0x18,
  SWFDEC_ABC_OPCODE_IFSTRICTEQ = 0x19,
  SWFDEC_ABC_OPCODE_IFSTRICTNE = 0x1A,
  SWFDEC_ABC_OPCODE_LOOKUP_SWITCH = 0x1B,
  SWFDEC_ABC_OPCODE_PUSH_WITH = 0x1C,
  SWFDEC_ABC_OPCODE_POP_SCOPE = 0x1D,
  SWFDEC_ABC_OPCODE_NEXT_NAME = 0x1E,
  SWFDEC_ABC_OPCODE_HAS_NEXT = 0x1F,
  SWFDEC_ABC_OPCODE_PUSH_NULL = 0x20,
  SWFDEC_ABC_OPCODE_PUSH_UNDEFINED = 0x21,
  SWFDEC_ABC_OPCODE_NEXT_VALUE = 0x23,
  SWFDEC_ABC_OPCODE_PUSH_BYTE = 0x24,
  SWFDEC_ABC_OPCODE_PUSH_SHORT = 0x25,
  SWFDEC_ABC_OPCODE_PUSH_TRUE = 0x26,
  SWFDEC_ABC_OPCODE_PUSH_FALSE = 0x27,
  SWFDEC_ABC_OPCODE_PUSH_NAN = 0x28,
  SWFDEC_ABC_OPCODE_POP = 0x29,
  SWFDEC_ABC_OPCODE_DUP = 0x2A,
  SWFDEC_ABC_OPCODE_SWAP = 0x2B,
  SWFDEC_ABC_OPCODE_PUSH_STRING = 0x2C,
  SWFDEC_ABC_OPCODE_PUSH_INT = 0x2D,
  SWFDEC_ABC_OPCODE_PUSH_UINT = 0x2E,
  SWFDEC_ABC_OPCODE_PUSH_DOUBLE = 0x2F,
  SWFDEC_ABC_OPCODE_PUSH_SCOPE = 0x30,
  SWFDEC_ABC_OPCODE_PUSH_NAMESPACE = 0x31,
  SWFDEC_ABC_OPCODE_HAS_NEXT2 = 0x32,
  SWFDEC_ABC_OPCODE_NEW_FUNCTION = 0x40,
  SWFDEC_ABC_OPCODE_CALL = 0x41,
  SWFDEC_ABC_OPCODE_CONSTRUCT = 0x42,
  SWFDEC_ABC_OPCODE_CALL_METHOD = 0x43,
  SWFDEC_ABC_OPCODE_CALL_STATIC = 0x44,
  SWFDEC_ABC_OPCODE_CALL_SUPER = 0x45,
  SWFDEC_ABC_OPCODE_CALL_PROPERTY = 0x46,
  SWFDEC_ABC_OPCODE_RETURN_VOID = 0x47,
  SWFDEC_ABC_OPCODE_RETURN_VALUE = 0x48,
  SWFDEC_ABC_OPCODE_CONSTRUCT_SUPER = 0x49,
  SWFDEC_ABC_OPCODE_CONSTRUCT_PROP = 0x4A,
  SWFDEC_ABC_OPCODE_CALL_SUPER_ID = 0x4B,
  SWFDEC_ABC_OPCODE_CALL_PROP_LEX = 0x4C,
  SWFDEC_ABC_OPCODE_CALL_INTERFACE = 0x4D,
  SWFDEC_ABC_OPCODE_CALL_SUPER_VOID = 0x4E,
  SWFDEC_ABC_OPCODE_CALL_PROP_VOID = 0x4F,
  SWFDEC_ABC_OPCODE_NEW_OBJECT = 0x55,
  SWFDEC_ABC_OPCODE_NEW_ARRAY = 0x56,
  SWFDEC_ABC_OPCODE_NEW_ACTIVATION = 0x57,
  SWFDEC_ABC_OPCODE_NEW_CLASS = 0x58,
  SWFDEC_ABC_OPCODE_GET_DESCENDANTS = 0x59,
  SWFDEC_ABC_OPCODE_NEW_CATCH = 0x5A,
  SWFDEC_ABC_OPCODE_FIND_PROP_STRICT = 0x5D,
  SWFDEC_ABC_OPCODE_FIND_PROPERTY = 0x5E,
  SWFDEC_ABC_OPCODE_FIND_DEF = 0x5F,
  SWFDEC_ABC_OPCODE_GET_LEX = 0x60,
  SWFDEC_ABC_OPCODE_SET_PROPERTY = 0x61,
  SWFDEC_ABC_OPCODE_GET_LOCAL = 0x62,
  SWFDEC_ABC_OPCODE_SET_LOCAL = 0x63,
  SWFDEC_ABC_OPCODE_GET_GLOBAL_SCOPE = 0x64,
  SWFDEC_ABC_OPCODE_GET_SCOPE_OBJECT = 0x65,
  SWFDEC_ABC_OPCODE_GET_PROPERTY = 0x66,
  SWFDEC_ABC_OPCODE_INIT_PROPERTY = 0x68,
  SWFDEC_ABC_OPCODE_DELETE_PROPERTY = 0x6A,
  SWFDEC_ABC_OPCODE_GET_SLOT = 0x6C,
  SWFDEC_ABC_OPCODE_SET_SLOT = 0x6D,
  SWFDEC_ABC_OPCODE_GET_GLOBAL_SLOT = 0x6E,
  SWFDEC_ABC_OPCODE_SET_GLOBAL_SLOT = 0x6F,
  SWFDEC_ABC_OPCODE_CONVERT_S = 0x70,
  SWFDEC_ABC_OPCODE_ESC_XELEM = 0x71,
  SWFDEC_ABC_OPCODE_ESC_XATTR = 0x72,
  SWFDEC_ABC_OPCODE_CONVERT_I = 0x73,
  SWFDEC_ABC_OPCODE_CONVERT_U = 0x74,
  SWFDEC_ABC_OPCODE_CONVERT_D = 0x75,
  SWFDEC_ABC_OPCODE_CONVERT_B = 0x76,
  SWFDEC_ABC_OPCODE_CONVERT_O = 0x77,
  SWFDEC_ABC_OPCODE_CHECK_FILTER = 0x78,
  SWFDEC_ABC_OPCODE_COERCE = 0x80,
  SWFDEC_ABC_OPCODE_COERCE_B = 0x81,
  SWFDEC_ABC_OPCODE_COERCE_A = 0x82,
  SWFDEC_ABC_OPCODE_COERCE_I = 0x83,
  SWFDEC_ABC_OPCODE_COERCE_D = 0x84,
  SWFDEC_ABC_OPCODE_COERCE_S = 0x85,
  SWFDEC_ABC_OPCODE_AS_TYPE = 0x86,
  SWFDEC_ABC_OPCODE_AS_TYPE_LATE = 0x87,
  SWFDEC_ABC_OPCODE_COERCE_U = 0x88,
  SWFDEC_ABC_OPCODE_COERCE_O = 0x89,
  SWFDEC_ABC_OPCODE_NEGATE = 0x90,
  SWFDEC_ABC_OPCODE_INCREMENT = 0x91,
  SWFDEC_ABC_OPCODE_INC_LOCAL = 0x92,
  SWFDEC_ABC_OPCODE_DECREMENT = 0x93,
  SWFDEC_ABC_OPCODE_DEC_LOCAL = 0x94,
  SWFDEC_ABC_OPCODE_TYPEOF = 0x95,
  SWFDEC_ABC_OPCODE_NOT = 0x96,
  SWFDEC_ABC_OPCODE_BITNOT = 0x97,
  SWFDEC_ABC_OPCODE_CONCAT = 0x9A,
  SWFDEC_ABC_OPCODE_ADD_D = 0x9B,
  SWFDEC_ABC_OPCODE_ADD = 0xA0,
  SWFDEC_ABC_OPCODE_SUBTRACT = 0xA1,
  SWFDEC_ABC_OPCODE_MULTIPLY = 0xA2,
  SWFDEC_ABC_OPCODE_DIVIDE = 0xA3,
  SWFDEC_ABC_OPCODE_MODULO = 0xA4,
  SWFDEC_ABC_OPCODE_LSHIFT = 0xA5,
  SWFDEC_ABC_OPCODE_RSHIFT = 0xA6,
  SWFDEC_ABC_OPCODE_URSHIFT = 0xA7,
  SWFDEC_ABC_OPCODE_BITAND = 0xA8,
  SWFDEC_ABC_OPCODE_BITOR = 0xA9,
  SWFDEC_ABC_OPCODE_BITXOR = 0xAA,
  SWFDEC_ABC_OPCODE_EQUALS = 0xAB,
  SWFDEC_ABC_OPCODE_STRICT_EQUALS = 0xAC,
  SWFDEC_ABC_OPCODE_LESS_THAN = 0xAD,
  SWFDEC_ABC_OPCODE_LESS_EQUALS = 0xAE,
  SWFDEC_ABC_OPCODE_GREATER_THAN = 0xAF,
  SWFDEC_ABC_OPCODE_GREATER_EQUALS = 0xB0,
  SWFDEC_ABC_OPCODE_INSTANCE_OF = 0xB1,
  SWFDEC_ABC_OPCODE_IS_TYPE = 0xB2,
  SWFDEC_ABC_OPCODE_IS_TYPE_LATE = 0xB3,
  SWFDEC_ABC_OPCODE_IN = 0xB4,
  SWFDEC_ABC_OPCODE_INCREMENT_I = 0xC0,
  SWFDEC_ABC_OPCODE_DECREMENT_I = 0xC1,
  SWFDEC_ABC_OPCODE_INCLOCAL_I = 0xC2,
  SWFDEC_ABC_OPCODE_DECLOCAL_I = 0xC3,
  SWFDEC_ABC_OPCODE_NEGATE_I = 0xC4,
  SWFDEC_ABC_OPCODE_ADD_I = 0xC5,
  SWFDEC_ABC_OPCODE_SUBTRACT_I = 0xC6,
  SWFDEC_ABC_OPCODE_MULTIPLY_I = 0xC7,
  SWFDEC_ABC_OPCODE_GET_LOCAL_0 = 0xD0,
  SWFDEC_ABC_OPCODE_GET_LOCAL_1 = 0xD1,
  SWFDEC_ABC_OPCODE_GET_LOCAL_2 = 0xD2,
  SWFDEC_ABC_OPCODE_GET_LOCAL_3 = 0xD3,
  SWFDEC_ABC_OPCODE_SET_LOCAL_0 = 0xD4,
  SWFDEC_ABC_OPCODE_SET_LOCAL_1 = 0xD5,
  SWFDEC_ABC_OPCODE_SET_LOCAL_2 = 0xD6,
  SWFDEC_ABC_OPCODE_SET_LOCAL_3 = 0xD7,
  SWFDEC_ABC_OPCODE_ABS_JUMP = 0xEE,
  SWFDEC_ABC_OPCODE_DEBUG = 0xEF,
  SWFDEC_ABC_OPCODE_DEBUG_LINE = 0xF0,
  SWFDEC_ABC_OPCODE_DEBUG_FILE = 0xF1,
  SWFDEC_ABC_OPCODE_BREAKPOINT_LINE = 0xF2,
  SWFDEC_ABC_OPCODE_TIMESTAMP = 0xF3,
  SWFDEC_ABC_OPCODE_VERIFY_PASS = 0xF5,
  SWFDEC_ABC_OPCODE_ALLOC = 0xF6,
  SWFDEC_ABC_OPCODE_MARK = 0xF7,
  SWFDEC_ABC_OPCODE_WB = 0xF8,
  SWFDEC_ABC_OPCODE_PROLOGUE = 0xF9,
  SWFDEC_ABC_OPCODE_SEND_ENTER = 0xFA,
  SWFDEC_ABC_OPCODE_DOUBLE_TO_ATOM = 0xFB,
  SWFDEC_ABC_OPCODE_SWEEP = 0xFC,
  SWFDEC_ABC_OPCODE_CODEGEN = 0xFD,
  SWFDEC_ABC_OPCODE_VERIFY = 0xFE,
  SWFDEC_ABC_OPCODE_DECODE = 0xFF
} SwfdecAbcOpcode;

gboolean	swfdec_abc_interpret		(SwfdecAbcFunction *	fun,
						 SwfdecAbcScopeChain *	outer_scope);

const char *	swfdec_abc_opcode_get_name	(guint			opcode);


G_END_DECLS
#endif
