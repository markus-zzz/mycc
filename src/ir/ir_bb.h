/*
 * MyCC - A lightweight C compiler and experimentation platform
 *
 * Copyright (C) 2018 Markus Lavin (https://www.zzzconsulting.se/)
 *
 * All rights reserved.
 *
 * This file is part of MyCC.
 *
 * MyCC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MyCC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MyCC. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include "ir/ir.h"

typedef struct ir_bb_iter {
	ir_bb **po_bbs;
	unsigned idx;
} ir_bb_iter;

ir_bb *
ir_bb_build(ir_func *func);

void
ir_bb_build_cond_br(ir_bb *bb, ir_node *cond, ir_bb *true_bb, ir_bb *false_bb);

void
ir_bb_build_br(ir_bb *bb, ir_bb *target_bb);

void
ir_bb_build_value_ret(ir_bb *bb, ir_node *value);

void
ir_bb_build_ret(ir_bb *bb);

void
ir_bb_iter_init(ir_bb_iter *it, ir_func *func);

void
ir_bb_iter_rev_init(ir_bb_iter *it, ir_func *func);

ir_bb *
ir_bb_iter_next(ir_bb_iter *it);

ir_bb *
ir_bb_get_default_target(ir_bb *bb);

ir_bb *
ir_bb_get_true_target(ir_bb *bb);

ir_bb *
ir_bb_get_false_target(ir_bb *bb);

ir_node *
ir_bb_get_term_node(ir_bb *bb);

unsigned
ir_bb_id(ir_bb *bb);

void *
ir_bb_scratch(ir_bb *bb);

void
ir_bb_scratch_set(ir_bb *bb, void *p);

struct ir_dom_info *
ir_bb_dom_info(ir_bb *bb);
