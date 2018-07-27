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

#include "ir_node_private.h"
#include "ir_bb_private.h"

#include "ir_bb.h"
#include "ir_func.h"
#include "ir_validate.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>

typedef struct node_edge {
	graph_edge edge;
	union {
		struct {
			unsigned idx;
		} arg;
		struct {
			ir_bb *bb;
		} phi_arg;
	} u;
} node_edge;


static unsigned global_node_id = 0;

static int
edge_cmp(void *a, void *b)
{
	ir_node *n_a = a;
	ir_node *n_b = b;
	int id_a, id_b;

	switch (n_a->op)
	{
		case IR_OP_br:
		case IR_OP_ret:
		case IR_OP_term:
			id_a = n_a->bb->id * 0x10000;
			break;
		default:
			id_a = n_a->id;
			break;
	}

	switch (n_b->op)
	{
		case IR_OP_br:
		case IR_OP_ret:
		case IR_OP_term:
			id_b = n_b->bb->id * 0x10000;
			break;
		default:
			id_b = n_b->id;
			break;
	}

	return id_a - id_b;
}

static void
bb_link_phi_last(ir_node *n, ir_bb *bb)
{
	assert(n->op == IR_OP_phi);
	n->bb_list_next = NULL;
	n->bb_list_prev = bb->last_ir_phi_node;
	if (bb->first_ir_phi_node != NULL)
	{
		assert(bb->last_ir_phi_node != NULL);
		bb->last_ir_phi_node->bb_list_next = n;
	}
	else
	{
		assert(bb->last_ir_phi_node == NULL);
		bb->first_ir_phi_node = n;
	}
	bb->last_ir_phi_node = n;
}

static void
bb_unlink_phi(ir_node *n)
{
	ir_bb *bb = n->bb;
	assert(n->op == IR_OP_phi);

	if (bb->first_ir_phi_node == n)
	{
		assert(n->bb_list_prev == NULL);
		bb->first_ir_phi_node = n->bb_list_next;
	}
	else
	{
		assert(n->bb_list_prev != NULL);
		n->bb_list_prev->bb_list_next = n->bb_list_next;
	}

	if (bb->last_ir_phi_node == n)
	{
		assert(n->bb_list_next == NULL);
		bb->last_ir_phi_node = n->bb_list_prev;
	}
	else
	{
		assert(n->bb_list_next != NULL);
		n->bb_list_next->bb_list_prev = n->bb_list_prev;
	}

	n->bb_list_prev = NULL;
	n->bb_list_next = NULL;
}
static void
bb_link_last(ir_node *n, ir_bb *bb)
{
	assert(n->op != IR_OP_phi);
	n->bb_list_next = NULL;
	n->bb_list_prev = bb->last_ir_node;
	if (bb->first_ir_node != NULL)
	{
		assert(bb->last_ir_node != NULL);
		bb->last_ir_node->bb_list_next = n;
	}
	else
	{
		assert(bb->last_ir_node == NULL);
		bb->first_ir_node = n;
	}
	bb->last_ir_node = n;
}

static void
bb_link_before(ir_node *n, ir_node *before, ir_bb *bb)
{
	assert(n->op != IR_OP_phi);
	n->bb_list_next = before;
	if (before->bb_list_prev != NULL)
	{
		before->bb_list_prev->bb_list_next = n;
		n->bb_list_prev = before->bb_list_prev;
	}
	else
	{
		n->bb_list_prev = NULL;
		bb->first_ir_node = n;
	}
	before->bb_list_prev = n;
}

static void
bb_unlink(ir_node *n)
{
	ir_bb *bb = n->bb;
	assert(n->op != IR_OP_phi);

	if (bb->first_ir_node == n)
	{
		assert(n->bb_list_prev == NULL);
		bb->first_ir_node = n->bb_list_next;
	}
	else
	{
		assert(n->bb_list_prev != NULL);
		n->bb_list_prev->bb_list_next = n->bb_list_next;
	}

	if (bb->last_ir_node == n)
	{
		assert(n->bb_list_next == NULL);
		bb->last_ir_node = n->bb_list_prev;
	}
	else
	{
		assert(n->bb_list_next != NULL);
		n->bb_list_next->bb_list_prev = n->bb_list_prev;
	}

	n->bb_list_prev = NULL;
	n->bb_list_next = NULL;
}

static void
bb_redist_depth(ir_bb *bb)
{
	unsigned depth = 0x10000;
	ir_node *n;
	for (n = bb->first_ir_node; n != NULL; n = n->bb_list_next)
	{
		n->bb_list_depth = depth;
		depth += 0x10000;
	}
}

static void
bb_move_up_before(ir_node *before, ir_node *n)
{
	graph_edge *edge;
	unsigned dist;

	dist = before->bb_list_depth - (before->bb_list_prev ? before->bb_list_prev->bb_list_depth : 0);

	if (dist < 2)
	{
		bb_redist_depth(before->bb);
	}

	bb_unlink(n);
	n->bb_list_depth = before->bb_list_depth - dist/2;
	bb_link_before(n, before, before->bb);

	for (edge = graph_pred_first((graph_node *)n); edge != NULL; edge = graph_pred_next(edge))
	{
		ir_node *pred = (ir_node *)graph_edge_tail(edge);
		if (pred->bb == n->bb &&
		    pred->op != IR_OP_phi &&
		    pred->bb_list_depth > n->bb_list_depth)
		{
			bb_move_up_before(n, pred);
		}
	}
}

static void
bb_move_up_if_needed(ir_node *n)
{
	graph_edge *edge;
	ir_node *min_node = NULL;
	unsigned min_depth = UINT_MAX;

	for (edge = graph_succ_first((graph_node *)n); edge != NULL; edge = graph_succ_next(edge))
	{
		ir_node *succ = (ir_node *)graph_edge_head(edge);
		if (succ->bb == n->bb &&
		    succ->op != IR_OP_phi &&
		    succ->op != IR_OP_term &&
		    succ->bb_list_depth < min_depth)
		{
			min_depth = succ->bb_list_depth;
			min_node = succ;
		}
	}

	if (min_node != NULL && n->bb_list_depth > min_node->bb_list_depth)
	{
		bb_move_up_before(min_node, n);
	}
}

static void
mark_unused(ir_node *n, ir_func *func)
{
	if (n->op == IR_OP_call || n->op == IR_OP_store)
	{
		return;
	}

	assert(n->status == IR_NODE_USED);
	n->unused_list_next = NULL;
	n->unused_list_prev = func->last_unused_ir_node;
	if (func->first_unused_ir_node != NULL)
	{
		assert(func->last_unused_ir_node != NULL);
		func->last_unused_ir_node->unused_list_next = n;
	}
	else
	{
		assert(func->last_unused_ir_node == NULL);
		func->first_unused_ir_node = n;
	}
	func->last_unused_ir_node = n;
	n->status = IR_NODE_UNUSED;
}

static void
mark_used(ir_node *n)
{
	ir_func *func = n->bb->func;
	if (n->op == IR_OP_call || n->op == IR_OP_store)
	{
		return;
	}

	assert(n->status == IR_NODE_UNUSED);
	if (func->first_unused_ir_node == n)
	{
		assert(n->unused_list_prev == NULL);
		func->first_unused_ir_node = n->unused_list_next;
	}
	else
	{
		assert(n->unused_list_prev != NULL);
		n->unused_list_prev->unused_list_next = n->unused_list_next;
	}

	if (func->last_unused_ir_node == n)
	{
		assert(n->unused_list_next == NULL);
		func->last_unused_ir_node = n->unused_list_prev;
	}
	else
	{
		assert(n->unused_list_next != NULL);
		n->unused_list_next->unused_list_prev = n->unused_list_prev;
	}

	n->unused_list_prev = NULL;
	n->unused_list_next = NULL;
	n->status = IR_NODE_USED;
}


static ir_node *
build_node(ir_bb *bb, ir_op op, ir_type type)
{
	ir_node *n = (ir_node *)graph_create_node(sizeof(ir_node));
	n->op = op;
	n->type = type;
	n->bb = bb;
	n->id = ++global_node_id;
	n->status = IR_NODE_USED;

	if (op != IR_OP_term)
	{
		bb->n_ir_nodes++;
		bb->func->n_ir_nodes++;
		if (op == IR_OP_phi)
		{
			n->bb_list_depth = 0;
			bb_link_phi_last(n, bb);
		}
		else
		{
			n->bb_list_depth = (bb->last_ir_node != NULL ? bb->last_ir_node->bb_list_depth : 0) + 0x10000;
			bb_link_last(n, bb);
		}
		if (op != IR_OP_store && op != IR_OP_call)
		{
			mark_unused(n, bb->func);
		}
	}

	return n;
}

static void
add_arg(ir_node *n, unsigned arg_idx, ir_node *arg)
{
	graph_ctx *gctx = &n->bb->func->ssa_graph_ctx;
	node_edge *arg_edge;

	if (graph_succ_first((graph_node *)arg) == NULL)
	{
		mark_used(arg);
	}

	arg_edge = (node_edge *)graph_edge_create(gctx, (graph_node*)arg, (graph_node*)n, sizeof(node_edge), edge_cmp);
	arg_edge->u.arg.idx = arg_idx;
}

ir_node *
ir_node_build0(ir_bb *bb, ir_op op, ir_type type)
{
	ir_node *n;

	n = build_node(bb, op, type);

	ir_validate_node(n);

	return n;
}

ir_node *
ir_node_build1(ir_bb *bb, ir_op op, ir_type type, ir_node *arg1)
{
	ir_node *n;

	assert(arg1 != NULL);

	n = build_node(bb, op, type);
	add_arg(n, 0, arg1);

	ir_validate_node(n);

	return n;
}

ir_node *
ir_node_build2(ir_bb *bb, ir_op op, ir_type type, ir_node *arg1, ir_node *arg2)
{
	ir_node *n;

	assert(arg1 != NULL);
	assert(arg2 != NULL);

	n = build_node(bb, op, type);
	add_arg(n, 0, arg1);
	add_arg(n, 1, arg2);

	ir_validate_node(n);

	return n;
}

ir_node *
ir_node_build3(ir_bb *bb, ir_op op, ir_type type, ir_node *arg1, ir_node *arg2, ir_node *arg3)
{
	ir_node *n;

	assert(arg1 != NULL);
	assert(arg2 != NULL);
	assert(arg3 != NULL);

	n = build_node(bb, op, type);
	add_arg(n, 0, arg1);
	add_arg(n, 1, arg2);
	add_arg(n, 2, arg3);

	ir_validate_node(n);

	return n;
}

ir_node *
ir_node_build_const(ir_bb *bb, ir_type type, unsigned value)
{
	ir_node *n;

	n = build_node(bb, IR_OP_const, type);

	switch (type) {
	case i1:
		n->u.constant.u.i1 = value ? 1 : 0;;
		break;
	case i8:
		n->u.constant.u.i8 = value;
		break;
	case i16:
		n->u.constant.u.i16 = value;
		break;
	case i32:
	case p32:
		n->u.constant.u.i32 = value;
	case i64:
	case p64:
		n->u.constant.u.i64 = value;
		break;
	default:
		assert(0);
		break;
	}

	ir_validate_node(n);

	return n;
}

ir_node *
ir_node_build_alloca(ir_bb *bb, unsigned size, unsigned align)
{
	ir_node *n;

	n = build_node(bb, IR_OP_alloca, p32);

	n->u.alloca.size = size;
	n->u.alloca.align = align;

	ir_validate_node(n);

	return n;
}

ir_node *
ir_node_build_addr_of(ir_bb *bb, ir_data *sym)
{
	ir_node *n;

	n = build_node(bb, IR_OP_addr_of, p32);

	n->u.addr_of.sym = sym;

	ir_validate_node(n);

	return n;
}

ir_node *
ir_node_build_getparam(ir_bb *bb, ir_type type, unsigned idx)
{
	ir_node *n;

	n = build_node(bb, IR_OP_getparam, type);

	n->u.getparam.idx = idx;

	ir_validate_node(n);

	return n;
}

ir_node *
ir_node_build_call(ir_bb *bb, ir_func *target, ir_type type, unsigned n_args, ir_node **args)
{
	ir_node *n;
	unsigned i;

	n = build_node(bb, IR_OP_call, type);
	for (i = 0; i < n_args; i++)
	{
		assert(args[i]);
		add_arg(n, i, args[i]);
	}

	n->u.call.target = target;

	ir_validate_node(n);

	return n;
}


ir_node *
ir_node_build_phi(ir_bb *bb, ir_type type)
{
	ir_node *n;

	n = build_node(bb, IR_OP_phi, type);

	ir_validate_node(n);

	return n;
}

void
ir_node_add_phi_arg(ir_node *phi, ir_bb *arg_bb, ir_node *arg)
{
	graph_ctx *gctx = &phi->bb->func->ssa_graph_ctx;
	node_edge *phi_edge;
	assert(phi->op == IR_OP_phi);

	if (graph_succ_first((graph_node *)arg) == NULL)
	{
		mark_used(arg);
	}

	phi_edge = (node_edge *)graph_edge_create(gctx, (graph_node*)arg, (graph_node*)phi, sizeof(node_edge), edge_cmp);
	phi_edge->u.phi_arg.bb = arg_bb;

	ir_validate_node(phi);
}

void
ir_node_remove(ir_node *n)
{
	graph_ctx *gctx = &n->bb->func->ssa_graph_ctx;
	ir_node *args[4];
	unsigned n_args;
	unsigned i;

	if (n->op == IR_OP_store || n->op == IR_OP_call)
	{
		mark_unused(n, n->bb->func);
	}

	assert(graph_succ_first((graph_node *)n) == NULL && "Cannot remove used node!");

	mark_used(n); /* remove it from unused list */
	n->bb->n_ir_nodes--;
	n->bb->func->n_ir_nodes--;

	if (n->op == IR_OP_phi)
	{
		bb_unlink_phi(n);
	}
	else
	{
		bb_unlink(n);
	}

	ir_node_get_args(n, &n_args, args, sizeof(args)/sizeof(args[0]));
	for (i = 0; i < n_args; i++)
	{
		graph_edge *edge = graph_succ_first((graph_node *)args[i]);
		if (edge != NULL && graph_succ_next(edge) == NULL)
		{
			/* pred with exactly one succ (n) will become unused */
			mark_unused(args[i], args[i]->bb->func);
		}
	}

	graph_node_delete(gctx, (graph_node *)n);
}

void
ir_node_replace(ir_node *old, ir_node *new)
{
	graph_ctx *gctx = &old->bb->func->ssa_graph_ctx;
	graph_edge *edge, *next_edge;

	if (graph_succ_first((graph_node *)old) != NULL)
	{
		mark_unused(old, old->bb->func);
		if (graph_succ_first((graph_node *)new) == NULL)
		{
			mark_used(new);
		}
	}

	for (edge = graph_succ_first((graph_node *)old); edge != NULL; edge = next_edge)
	{
		ir_node *succ = (ir_node *)graph_edge_head(edge);
		node_edge *new_edge = (node_edge *)graph_edge_create(gctx, (graph_node*)new, (graph_node*)succ, sizeof(node_edge), edge_cmp);
		new_edge->u = ((node_edge *)edge)->u;
		next_edge = graph_succ_next(edge);
		graph_edge_delete(gctx, edge);
		ir_validate_node(succ);
	}

	bb_move_up_if_needed(new);
}

void
ir_node_change_arg(ir_node *n, ir_node *old_arg, ir_node *new_arg)
{
	graph_ctx *gctx = &n->bb->func->ssa_graph_ctx;
	graph_edge *edge, *next_edge;

	assert(0 && "Needs to handle unused");

	for (edge = graph_pred_first((graph_node *)n); edge != NULL; edge = next_edge)
	{
		ir_node *pred = (ir_node *)graph_edge_tail(edge);
		next_edge = graph_pred_next(edge);
		if (pred == old_arg)
		{
			node_edge *new_edge = (node_edge *)graph_edge_create(
					gctx, (graph_node*)new_arg, (graph_node*)n, sizeof(node_edge), edge_cmp);
			new_edge->u.arg.idx = ((node_edge *)edge)->u.arg.idx;
			graph_edge_delete(gctx, edge);
		}
	}

	ir_validate_node(n);

	bb_move_up_if_needed(new_arg);
}

int
ir_node_cmp(ir_node *a, ir_node *b)
{
	assert(a->bb == b->bb);
	return a->bb_list_depth - b->bb_list_depth;
}

void
ir_node_arg_iter_init(ir_node_arg_iter *it, ir_node *n)
{
	assert(n->op != IR_OP_phi);
	it->next_edge = graph_pred_first((graph_node*)n);
}

ir_node *
ir_node_arg_iter_next(ir_node_arg_iter *it, unsigned *arg_idx)
{
	if (it->next_edge == NULL)
	{
		return NULL;
	}
	else
	{
		ir_node *arg = (ir_node *)graph_edge_tail(it->next_edge);
		if (arg_idx != NULL)
		{
			*arg_idx = ((node_edge *)it->next_edge)->u.arg.idx;
		}
		it->next_edge = graph_pred_next(it->next_edge);
		return arg;
	}
}

void
ir_node_phi_arg_iter_init(ir_node_phi_arg_iter *it, ir_node *phi)
{
	assert(phi->op == IR_OP_phi);
	it->next_edge = graph_pred_first((graph_node*)phi);
}

ir_node *
ir_node_phi_arg_iter_next(ir_node_phi_arg_iter *it, ir_bb **arg_bb)
{
	if (it->next_edge == NULL)
	{
		return NULL;
	}
	else
	{
		ir_node *arg = (ir_node *)graph_edge_tail(it->next_edge);
		if (arg_bb != NULL)
		{
			*arg_bb = ((node_edge *)it->next_edge)->u.phi_arg.bb;
		}
		it->next_edge = graph_pred_next(it->next_edge);
		return arg;
	}
}

void
ir_node_use_iter_init(ir_node_use_iter *it, ir_node *n)
{
	it->next_edge = graph_succ_first((graph_node*)n);
}

ir_node *
ir_node_use_iter_next(ir_node_use_iter *it, unsigned *arg_idx)
{
	if (it->next_edge == NULL)
	{
		return NULL;
	}
	else
	{
		ir_node *use = (ir_node *)graph_edge_head(it->next_edge);
		if (arg_idx != NULL && use->op != IR_OP_phi)
		{
			*arg_idx = ((node_edge *)it->next_edge)->u.arg.idx;
		}
		it->next_edge = graph_succ_next(it->next_edge);
		return use;
	}
}

void
ir_node_get_args(ir_node *n, unsigned *n_args, ir_node **args, unsigned args_size)
{
	graph_edge *edge = graph_pred_first((graph_node*)n);
	unsigned counter = 0;

	assert(n->op != IR_OP_phi);

	while (edge)
	{
		ir_node *arg = (ir_node *)graph_edge_tail(edge);
		unsigned arg_idx = ((node_edge *)edge)->u.arg.idx;

		if (arg_idx < args_size)
		{
			args[arg_idx] = arg;
		}
		counter++;
		edge = graph_pred_next(edge);
	}

	if (n_args != NULL)
	{
		*n_args = counter;
	}
}

void
ir_node_iter_init(ir_node_iter *it, ir_bb *bb)
{
	ir_node *n;
	unsigned idx = 0;

	it->po_nodes = calloc(bb->n_ir_nodes, sizeof(ir_node *));

	idx = bb->n_ir_nodes;
	for (n = bb->first_ir_phi_node; n != NULL; n = n->bb_list_next)
	{
		assert(n->op == IR_OP_phi);
		it->po_nodes[--idx] = n;
	}
	for (n = bb->first_ir_node; n != NULL; n = n->bb_list_next)
	{
		assert(n->op != IR_OP_phi);
		it->po_nodes[--idx] = n;
	}
	assert(idx == 0);
	it->idx = bb->n_ir_nodes;
}

void
ir_node_iter_rev_init(ir_node_iter *it, ir_bb *bb)
{
	ir_node *n;
	unsigned idx = 0;

	it->po_nodes = calloc(bb->n_ir_nodes, sizeof(ir_node *));

	idx = bb->n_ir_nodes;
	for (n = bb->last_ir_phi_node; n != NULL; n = n->bb_list_prev)
	{
		assert(n->op == IR_OP_phi);
		it->po_nodes[--idx] = n;
	}
	for (n = bb->last_ir_node; n != NULL; n = n->bb_list_prev)
	{
		assert(n->op != IR_OP_phi);
		it->po_nodes[--idx] = n;
	}
	assert(idx == 0);
	it->idx = bb->n_ir_nodes;
}

ir_node *
ir_node_iter_next(ir_node_iter *it)
{
	if (it->idx == 0)
	{
		return NULL;
	}
	else
	{
		return it->po_nodes[--it->idx];
	}
}

void ir_func_free_unused_nodes(ir_func *func)
{
	while (func->first_unused_ir_node != NULL)
	{
		ir_node *n = func->first_unused_ir_node;

		n->bb->n_ir_nodes--;
		n->bb->func->n_ir_nodes--;
		if (n->op == IR_OP_phi)
		{
			bb_unlink_phi(n);
		}
		else
		{
			bb_unlink(n);
		}
		mark_used(n);

		if (n->op == IR_OP_phi)
		{
			ir_node_phi_arg_iter it;
			ir_node *arg;

			ir_node_phi_arg_iter_init(&it, n);
			while ((arg = ir_node_phi_arg_iter_next(&it, NULL)))
			{
				graph_edge *edge = graph_succ_first((graph_node *)arg);
				if (edge != NULL && graph_succ_next(edge) == NULL)
				{
					mark_unused(arg, arg->bb->func);
				}
			}
		}
		else
		{
			ir_node *args[4];
			unsigned n_args;
			unsigned i;

			ir_node_get_args(n, &n_args, args, sizeof(args)/sizeof(args[0]));
			for (i = 0; i < n_args; i++)
			{
				graph_edge *edge = graph_succ_first((graph_node *)args[i]);
				if (edge != NULL && graph_succ_next(edge) == NULL)
				{
					mark_unused(args[i], args[i]->bb->func);
				}
			}
		}

		graph_node_delete(&func->ssa_graph_ctx, (graph_node *)n);
	}
}

void
ir_bb_set_term_node(ir_bb *bb, ir_node *n)
{
	graph_ctx *gctx = &bb->func->ssa_graph_ctx;
	node_edge *edge;

	assert(bb->term_node != NULL);
	assert(bb->term_node->op == IR_OP_term);

	graph_preds_delete(gctx, (graph_node *)bb->term_node);

	if (n != NULL)
	{
		if (graph_succ_first((graph_node *)n) == NULL)
		{
			mark_used(n);
		}

		edge = (node_edge *)graph_edge_create(gctx, (graph_node *)n, (graph_node *)bb->term_node, sizeof(node_edge), edge_cmp);
		edge->u.arg.idx = 0;
	}
}

unsigned
ir_node_id(ir_node *n)
{
	return n->id;
}

ir_type
ir_node_type(ir_node *n)
{
	return n->type;
}

ir_bb *
ir_node_bb(ir_node *n)
{
	return n->bb;
}

ir_op
ir_node_op(ir_node *n)
{
	return n->op;
}

ir_data *
ir_node_addr_of_data(ir_node *n)
{
	assert(n->op == IR_OP_addr_of);
	return n->u.addr_of.sym;
}

ir_func *
ir_node_call_target(ir_node *n)
{
	assert(n->op == IR_OP_call);
	return n->u.call.target;
}

unsigned
ir_node_alloca_size(ir_node *n)
{
	assert(n->op == IR_OP_alloca);
	return n->u.alloca.size;
}

unsigned
ir_node_alloca_align(ir_node *n)
{
	assert(n->op == IR_OP_alloca);
	return n->u.alloca.align;
}

unsigned
ir_node_getparam_idx(ir_node *n)
{
	return n->u.getparam.idx;
}

uint64_t
ir_node_const_as_u64(ir_node *n)
{
	uint64_t value;

	assert(n->op == IR_OP_const);

	switch (n->type) {
	default:
		assert(n->type == i1);
		value = n->u.constant.u.i1;
		break;
	case i8:
		value = n->u.constant.u.i8;
		break;
	case i16:
		value = n->u.constant.u.i16;
		break;
	case i32:
	case p32:
		value = n->u.constant.u.i32;
	case i64:
	case p64:
		value = n->u.constant.u.i64;
		break;
	}

	return value;
}

int64_t
ir_node_const_as_i64(ir_node *n)
{
	int64_t value;

	assert(n->op == IR_OP_const);

	switch (n->type) {
	default:
		assert(n->type == i1);
		value = n->u.constant.u.i1;
		break;
	case i8:
		value = n->u.constant.u.i8;
		break;
	case i16:
		value = n->u.constant.u.i16;
		break;
	case i32:
	case p32:
		value = n->u.constant.u.i32;
	case i64:
	case p64:
		value = n->u.constant.u.i64;
		break;
	}

	return value;
}

void *
ir_node_scratch(ir_node *n)
{
	return n->scratch;
}

void
ir_node_scratch_set(ir_node *n, void *p)
{
	n->scratch = p;
}
