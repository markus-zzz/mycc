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

#include "cg_func.h"
#include "cg_bb.h"
#include "cg_instr.h"
#include "cg_reg.h"

struct instr_edge {
	graph_edge edge;
	cg_bb *phi_arg_bb;
};

static int
edge_cmp(void *a, void *b)
{
	cg_instr *instr_a = a;
	cg_instr *instr_b = b;
	return instr_a->reg - instr_b->reg;
}

cg_instr *
cg_instr_build(cg_bb *bb, cg_instr_op op)
{
	cg_instr *instr = (cg_instr *)graph_create_node(sizeof(cg_instr));
	instr->bb = bb;
	instr->op = op;
	instr->cond = CG_COND_al;
	instr->reg = bb->func->vreg_cntr++;

	instr->ra.vreg = instr->reg;
	instr->ra.curr_reg = -1;
	instr->ra.spill_id = -1;
	instr->ra.dbg_spill_id = -1;

	return instr;
}

cg_instr *
cg_instr_build_phi(cg_bb *bb, ir_type type)
{
	cg_instr *instr = cg_instr_build(bb, CG_INSTR_OP_phi);

	if (bb->instr_phi_last != NULL)
	{
		assert(bb->instr_phi_first != NULL);
		instr->instr_prev = bb->instr_phi_last;
		bb->instr_phi_last->instr_next = instr;
	}
	else
	{
		assert(bb->instr_phi_first == NULL);
		bb->instr_phi_first = instr;
	}
	bb->instr_phi_last = instr;
	bb->n_phis++;

	return instr;
}

void
cg_instr_add_phi_arg(cg_instr *phi, cg_bb *arg_bb, cg_instr *arg)
{
	struct instr_edge *edge;
	graph_ctx *gctx = &phi->bb->func->vreg_graph_ctx;
	assert(phi->op == CG_INSTR_OP_phi);
	edge = (struct instr_edge *)graph_edge_create(gctx, (graph_node*)arg, (graph_node*)phi, sizeof(struct instr_edge), edge_cmp);
	edge->phi_arg_bb = arg_bb;
}

void
cg_instr_change_phi_arg(cg_instr *phi, cg_bb *arg_bb, cg_instr *arg)
{
	graph_edge *edge;
	graph_ctx *gctx = &phi->bb->func->vreg_graph_ctx;
	assert(phi->op == CG_INSTR_OP_phi);
	for (edge = graph_pred_first((graph_node *)phi);
	     edge != NULL;
	     edge = graph_pred_next((graph_edge *)edge))
	{
		if (((struct instr_edge *)edge)->phi_arg_bb == arg_bb)
		{
			graph_edge_delete(gctx, edge);
			break;
		}
	}
	assert(edge);
	edge = graph_edge_create(gctx, (graph_node*)arg, (graph_node*)phi, sizeof(struct instr_edge), edge_cmp);
	((struct instr_edge *)edge)->phi_arg_bb = arg_bb;
}

cg_instr *
cg_instr_phi_input_of(cg_instr *phi, cg_bb *b)
{
	graph_edge *edge;
	assert(phi->op == CG_INSTR_OP_phi);
	for (edge = graph_pred_first((graph_node *)phi); edge != NULL; edge = graph_pred_next(edge))
	{
		if (((struct instr_edge *)edge)->phi_arg_bb == b)
		{
			cg_instr *pred = (cg_instr *)graph_edge_tail(edge);
			return pred;
		}
	}

	assert(0);
	return NULL;
}

cg_bb *
cg_instr_phi_input_from(cg_instr *phi, cg_instr *arg)
{
	graph_edge *edge;
	assert(phi->op == CG_INSTR_OP_phi);
	for (edge = graph_pred_first((graph_node *)phi); edge != NULL; edge = graph_pred_next(edge))
	{
		cg_instr *pred = (cg_instr *)graph_edge_tail(edge);
		if (pred == arg)
		{
			return ((struct instr_edge *)edge)->phi_arg_bb;
		}
	}

	assert(0);
	return NULL;
}

void
cg_instr_arg_set_vreg(cg_instr *instr, unsigned arg_idx, cg_instr *arg)
{
	graph_ctx *gctx = &instr->bb->func->vreg_graph_ctx;
	assert(instr->op != CG_INSTR_OP_phi);
	assert(arg->reg >= 0);
	if (instr->args[arg_idx].kind == CG_INSTR_ARG_VREG)
	{
		graph_edge_delete(gctx, (graph_edge *)instr->args[arg_idx].u.vreg);
		instr->args[arg_idx].kind = CG_INSTR_ARG_INVALID;
	}
	assert(instr->args[arg_idx].kind == CG_INSTR_ARG_INVALID);
	instr->args[arg_idx].u.vreg = graph_edge_create(gctx, (graph_node*)arg, (graph_node*)instr, sizeof(struct instr_edge), edge_cmp);
	instr->args[arg_idx].kind = CG_INSTR_ARG_VREG;
}

void
cg_instr_replace_uses(cg_instr *old, cg_instr *new)
{
	graph_ctx *gctx = &old->bb->func->vreg_graph_ctx;
	graph_edge *edge, *next_edge;

	for (edge = graph_succ_first((graph_node *)old); edge != NULL; edge = next_edge)
	{
		int i;
		cg_instr *succ = (cg_instr *)graph_edge_head(edge);
		struct instr_edge *new_edge = (struct instr_edge *)graph_edge_create(gctx, (graph_node*)new, (graph_node*)succ, sizeof(struct instr_edge), edge_cmp);
		new_edge->phi_arg_bb = ((struct instr_edge *)edge)->phi_arg_bb;
		for (i = 0; i < CG_INSTR_N_ARGS; i++)
		{
			if (succ->args[i].kind == CG_INSTR_ARG_VREG &&
			    succ->args[i].u.vreg == edge)
			{
				succ->args[i].u.vreg = (graph_edge *)new_edge;
			}
		}
		next_edge = graph_succ_next(edge);
		graph_edge_delete(gctx, edge);
	}
}

void
cg_instr_link_first(cg_instr *instr)
{
	cg_bb *bb = instr->bb;

	assert(instr->op != CG_INSTR_OP_phi);
	assert(instr->instr_prev == NULL);
	assert(instr->instr_next == NULL);

	if (bb->instr_first != NULL)
	{
		assert(bb->instr_last != NULL);
		instr->instr_next = bb->instr_first;
		bb->instr_first->instr_prev = instr;
	}
	else
	{
		assert(bb->instr_last == NULL);
		bb->instr_last = instr;
	}
	bb->instr_first = instr;
	bb->n_instrs++;
}

void
cg_instr_link_before(cg_instr *ref, cg_instr *instr)
{
	cg_bb *bb = instr->bb;

	assert(instr->op != CG_INSTR_OP_phi);
	assert(instr->instr_prev == NULL);
	assert(instr->instr_next == NULL);

	instr->instr_next = ref;
	instr->instr_prev = ref->instr_prev;

	if (ref->instr_prev != NULL)
	{
		ref->instr_prev->instr_next = instr;
	}
	else
	{
		bb->instr_first = instr;
	}
	ref->instr_prev = instr;
	bb->n_instrs++;
}

void
cg_instr_link_after(cg_instr *ref, cg_instr *instr)
{
	cg_bb *bb = instr->bb;

	assert(instr->op != CG_INSTR_OP_phi);
	assert(instr->instr_prev == NULL);
	assert(instr->instr_next == NULL);

	instr->instr_prev = ref;
	instr->instr_next = ref->instr_next;

	if (ref->instr_next != NULL)
	{
		ref->instr_next->instr_prev = instr;
	}
	else
	{
		bb->instr_last = instr;
	}
	ref->instr_next = instr;
	bb->n_instrs++;
}

void
cg_instr_link_last(cg_instr *instr)
{
	cg_bb *bb = instr->bb;

	assert(instr->op != CG_INSTR_OP_phi);
	assert(instr->instr_prev == NULL);
	assert(instr->instr_next == NULL);

	if (bb->instr_last != NULL)
	{
		assert(bb->instr_first != NULL);
		instr->instr_prev = bb->instr_last;
		bb->instr_last->instr_next = instr;
	}
	else
	{
		assert(bb->instr_first == NULL);
		bb->instr_first = instr;
	}
	bb->instr_last = instr;
	bb->n_instrs++;
}

void
cg_instr_unlink(cg_instr *instr)
{
	cg_bb *bb = instr->bb;

	assert(instr->op != CG_INSTR_OP_phi);

	if (instr->instr_prev == NULL)
	{
		assert(bb->instr_first == instr);
		bb->instr_first = instr->instr_next;
	}
	else
	{
		instr->instr_prev->instr_next = instr->instr_next;
	}

	if (instr->instr_next == NULL)
	{
		assert(bb->instr_last == instr);
		bb->instr_last = instr->instr_prev;
	}
	else
	{
		instr->instr_next->instr_prev = instr->instr_prev;
	}

	instr->instr_prev = NULL;
	instr->instr_next = NULL;
	bb->n_instrs--;
}

void
cg_instr_phi_arg_iter_init(cg_instr_phi_arg_iter *it, cg_instr *phi)
{
	assert(phi->op == CG_INSTR_OP_phi);
	it->next_edge = graph_pred_first((graph_node*)phi);
}

cg_instr *
cg_instr_phi_arg_iter_next(cg_instr_phi_arg_iter *it, cg_bb **arg_bb)
{
	if (it->next_edge == NULL)
	{
		return NULL;
	}
	else
	{
		cg_instr *arg = (cg_instr *)graph_edge_tail(it->next_edge);
		if (arg_bb != NULL)
		{
			*arg_bb = ((struct instr_edge *)it->next_edge)->phi_arg_bb;
		}
		it->next_edge = graph_pred_next(it->next_edge);
		return arg;
	}
}

