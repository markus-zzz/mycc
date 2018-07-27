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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "cg_tu.h"
#include "cg_func.h"
#include "cg_bb.h"
#include "cg_reg.h"

#include "util/graph_loop.h"

cg_func *
cg_func_build(cg_tu *tu, const char *name)
{
	cg_func *func = calloc(1, sizeof(cg_func));
	func->name = strdup(name);
	func->vreg_cntr = CG_REG_VREG0;
	if (tu->func_first == NULL)
	{
		assert(tu->func_last == NULL);
		tu->func_first = func;
	}
	else
	{
		assert(tu->func_last != NULL);
		tu->func_last->func_next = func;
	}
	tu->func_last = func;
	return func;
}

static graph_loop_bb_info *
get_bb_loop_info(graph_node *b)
{
	return &((cg_bb *)b)->loop_info;
}

void
cg_func_analyze_loops(cg_func *f)
{
	if (f->loop_analysis_cfg_version < f->cfg_graph_ctx.version)
	{
		/* cfg modified since last time loop_analyze ran */
		graph_loop_analyze(&f->cfg_graph_ctx,
		                   (graph_node *)f->bb_first,
		                   f->n_bbs,
		                   get_bb_loop_info);

		f->loop_analysis_cfg_version = f->cfg_graph_ctx.version;
	}
}
