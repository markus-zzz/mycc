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
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "cg_tu.h"
#include "cg_func.h"
#include "cg_bb.h"
#include "cg_instr.h"
#include "cg_reg.h"
#include "cg_print.h"

#define D(x)

/*
	===========
	Pattern #1:
	===========
	...
	branch{gt} %bb1, %bb2
   bb1:
	mov %r0, #1
	branch %bb3
   bb2:
	mov %r0, #2
	branch %bb3
	...

	into:

	...
	mov{gt} %r0, #1
	mov{le} %r0, #2
	branch %bb3
	...
	
	===========
	Pattern #2:
	===========
	...
	branch{gt} %bb1, %bb2
   bb1:
	mov %r0, #1
	branch %bb2
	...

	into:

	...
	mov{gt} %r0, #1
	branch %bb2
	...

	===========
	Pattern #3:
	===========
	...
	branch{gt} %bb2, %bb1
   bb1:
	mov %r0, #1
	branch %bb2
	...

	into:

	...
	mov{le} %r0, #1
	branch %bb2
	...
*/

static int
can_predicate_block(cg_bb *b)
{
	cg_instr *instr;

	for (instr = b->instr_first; instr != NULL; instr = instr->instr_next)
	{
		switch (instr->op)
		{
		case CG_INSTR_OP_call:
			return 0;
		default:
			break;
		}
	}

	return 1;
}

static cg_bb *
get_single_pred(cg_bb *b)
{
	graph_edge *edge = graph_pred_first((graph_node *)b);
	cg_bb *pred = (cg_bb *)graph_edge_tail(edge);
	return graph_pred_next(edge) ? NULL : pred;
}

static cg_bb *
get_single_succ(cg_bb *b)
{
	graph_edge *edge = graph_succ_first((graph_node *)b);
	cg_bb *succ = (cg_bb *)graph_edge_head(edge);
	return graph_succ_next(edge) ? NULL : succ;
}

static void
move_and_conditionalize(cg_bb *to, cg_bb *from, cg_cond cond)
{
	cg_instr *instr, *next_instr;

	for (instr = from->instr_first; instr != NULL; instr = next_instr)
	{
		next_instr = instr->instr_next;
		cg_instr_unlink(instr);

		instr->bb = to;
		instr->cond = cond;
		cg_instr_link_last(instr);
	}
}

static int
try_pattern_1(cg_bb *bb)
{
	cg_bb *true_succ, *false_succ;

	if (!bb->true_target || !bb->false_target)
	{
		return 0;
	}

	if (!get_single_pred(bb->true_target) || !get_single_pred(bb->false_target))
	{
		return 0;
	}

	true_succ = get_single_succ(bb->true_target);
	false_succ = get_single_succ(bb->false_target);

	if (!true_succ || !false_succ)
	{
		return 0;
	}

	if (true_succ != false_succ)
	{
		return 0;
	}

	if (bb->true_target->n_instrs > 2 || !can_predicate_block(bb->true_target) ||
	    bb->false_target->n_instrs > 2 || !can_predicate_block(bb->false_target))
	{
		return 0;
	}

	D(printf("bb%d is a candidate for branch predication pattern #1\n", bb->id));

	move_and_conditionalize(bb, bb->true_target, bb->true_cond);
	move_and_conditionalize(bb, bb->false_target, cg_cond_inv(bb->true_cond));

	graph_node_delete(&bb->func->cfg_graph_ctx, (graph_node *)bb->true_target);
	graph_node_delete(&bb->func->cfg_graph_ctx, (graph_node *)bb->false_target);

	cg_bb_link_cfg(bb, true_succ);

	cg_bb_unlink(bb->true_target);
	cg_bb_unlink(bb->false_target);

	bb->true_cond = CG_COND_al;
	bb->true_target = NULL;
	bb->false_target = NULL;

	return 1;
}

static int
try_pattern_2(cg_bb *bb)
{
	cg_bb *true_succ;

	if (!bb->true_target || !bb->false_target)
	{
		return 0;
	}

	if (!get_single_pred(bb->true_target))
	{
		return 0;
	}

	true_succ = get_single_succ(bb->true_target);

	if (!true_succ)
	{
		return 0;
	}

	if (true_succ != bb->false_target)
	{
		return 0;
	}

	if (bb->true_target->n_instrs > 2 || !can_predicate_block(bb->true_target))
	{
		return 0;
	}

	D(printf("bb%d is a candidate for branch predication pattern #2\n", bb->id));

	move_and_conditionalize(bb, bb->true_target, bb->true_cond);

	graph_node_delete(&bb->func->cfg_graph_ctx, (graph_node *)bb->true_target);

	cg_bb_unlink(bb->true_target);

	bb->true_cond = CG_COND_al;
	bb->true_target = NULL;
	bb->false_target = NULL;

	return 1;
}

static int
try_pattern_3(cg_bb *bb)
{
	cg_bb *false_succ;

	if (!bb->true_target || !bb->false_target)
	{
		return 0;
	}

	if (!get_single_pred(bb->false_target))
	{
		return 0;
	}

	false_succ = get_single_succ(bb->false_target);

	if (!false_succ)
	{
		return 0;
	}

	if (false_succ != bb->true_target)
	{
		return 0;
	}

	if (bb->false_target->n_instrs > 2 || !can_predicate_block(bb->false_target))
	{
		return 0;
	}

	D(printf("bb%d is a candidate for branch predication pattern #3\n", bb->id));

	move_and_conditionalize(bb, bb->false_target, cg_cond_inv(bb->true_cond));

	graph_node_delete(&bb->func->cfg_graph_ctx, (graph_node *)bb->false_target);

	cg_bb_unlink(bb->false_target);

	bb->true_cond = CG_COND_al;
	bb->true_target = NULL;
	bb->false_target = NULL;

	return 1;
}

static void
branch_predication_func(cg_func *func)
{
	cg_bb *bb;

	for (bb = func->bb_first; bb != NULL; bb = bb->bb_next)
	{
		if (try_pattern_1(bb))
		{
			continue;
		}
		if (try_pattern_2(bb))
		{
			continue;
		}
		if (try_pattern_3(bb))
		{
			continue;
		}
	}
}

void
cg_branch_predication_tu(cg_tu *tu)
{
	cg_func *f;

	for (f = tu->func_first; f != NULL; f = f->func_next)
	{
		branch_predication_func(f);
	}
}

