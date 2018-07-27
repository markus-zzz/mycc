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
#include "util/graph.h"

/* TODO:FIXME: split into public and private header file */

typedef enum ir_op {
#define DEF_IR_OP(x) IR_OP_##x,
#include "ir_op.def"
#undef DEF_IR_OP
} ir_op;

typedef struct ir_node_arg_iter {
	graph_edge *next_edge;
} ir_node_arg_iter;

typedef struct ir_node_phi_arg_iter {
	graph_edge *next_edge;
} ir_node_phi_arg_iter;

typedef struct ir_node_use_iter {
	graph_edge *next_edge;
} ir_node_use_iter;

typedef struct ir_node_iter {
	ir_node **po_nodes;
	unsigned idx;
} ir_node_iter;




/*
 * Build IR nodes
 */
ir_node *
ir_node_build0(ir_bb *bb, ir_op op, ir_type type);

ir_node *
ir_node_build1(ir_bb *bb, ir_op op, ir_type type, ir_node *arg1);

ir_node *
ir_node_build2(ir_bb *bb, ir_op op, ir_type type, ir_node *arg1, ir_node *arg2);

ir_node *
ir_node_build3(ir_bb *bb, ir_op op, ir_type type, ir_node *arg1, ir_node *arg2, ir_node *arg3);

ir_node *
ir_node_build_const(ir_bb *bb, ir_type type, unsigned value);

ir_node *
ir_node_build_phi(ir_bb *bb, ir_type type);

void
ir_node_add_phi_arg(ir_node *phi, ir_bb *arg_bb, ir_node *arg);

ir_node *
ir_node_build_alloca(ir_bb *bb, unsigned size, unsigned align);

ir_node *
ir_node_build_addr_of(ir_bb *bb, ir_data *sym);

ir_node *
ir_node_build_getparam(ir_bb *bb, ir_type type, unsigned idx);

ir_node *
ir_node_build_call(ir_bb *bb, ir_func *target, ir_type type, unsigned n_args, ir_node **args);

/*
 * Modify IR nodes
 */

void
ir_node_remove(ir_node *n);

void
ir_node_replace(ir_node *old, ir_node *new);

void
ir_node_change_arg(ir_node *n, ir_node *old_arg, ir_node *new_arg);

void
ir_node_move_to_bb(ir_node *n, ir_bb *bb);


/*
 * Iterate over IR nodes
 */
/* TODO:FIXME: replase _init with _new so that we can hide the internals of the iterators */
void
ir_node_arg_iter_init(ir_node_arg_iter *it, ir_node *n);

ir_node *
ir_node_arg_iter_next(ir_node_arg_iter *it, unsigned *arg_idx);

void
ir_node_phi_arg_iter_init(ir_node_phi_arg_iter *it, ir_node *phi);

ir_node *
ir_node_phi_arg_iter_next(ir_node_phi_arg_iter *it, ir_bb **arg_bb);

void
ir_node_use_iter_init(ir_node_use_iter *it, ir_node *n);

ir_node *
ir_node_use_iter_next(ir_node_use_iter *it, unsigned *arg_idx);

void
ir_node_iter_init(ir_node_iter *it, ir_bb *bb);

void
ir_node_iter_rev_init(ir_node_iter *it, ir_bb *bb);

ir_node *
ir_node_iter_next(ir_node_iter *it);

void
ir_node_get_args(ir_node *n, unsigned *n_args, ir_node **args, unsigned args_size);

void
ir_bb_set_term_node(ir_bb *bb, ir_node *n);

int
ir_node_cmp(ir_node *a, ir_node *b);


unsigned
ir_node_id(ir_node *n);

ir_type
ir_node_type(ir_node *n);

ir_bb *
ir_node_bb(ir_node *n);

ir_op
ir_node_op(ir_node *n);

ir_data *
ir_node_addr_of_data(ir_node *n);

ir_func *
ir_node_call_target(ir_node *n);

unsigned
ir_node_alloca_size(ir_node *n);

unsigned
ir_node_alloca_align(ir_node *n);

unsigned
ir_node_getparam_idx(ir_node *n);

int64_t
ir_node_const_as_i64(ir_node *n);

uint64_t
ir_node_const_as_u64(ir_node *n);

void *
ir_node_scratch(ir_node *n);

void
ir_node_scratch_set(ir_node *n, void *p);
