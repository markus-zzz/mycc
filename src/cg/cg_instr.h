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

#ifndef CR_INSTR_H
#define CR_INSTR_H

#include "cg/cg.h"
#include "cg/cg_cond.h"
#include "ir/ir.h"
#include "util/graph.h"
#include "cg/lifetime.h"

#define CG_INSTR_N_ARGS 5

typedef enum cg_instr_op {
#define DEF_CG_INSTR(x) CG_INSTR_OP_##x,
#include "cg_instr.def"
#undef DEF_CG_INSTR
} cg_instr_op;

struct cg_instr {
	graph_node graph;

	struct cg_bb *bb;

	struct cg_instr *instr_prev;
	struct cg_instr *instr_next;

	cg_instr_op op;
	cg_cond cond;
	int reg;

	union {
		struct {
			unsigned arg_reg_mask;
			unsigned ret_reg_mask;
		} call;
	} u;

	struct {
		enum {CG_INSTR_ARG_INVALID, CG_INSTR_ARG_VREG, CG_INSTR_ARG_HREG, CG_INSTR_ARG_IMM, CG_INSTR_ARG_SYM} kind;
		union {
			graph_edge *vreg;
			unsigned imm;
			unsigned hreg;
			char *sym;
		} u;
		int offset;
	} args[CG_INSTR_N_ARGS];

	struct {
		struct pos pos;
		int spill_id;
		int dbg_spill_id; /* For debug use only. */
		int curr_reg;
		int vreg;
	} ra;
};

typedef struct cg_instr_phi_arg_iter {
	graph_edge *next_edge;
} cg_instr_phi_arg_iter;

void
cg_instr_phi_arg_iter_init(cg_instr_phi_arg_iter *it, cg_instr *phi);

cg_instr *
cg_instr_phi_arg_iter_next(cg_instr_phi_arg_iter *it, cg_bb **arg_bb);

cg_instr *
cg_instr_build_phi(cg_bb *bb, ir_type type);

void
cg_instr_add_phi_arg(cg_instr *phi, cg_bb *arg_bb, cg_instr *arg);

void
cg_instr_change_phi_arg(cg_instr *phi, cg_bb *arg_bb, cg_instr *arg);

cg_instr *
cg_instr_phi_input_of(cg_instr *phi, cg_bb *b);

cg_bb *
cg_instr_phi_input_from(cg_instr *phi, cg_instr *arg);

cg_instr *
cg_instr_build(cg_bb *bb, cg_instr_op op);

void
cg_instr_arg_set_vreg(cg_instr *instr, unsigned arg_idx, cg_instr *arg);

void
cg_instr_replace_uses(cg_instr *old, cg_instr *new);

void
cg_instr_link_first(cg_instr *instr);

void
cg_instr_link_before(cg_instr *ref, cg_instr *instr);

void
cg_instr_link_after(cg_instr *ref, cg_instr *instr);

void
cg_instr_link_last(cg_instr *instr);

void
cg_instr_unlink(cg_instr *instr);

#endif
