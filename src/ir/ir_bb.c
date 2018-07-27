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

#include "ir_bb_private.h"
#include "ir_func.h"
#include "ir_node_private.h"

#include <assert.h>
#include <stdlib.h>

typedef struct cfg_edge {
	graph_edge edge;
	enum {DEFAULT_TARGET, FALSE_TARGET = DEFAULT_TARGET, TRUE_TARGET} target;
} cfg_edge;

static unsigned global_bb_id = 0;

static int
edge_cmp(void *a, void *b)
{
	ir_bb *bb_a = a;
	ir_bb *bb_b = b;

	return bb_a->id - bb_b->id;
}

ir_bb *
ir_bb_build(ir_func *func)
{
	ir_bb *bb = (ir_bb *)graph_create_node(sizeof(ir_bb));
	bb->id = ++global_bb_id;
	bb->func = func;
	func->n_ir_bbs++;

	if (func->entry == NULL)
	{
		func->entry = bb;
	}

	bb->term_node = ir_node_build0(bb, IR_OP_term, i1);

	return bb;
}

void
ir_bb_build_br(ir_bb *bb, ir_bb *target_bb)
{
	graph_ctx *gctx = &bb->func->cfg_graph_ctx;
	cfg_edge *edge;

	graph_succs_delete(gctx, (graph_node *)bb);

	bb->term_kind = IR_BB_TERM_BR;
	ir_bb_set_term_node(bb, NULL);

	edge = (cfg_edge *)graph_edge_create(gctx, (graph_node*)bb,
							(graph_node*)target_bb, sizeof(cfg_edge), edge_cmp);
	edge->target = DEFAULT_TARGET;
}

void
ir_bb_build_cond_br(ir_bb *bb, ir_node *cond, ir_bb *true_bb, ir_bb *false_bb)
{
	graph_ctx *gctx = &bb->func->cfg_graph_ctx;
	cfg_edge *edge;

	assert(cond != NULL);

	graph_succs_delete(gctx, (graph_node *)bb);

	bb->term_kind = IR_BB_TERM_BR;
	ir_bb_set_term_node(bb, cond);

	edge = (cfg_edge *)graph_edge_create(gctx, (graph_node*)bb,
							(graph_node*)true_bb, sizeof(cfg_edge), edge_cmp);
	edge->target = TRUE_TARGET;

	edge = (cfg_edge *)graph_edge_create(gctx, (graph_node*)bb,
							(graph_node*)false_bb, sizeof(cfg_edge), edge_cmp);
	edge->target = FALSE_TARGET;
}

void
ir_bb_build_ret(ir_bb *bb)
{
	assert(bb->func->exit == NULL);
	bb->func->exit = bb;
	bb->term_kind = IR_BB_TERM_RET;
	ir_bb_set_term_node(bb, NULL);
}

void
ir_bb_build_value_ret(ir_bb *bb, ir_node *value)
{
	assert(value != NULL);
	assert(bb->func->exit == NULL);
	bb->func->exit = bb;
	bb->term_kind = IR_BB_TERM_RET;
 	ir_bb_set_term_node(bb, value);
}

static void
po_worker(ir_bb_iter *it, graph_node *n, graph_marker *marker)
{
	graph_edge *edge;

	if (graph_marker_is_set(n, marker))
	{
		return;
	}

	graph_marker_set(n, marker);

	for (edge = graph_succ_first(n); edge != NULL; edge = graph_succ_next(edge))
	{
		graph_node *succ = graph_edge_head(edge);
		po_worker(it, succ, marker);
	}

	it->po_bbs[it->idx++] = (ir_bb *)n;
}

void
ir_bb_iter_init(ir_bb_iter *it, ir_func *func)
{
	graph_marker marker;

	graph_marker_alloc(&func->cfg_graph_ctx, &marker);

	it->po_bbs = calloc(func->n_ir_bbs, sizeof(ir_bb *));
	it->idx = 0;

	po_worker(it, (graph_node *)func->entry, &marker);

	graph_marker_free(&func->cfg_graph_ctx, &marker);
}

void
ir_bb_iter_rev_init(ir_bb_iter *it, ir_func *func)
{
	graph_marker marker;
	unsigned i;

	graph_marker_alloc(&func->cfg_graph_ctx, &marker);

	it->po_bbs = calloc(func->n_ir_bbs, sizeof(ir_bb *));
	it->idx = 0;

	po_worker(it, (graph_node *)func->entry, &marker);

	for (i = 0; i < func->n_ir_bbs/2; i++)
	{
		ir_bb *tmp = it->po_bbs[func->n_ir_bbs-i-1];
		it->po_bbs[func->n_ir_bbs-i-1] = it->po_bbs[i];
		it->po_bbs[i] = tmp;
	}

	graph_marker_free(&func->cfg_graph_ctx, &marker);
}

ir_bb *
ir_bb_iter_next(ir_bb_iter *it)
{
	if (it->idx == 0)
	{
		return NULL;
	}
	else
	{
		return it->po_bbs[--it->idx];
	}
}

ir_bb *
ir_bb_get_default_target(ir_bb *bb)
{
	graph_edge *edge;

	assert(bb->term_kind == IR_BB_TERM_BR);
	assert(ir_bb_get_term_node(bb) == NULL);

	edge = graph_succ_first((graph_node *)bb);
	assert(graph_succ_next(edge) == NULL);
	assert(((cfg_edge *)edge)->target == DEFAULT_TARGET);

	return (ir_bb *)graph_edge_head(edge);
}

ir_bb *
ir_bb_get_true_target(ir_bb *bb)
{
	graph_edge *edge;

	assert(bb->term_kind == IR_BB_TERM_BR);
	assert(ir_bb_get_term_node(bb) != NULL);

	for (edge = graph_succ_first((graph_node *)bb); edge != NULL; edge = graph_succ_next(edge))
	{
		if (((cfg_edge *)edge)->target == TRUE_TARGET)
		{
			return (ir_bb *)graph_edge_head(edge);
		}
	}

	assert(0);
	return NULL;
}

ir_bb *
ir_bb_get_false_target(ir_bb *bb)
{
	graph_edge *edge;

	assert(bb->term_kind == IR_BB_TERM_BR);
	assert(ir_bb_get_term_node(bb) != NULL);

	for (edge = graph_succ_first((graph_node *)bb); edge != NULL; edge = graph_succ_next(edge))
	{
		if (((cfg_edge *)edge)->target == FALSE_TARGET)
		{
			return (ir_bb *)graph_edge_head(edge);
		}
	}

	assert(0);
	return NULL;
}

ir_node *
ir_bb_get_term_node(ir_bb *bb)
{
	ir_node *args[1];
	unsigned n_args;

	assert(bb->term_node != NULL);
	assert(bb->term_node->op == IR_OP_term);

	ir_node_get_args(bb->term_node, &n_args, args, 1);
	if (n_args == 1)
	{
		return args[0];
	}
	else
	{
		assert(n_args == 0);
		return NULL;
	}
}

unsigned
ir_bb_id(ir_bb *bb)
{
	return bb->id;
}

void *
ir_bb_scratch(ir_bb *bb)
{
	return bb->scratch;
}

void
ir_bb_scratch_set(ir_bb *bb, void *p)
{
	bb->scratch = p;
}

struct ir_dom_info *
ir_bb_dom_info(ir_bb *bb)
{
	return bb->dom_info;
}
