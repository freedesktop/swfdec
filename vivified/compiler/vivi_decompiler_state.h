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

#ifndef _VIVI_DECOMPILER_STATE_H_
#define _VIVI_DECOMPILER_STATE_H_

#include <swfdec/swfdec.h>
#include <vivified/compiler/vivi_decompiler_value.h>

G_BEGIN_DECLS


typedef struct _ViviDecompilerState ViviDecompilerState;


void				vivi_decompiler_state_free	(ViviDecompilerState *		state);
ViviDecompilerState *		vivi_decompiler_state_new	(SwfdecScript *			script,
								 const guint8 *			pc,
								 guint				n_registers);
ViviDecompilerState *		vivi_decompiler_state_copy	(const ViviDecompilerState *	src);

void				vivi_decompiler_state_push	(ViviDecompilerState *		state,
								 ViviDecompilerValue *		val);
ViviDecompilerValue *		vivi_decompiler_state_pop	(ViviDecompilerState *		state);
const ViviDecompilerValue *	vivi_decompiler_state_get_register (const ViviDecompilerState *	state,
								 guint				reg);

const guint8 *			vivi_decompiler_state_get_pc	(const ViviDecompilerState *	state);
void				vivi_decompiler_state_add_pc	(ViviDecompilerState *		state,
								 int				diff);
const SwfdecConstantPool *    	vivi_decompiler_state_get_constant_pool 
								(const ViviDecompilerState *	state);
void				vivi_decompiler_state_set_constant_pool 
								(ViviDecompilerState *		state, 
								 SwfdecConstantPool *		pool);
guint				vivi_decompiler_state_get_version
								(const ViviDecompilerState *  	state);


G_END_DECLS
#endif
