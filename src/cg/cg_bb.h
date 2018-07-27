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

#ifndef CG_BB_H
#define CG_BB_H

/* contains an ordered list of cg_instr. Otherwise similar to ir_bb */
#include "cg/cg.h"
#include "cg/cg_cond.h"
#include "util/bset.h"
#include "util/graph.h"
#include "util/graph_loop.h"
#include "cg/lifetime.h"

struct cg_bb {
	graph_node graph;

	unsigned id;
	struct cg_func *func;

	/* blocks are ordered in emit sequence */
	struct cg_bb *bb_prev;
	struct cg_bb *bb_next;

	struct cg_instr *instr_phi_first;
	struct cg_instr *instr_phi_last;

	struct cg_instr *instr_first;
	struct cg_instr *instr_last;

	struct cg_dom_info *dom_info;

	struct cg_bb *true_target;
	struct cg_bb *false_target;
	cg_cond true_cond;

	unsigned n_phis;
	unsigned n_instrs;

	struct {
		bset_set *livein;
		struct pos ival_from;
		struct pos ival_to;
		int po;
		unsigned domtree_po;
	} ra;

	struct graph_loop_bb_info loop_info;
};

cg_bb *
cg_bb_build(cg_func *func);

void
cg_bb_link_cfg(cg_bb *pred, cg_bb *succ);

void
cg_bb_link_first(cg_bb *bb);

void
cg_bb_link_before(cg_bb *ref, cg_bb *bb);

void
cg_bb_link_after(cg_bb *ref, cg_bb *bb);

void
cg_bb_link_last(cg_bb *bb);

void
cg_bb_unlink(cg_bb *bb);


unsigned
cg_bb_loop_nest(cg_bb *bb);

#endif
