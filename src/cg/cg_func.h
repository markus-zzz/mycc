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

#ifndef CG_FUNC_H
#define CG_FUNC_H

#include "cg/cg.h"
#include "util/graph.h"

#define N_ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

struct cg_func {
	struct cg_func *func_next;
	const char *name;
	graph_ctx vreg_graph_ctx; /* vregs are in SSA form */
	graph_ctx cfg_graph_ctx;

	struct cg_bb *bb_first;
	struct cg_bb *bb_last;

	unsigned n_bbs;
	unsigned vreg_cntr;
	unsigned clobber_mask;

	unsigned stack_frame_size;
	struct cg_instr *args[16]; /* function arguments/parameters as arg instrs. */

	struct {
		struct cg_bb **rpo;
		struct dset_ctx *equiv_vreg;
		struct dset_ctx *equiv_spill_id;
		struct reg_info *rinfo;
		int *spill_slot_offsets;
		int n_spill_slots;
		int n_rinfo;
	} ra;

	unsigned loop_analysis_cfg_version;
};

cg_func *
cg_func_build(cg_tu *tu, const char *name);

void
cg_func_analyze_loops(cg_func *f);

#endif
