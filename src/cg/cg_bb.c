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

#include "cg_bb.h"
#include "cg_func.h"

cg_bb *
cg_bb_build(cg_func *func)
{
	cg_bb *bb = calloc(1, sizeof(cg_bb));
	bb->func = func;
	bb->id = func->n_bbs++;
	return bb;
}

static int
edge_cmp(void *a, void *b)
{
	cg_bb *bb_a = a;
	cg_bb *bb_b = b;

	return bb_a->id - bb_b->id;
}

void
cg_bb_link_cfg(cg_bb *pred, cg_bb *succ)
{
	graph_ctx *gctx = &pred->func->cfg_graph_ctx;
	(void)graph_edge_create(gctx, (graph_node*)pred, (graph_node*)succ, sizeof(struct graph_edge), edge_cmp);
}

void
cg_bb_link_first(cg_bb *bb)
{
	cg_func *f = bb->func;

	bb->bb_prev = NULL;
	if (f->bb_first != NULL)
	{
		assert(f->bb_last != NULL);
		bb->bb_next = f->bb_first;
		f->bb_first->bb_prev = bb;
	}
	else
	{
		assert(f->bb_last == NULL);
		f->bb_last = bb;
	}
	f->bb_first = bb;
}

void
cg_bb_link_before(cg_bb *ref, cg_bb *bb)
{
}

void
cg_bb_link_after(cg_bb *ref, cg_bb *bb)
{
}

void
cg_bb_link_last(cg_bb *bb)
{
	cg_func *f = bb->func;

	assert(bb->bb_prev == NULL);
	assert(bb->bb_next == NULL);

	if (f->bb_last != NULL)
	{
		assert(f->bb_first != NULL);
		bb->bb_prev = f->bb_last;
		f->bb_last->bb_next = bb;
	}
	else
	{
		assert(f->bb_first == NULL);
		f->bb_first = bb;
	}
	f->bb_last = bb;
}

void
cg_bb_unlink(cg_bb *bb)
{
	cg_func *f = bb->func;

	if (bb->bb_prev == NULL)
	{
		assert(f->bb_first == bb);
		f->bb_first = bb->bb_next;
	}
	else
	{
		bb->bb_prev->bb_next = bb->bb_next;
	}

	if (bb->bb_next == NULL)
	{
		assert(f->bb_last == bb);
		f->bb_last = bb->bb_prev;
	}
	else
	{
		bb->bb_next->bb_prev = bb->bb_prev;
	}

	bb->bb_prev = NULL;
	bb->bb_next = NULL;
	f->n_bbs--;
}

unsigned
cg_bb_loop_nest(cg_bb *bb)
{
	cg_func *f = bb->func;
	graph_node *n;
	unsigned nest = bb->loop_info.type == GRAPH_LOOP_HEADER ? 1 : 0;

	/* Will only do work if needed */
	cg_func_analyze_loops(f);

	n = bb->loop_info.header;
	while (n != NULL)
	{
		nest++;
		n = ((cg_bb *)n)->loop_info.header;
	}

	return nest;
}
