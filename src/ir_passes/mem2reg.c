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

#include "ir/ir_dom.h"
#include "ir/ir_bb.h"
#include "ir/ir_node.h"
#include "ir/ir_func.h"
#include "ir_passes/mem2reg.h"
#include "util/bset.h"
#include "util/graph.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>


struct node_stack {
	struct node_stack *next;
	ir_node *n;
};


typedef struct variable {
	ir_node *alloca;
	int is_rejected;
	unsigned live_idx;
	struct node_stack *stack;
} variable;

static graph_marker scratch_marker;
static variable * scratch_get_var(ir_node *n)
{
	if (graph_marker_is_set((graph_node *)n, &scratch_marker))
	{
		return (variable *)ir_node_scratch(n);
	}
	else
	{
		return NULL;
	}
}

static void scratch_set_var(ir_node *n, variable *v)
{
	graph_marker_set((graph_node *)n, &scratch_marker);
	ir_node_scratch_set(n, v);
}

static void push_def(variable *var, ir_node *n)
{
	struct node_stack *slot = calloc(1, sizeof(struct node_stack));
	slot->n = n;
	slot->next = var->stack;
	var->stack = slot;
}

static void pop_def(variable *var)
{
	assert(var->stack != NULL);
	var->stack = var->stack->next;
}

static ir_node * top_def(variable *var)
{
	if (var->stack != NULL)
	{
		return var->stack->n;
	}
	else
	{
		return NULL;
	}
}

static void link_def_use_to_var(variable *var, ir_node *n)
{
	ir_node_use_iter it;
	ir_node *use;
	unsigned arg_idx;
	ir_node_use_iter_init(&it, n);
	while ((use = ir_node_use_iter_next(&it, &arg_idx)))
	{
		if (ir_node_op(use) == IR_OP_store)
		{
			if (arg_idx == 0)
			{
				scratch_set_var(use, var);
			}
			else
			{
				var->is_rejected = 1;
				return;
			}
		}
		else if (ir_node_op(use) == IR_OP_load)
		{
			scratch_set_var(use, var);
		}
		else
		{
			var->is_rejected = 1;
			return;
		}
	}
}

static void insert_phi_in_iter_df(ir_bb *bb, variable *var, ir_type type, graph_marker *marker)
{
	ir_dom_info_lst *lst;
	ir_node *phi;

	graph_marker_set((graph_node *)bb, marker);

	for (lst = ir_bb_dom_info(bb)->df; lst != NULL; lst = lst->next)
	{
		ir_bb *df = lst->info->bb;
		if (!graph_marker_is_set((graph_node *)df, marker) &&
		    bset_has(ir_bb_scratch(df), var->live_idx))
		{
			/* Variable is in livein[df] so a phi-node is needed in df */
			phi = ir_node_build_phi(df, type);
			scratch_set_var(phi, var);
			insert_phi_in_iter_df(df, var, type, marker);
		}
	}
}

static void walk_domtree_rename(ir_bb *bb)
{
	ir_node_iter nit;
	ir_node *n, *nn;
	graph_edge *edge;
	ir_dom_info_lst *lst;

	ir_node_iter_init(&nit, bb);
	while ((n = ir_node_iter_next(&nit)))
	{
		ir_node *args[2];
		variable *var = scratch_get_var(n);
		if (var == NULL || var->is_rejected)
		{
			continue;
		}

		switch (ir_node_op(n))
		{
		case IR_OP_phi:
			push_def(var, n);
			break;

		case IR_OP_store:
			ir_node_get_args(n, NULL, args, 2);
			push_def(var, args[1]);
			break;

		case IR_OP_load:
			if ((nn = top_def(var)) == NULL)
			{
				nn = ir_node_build0(bb, IR_OP_undef, ir_node_type(n));
			}

			ir_node_replace(n, nn);
			break;

		default:
			break;
		}
	}

	for (edge = graph_succ_first((graph_node *)bb);
		 edge != NULL;
		 edge = graph_succ_next(edge))
	{
		ir_bb *succ = (ir_bb *)graph_edge_head(edge);
		ir_node_iter_init(&nit, succ);
		while ((n = ir_node_iter_next(&nit)) && ir_node_op(n) == IR_OP_phi)
		{
			variable *var = scratch_get_var(n);
			if (var == NULL)
			{
				continue;
			}

			assert(var->is_rejected == 0);

			if ((nn = top_def(var)) == NULL)
			{
				nn = ir_node_build0(bb, IR_OP_undef, ir_node_type(n));
			}
			ir_node_add_phi_arg(n, bb, nn);
		}
	}

	for (lst = ir_bb_dom_info(bb)->domtree_children; lst != NULL; lst = lst->next)
	{
		walk_domtree_rename(lst->info->bb);
	}

	ir_node_iter_init(&nit, bb);
	while ((n = ir_node_iter_next(&nit)))
	{
		variable *var = scratch_get_var(n);
		if (var == NULL || var->is_rejected)
		{
			continue;
		}

		switch (ir_node_op(n))
		{
		case IR_OP_phi:
			pop_def(var);
			break;

		case IR_OP_store:
			pop_def(var);
			ir_node_remove(n);
			break;

		case IR_OP_load:
			ir_node_remove(n);
			break;

		default:
			break;
		}
	}
}

static void compute_livein(ir_func *func, unsigned n_vars)
{
	bset_set *tmp, *gen, *kill, *liveout;
	int livein_changed = 1;
	ir_bb_iter bit;
	ir_bb *bb;

	ir_bb_iter_init(&bit, func);
	while ((bb = ir_bb_iter_next(&bit)))
	{
		ir_bb_scratch_set(bb, bset_create_set(n_vars)); /* TODO:FIXME: Can do this more efficiently with a node marker */
	}

	tmp  = bset_create_set(n_vars);
	gen  = bset_create_set(n_vars);
	kill = bset_create_set(n_vars);
	liveout = bset_create_set(n_vars);

	while (livein_changed)
	{
		livein_changed = 0;
		ir_bb_iter_rev_init(&bit, func);
		while ((bb = ir_bb_iter_next(&bit)))
		{
			bset_set *livein = ir_bb_scratch(bb);
			graph_edge *edge;
			ir_node_iter nit;
			ir_node *n;

			bset_clear(tmp);
			bset_clear(gen);
			bset_clear(kill);
			bset_clear(liveout);

			/* liveout[bb] is union of livein[succ] for all succs of bb */
			for (edge = graph_succ_first((graph_node *)bb);
				 edge != NULL;
				 edge = graph_succ_next(edge))
			{
				ir_bb *succ = (ir_bb *)graph_edge_head(edge);
				bset_union(liveout, ir_bb_scratch(succ));
			}

			ir_node_iter_rev_init(&nit, bb);
			while ((n = ir_node_iter_next(&nit)))
			{
				variable *var;
				if (ir_node_op(n) == IR_OP_store && (var = scratch_get_var(n)))
				{
					bset_add(kill, var->live_idx);
					bset_remove(gen, var->live_idx);
				}
				else if (ir_node_op(n) == IR_OP_load && (var = scratch_get_var(n)))
				{
					bset_add(gen, var->live_idx);
				}
			}

			bset_not(kill, kill);
			bset_intersect(liveout, kill);
			bset_union(liveout, gen);
			if (!bset_equal(livein, liveout))
			{
				bset_copy(livein, liveout);
				livein_changed = 1;
			}
		}
	}
}

static void do_mem2reg(ir_func *func)
{
	ir_bb_iter bit;
	ir_bb *bb;
	ir_node_iter nit;
	ir_node *n;
	unsigned idx = 0;

	/* Dominance information required */
	ir_dom_setup_dom_info(func);

	graph_marker_alloc(&func->ssa_graph_ctx, &scratch_marker);

	/* All alloca nodes will be found in the entry block */
	ir_node_iter_init(&nit, func->entry);
	while ((n = ir_node_iter_next(&nit)))
	{
		if (ir_node_op(n) == IR_OP_alloca)
		{
			variable *var = calloc(1, sizeof(variable));
			scratch_set_var(n, var);
			var->alloca = n;
			var->live_idx = idx++;

			link_def_use_to_var(var, n);
		}
	}

	/* Compute liveness for pruned ssa-form */
	compute_livein(func, idx);

	/* Determine location for phi-nodes. Find definitions of variables
	   and insert phi-nodes in the iterated dominance frontier of the
	   defining block. */

	ir_bb_iter_init(&bit, func);
	while ((bb = ir_bb_iter_next(&bit)))
	{
		ir_node_iter_init(&nit, bb);
		while ((n = ir_node_iter_next(&nit)))
		{
			if (ir_node_op(n) == IR_OP_store)
			{
				graph_marker marker;
				variable *var = scratch_get_var(n);
				if (var != NULL && var->is_rejected == 0)
				{
					graph_marker_alloc(&func->cfg_graph_ctx, &marker);
					insert_phi_in_iter_df(ir_node_bb(n), var, ir_node_type(n), &marker);
					graph_marker_free(&func->cfg_graph_ctx, &marker);
				}
			}
		}
	}

	/* Walk the dominator tree and rename each variable as
	   appropriate */
	walk_domtree_rename(func->entry);
	graph_marker_free(&func->ssa_graph_ctx, &scratch_marker);

	/* Dominance information no longer needed */
	ir_dom_destroy_dom_info(func);
}

ir_pass mem2reg = {
	"mem2reg",
	do_mem2reg
};

