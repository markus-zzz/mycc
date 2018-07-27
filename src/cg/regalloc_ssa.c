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

#include "regalloc_ssa.h"
#include "cg_dom.h"
#include "cg_tu.h"
#include "cg_func.h"
#include "cg_bb.h"
#include "cg_instr.h"
#include "cg_reg.h"
#include "cg_print.h"
#include "ir/ir_tu.h"
#include "ir/ir_func.h"
#include "ir/ir_bb.h"
#include "ir/ir_node.h"
#include "util/bset.h"
#include "util/dset.h"

#define DEBUG_SSA_RA 0

#if DEBUG_SSA_RA
#define D(x) x
#else
#define D(x)
#endif

#define MAX(a,b) ((a)<(b)?(b):(a))
#define MIN(a,b) ((a)<(b)?(a):(b))

#define IPOS_NEGINF SHRT_MIN
#define IPOS_POSINF SHRT_MAX
#define IPOS_SPACING 4


typedef struct interval {
	struct interval *next;
	struct pos from;
	struct pos to;
} interval;

typedef struct reg_info {
	cg_instr *instr;
	struct interval *liverange; /* liverange for register */
	struct interval *equiv_liverange; /* liverange for leader of equivalence class */
	int *live;
} reg_info;

static int
pos_cmp(struct pos a, struct pos b);

typedef struct lifetime_tracker_ctx {
	cg_func *func;
	bset_set *live;
	struct interval **liverange;
	unsigned n_live;
	cg_instr *next_instr;
	unsigned n_room_for;
	int skip_vreg;

} lifetime_tracker_ctx;

static void
insert_single_swap(cg_bb *b, cg_instr *before, cg_instr *x, cg_instr *y);

void
lifetime_tracker_init(lifetime_tracker_ctx *ctx, cg_func *f)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->func = f;
	ctx->n_room_for = f->vreg_cntr + 128;
	ctx->live = bset_create_set(ctx->n_room_for);
	ctx->liverange = calloc(sizeof(ctx->liverange[0]), ctx->n_room_for);
	ctx->skip_vreg = -1;
}

void
lifetime_tracker_start(lifetime_tracker_ctx *ctx, cg_bb *b)
{
	const cg_func *f = ctx->func;
	int i;

	ctx->next_instr = b->instr_first;
	ctx->n_live = 0;

	for (i = CG_REG_VREG0; i < ctx->func->vreg_cntr; i++)
	{
		struct interval *p;
		for (p = f->ra.rinfo[i].liverange; p; p = p->next)
		{
			if (pos_cmp(p->from, b->ra.ival_from) <= 0 && pos_cmp(b->ra.ival_from, p->to) < 0)
			{
				break;
			}
		}

		ctx->liverange[i] = p;

		if (p)
		{
			bset_add(ctx->live, i);
			ctx->n_live++;
		}
		else
		{
			bset_remove(ctx->live, i);
		}
	}
}

cg_instr *
lifetime_tracker_next(lifetime_tracker_ctx *ctx, unsigned *n_live)
{
	/* if not start of block only need to check intervals instr uses and instr def */
	/* add a bunch of assert to make sure ranges are consistent with live set */

	cg_instr *instr = ctx->next_instr;

	if (instr)
	{
		unsigned i;

		for (i = 0; i < CG_INSTR_N_ARGS; i++)
		{
			if (instr->args[i].kind == CG_INSTR_ARG_VREG)
			{
				cg_instr *arg = (cg_instr *)graph_edge_tail(instr->args[i].u.vreg);
				int arg_reg = arg->ra.vreg;
#if 0
				/* TODO:FIXME: Does not work if there are two or more uses in same instruction */
				assert(ctx->liverange[arg_reg] && "must be live before use");
#else
				if (ctx->liverange[arg_reg] == NULL)
				{
					continue;
				}
#endif

				/* use */
				if (pos_cmp(instr->ra.pos, ctx->liverange[arg_reg]->to) == 0)
				{
					/* terminating use */
					bset_remove(ctx->live, arg_reg);
					ctx->n_live--;
					ctx->liverange[arg_reg] = ctx->liverange[arg_reg]->next;
				}
			}
		}

		if (instr->reg != -1)
		{
			/* def */
			assert(!ctx->liverange[instr->ra.vreg] && "must not be live before def");
			ctx->liverange[instr->ra.vreg] = ctx->func->ra.rinfo[instr->ra.vreg].liverange;
			assert(ctx->liverange[instr->ra.vreg] && "must be live after def");
			assert((instr->ra.vreg == 0 || pos_cmp(instr->ra.pos, ctx->liverange[instr->ra.vreg]->from) == 0) && "def must be start of liverange");
			bset_add(ctx->live, instr->ra.vreg);
			ctx->n_live++;
		}

		*n_live = ctx->n_live + ((ctx->skip_vreg != -1 && instr->reg == ctx->skip_vreg) ? -1 : 0);
		ctx->next_instr = instr->instr_next;
	}

	assert(bset_count(ctx->live) == ctx->n_live);

	return instr;
}

void
lifetime_tracker_get_live(lifetime_tracker_ctx *ctx, int *live)
{
	int i, idx = 0;

	for (i = CG_REG_VREG0; i < ctx->func->vreg_cntr; i++)
	{
		if (i != ctx->skip_vreg && bset_has(ctx->live, i))
		{
			live[idx++] = i;
		}
	}

	assert(ctx->n_live == idx || ctx->n_live == idx + 1);
}

void
lifetime_tracker_remove(lifetime_tracker_ctx *ctx, int var)
{
	if (bset_has(ctx->live, var))
	{
		bset_remove(ctx->live, var);
		ctx->n_live--;
	}
	ctx->liverange[var] = NULL;
}

void
lifetime_tracker_add_local(lifetime_tracker_ctx *ctx, cg_instr *instr)
{
	int var = instr->reg;
	/* the liverange is block-local only */
	assert(var < ctx->n_room_for);

	/* scan up to ctx->instr for variable and adjust n_live and live as appropriate */
	struct interval *p = ctx->func->ra.rinfo[var].liverange;

	if (instr->instr_next == ctx->next_instr)
	{
		ctx->next_instr = instr;
	}

	if (ctx->next_instr && pos_cmp(p->from, ctx->next_instr->ra.pos) < 0 && pos_cmp(ctx->next_instr->ra.pos, p->to) <= 0)
	{
		ctx->liverange[var] = p;
		bset_add(ctx->live, var);
		ctx->n_live++;
	}
}

/* TODO:FIXME: Split critical edges */
static struct pos
pos_make(int bpos, int ipos)
{
	struct pos p = {bpos, ipos};
	return p;
}

static int
pos_cmp(struct pos a, struct pos b)
{
	int bdiff = b.b - a.b;
	int idiff = b.i - a.i;

	return -(bdiff != 0 ? bdiff : idiff); /* TODO:FIXME: Update to strcmp convention */
}

#if DEBUG_SSA_RA
static void
pos_print(FILE *fp, struct pos p)
{
	fprintf(fp, "%d.", p.b);
	if (p.i == IPOS_NEGINF)
	{
		fprintf(fp, "-");
	}
	else if (p.i == IPOS_POSINF)
	{
		fprintf(fp, "+");
	}
	else
	{
		fprintf(fp, "%d", p.i);
	}
}
#endif

static void
grow_rinfo_as_needed(cg_func *f)
{
	if (f->ra.n_rinfo <= f->vreg_cntr)
	{
		int newsize = f->vreg_cntr + 1024;
		f->ra.rinfo = realloc(f->ra.rinfo, newsize*sizeof(f->ra.rinfo[0]));
		memset(&f->ra.rinfo[f->ra.n_rinfo], 0, (newsize - f->ra.n_rinfo)*sizeof(f->ra.rinfo[0]));
		f->ra.n_rinfo = newsize;
	}
}

#if DEBUG_SSA_RA
static void
range_print(FILE *fp, cg_func *func, unsigned v)
{
	interval *r = func->ra.rinfo[v].liverange;
	if (r != NULL)
	{
		fprintf(fp, "%%v%d: ", v);
		while (r != NULL)
		{
			fprintf(fp, "[");
			pos_print(fp, r->from);
			fprintf(fp, ",");
			pos_print(fp, r->to);
			fprintf(fp, ") ");
			r = r->next;
		}
		fprintf(fp, "\n");
	}
}
#endif

static interval *
range_add_interval(interval **range, struct pos from, struct pos to)
{
	interval *r, *prev;
	interval *p = *range;
	interval **pp = range;

	assert(pos_cmp(from, to) <= 0);

	/* Keep sorted by from */
	prev = NULL;
	while (p != NULL && pos_cmp(p->from, from) < 0)
	{
		pp = &p->next;
		prev = p;
		p = p->next;
	}

	assert(prev == NULL || pos_cmp(prev->from, from) <= 0);

	/* Insert new element */
	r = calloc(1, sizeof(interval));
	r->from = from;
	r->to = to;
	r->next = p;
	*pp = r;

	/* Want to merge overlaps and [a,+) [a+1,-) */
#define inf_adjacent(x,y) (((x).b+1 == (y).b) && x.i == IPOS_POSINF && y.i == IPOS_NEGINF)

	/* Try merging before */
	if (prev && (pos_cmp(prev->to, from) >= 0 || inf_adjacent(prev->to, from)))
	{
		if (pos_cmp(to, prev->to) > 0)
		{
			prev->to = to;
		}
		prev->next = r->next;
		r = prev;
	}

	/* Try merging after */
	p = r;
	while (p->next && (pos_cmp(p->to, p->next->from) >= 0 || inf_adjacent(p->to, p->next->from)))
	{
		if (pos_cmp(p->next->to, p->to) > 0)
		{
			p->to = p->next->to;
		}
		p->next = p->next->next;
	}

	return r;
}

static void
range_sub_interval(interval **dst, struct pos from, struct pos to)
{
	interval *p = *dst;
	interval **pp = dst;

	while (p && pos_cmp(p->to, from) <= 0)
	{
		pp = &p->next;
		p = p->next;
	}

	if (p)
	{
		assert(pos_cmp(p->to, from) > 0);

		if (pos_cmp(p->from, from) < 0 && pos_cmp(to, p->to) < 0)
		{
			/* Need to split interval {p} into {p,q} */
			struct interval *q = calloc(1, sizeof(*q));
			q->next = p->next;
			p->next = q;
			q->from = to;
			q->to = p->to;
			p->to = from;
		}
		else
		{
			/* Adjust start */
			if (pos_cmp(p->from, from) < 0)
			{
				p->to = from;
			}

			/* Eat entire intervals */
			while (p && pos_cmp(p->to, to) <= 0)
			{
				pp = &p->next;
				*pp = p = p->next;
			}

			/* Adjust end */
			if (pos_cmp(p->to, to) > 0)
			{
				p->from = to;
			}
		}
	}
}

static struct pos
pos_min(struct pos a, struct pos b)
{
	return pos_cmp(a, b) < 0 ? a : b;
}

static struct pos
pos_max(struct pos a, struct pos b)
{
	return pos_cmp(a, b) > 0 ? a : b;
}

#if 1
/* Perform union starting at offset and below (domtree po wise) */
void
range_add_sub_offset(cg_func *func, interval **dst, interval *src, struct pos offset, int add_not_sub)
{
	const unsigned offset_domtree_po = func->ra.rpo[offset.b]->ra.domtree_po;

	/* Advance to offset */
	while (src != NULL && pos_cmp(src->to, offset) < 0)
	{
		src = src->next;
	}

	while (src != NULL)
	{
		unsigned bidx;
		struct pos src_from = pos_max(src->from, offset);

		/* inside each interval go through the blocks that are contained
		   in that interval and union them if the domtree_po is less than
		   that of the original offset's */

		for (bidx = src_from.b; bidx < func->n_bbs; bidx++)
		{
			cg_bb *b = func->ra.rpo[bidx];

			if (pos_cmp(b->ra.ival_from, src->to) > 0)
			{
				break;
			}

			if (b->ra.domtree_po <= offset_domtree_po)
			{
				struct pos from = pos_max(src_from, b->ra.ival_from);
				struct pos to = pos_min(src->to, b->ra.ival_to);

				if (add_not_sub)
				{
					(void)range_add_interval(dst, from, to);
				}
				else
				{
					range_sub_interval(dst, from, to);
				}
			}
		}

		src = src->next;
	}
}
#endif

static void
range_union(interval **dst, interval *src)
{
	while (src != NULL)
	{
		(void)range_add_interval(dst, src->from, src->to);
		src = src->next;
	}
}

static int
range_test_intersect(interval *src0, interval *src1)
{
	while (src0 && src1)
	{
		while (src0 && src1 && pos_cmp(src0->from, src1->from) <= 0)
		{
			if (pos_cmp(src0->to, src1->from) > 0)
			{
				return 1;
			}
			src0 = src0->next;
		}
		while (src1 && src0 && pos_cmp(src1->from, src0->from) <= 0)
		{
			if (pos_cmp(src1->to, src0->from) > 0)
			{
				return 1;
			}
			src1 = src1->next;
		}
	}

	return 0;
}

static void
fill_rpo_rec(cg_func *func, cg_bb *b, unsigned *idx, graph_marker *marker)
{
	graph_edge *edge;

	/* The block order must guarantee that all predecessors
	   of a block are located before the block itself (this
	   implies that dominator blocks are located before the ones
	   they dominate). RPO is one such order while a pre-order walk
	   of the dominator tree is not (consider a diamond cfg). */

	if (graph_marker_set((graph_node *)b, marker))
	{
		return;
	}

	for (edge = graph_succ_first((graph_node *)b); edge != NULL; edge = graph_succ_next(edge))
	{
		cg_bb *succ = (cg_bb *)graph_edge_head(edge);
		fill_rpo_rec(func, succ, idx, marker);
	}

	b->ra.po = *idx;
	func->ra.rpo[func->n_bbs - (*idx) - 1] = b;
	(*idx)++;
}

static void
annotate_w_dtpo(cg_func *func, cg_bb *b, unsigned *cntr)
{
	struct cg_dom_info_lst *lst;

	for (lst = b->dom_info->domtree_children; lst; lst = lst->next)
	{
		annotate_w_dtpo(func, lst->info->bb, cntr);
	}

	b->ra.domtree_po = (*cntr)++;
}

static int
get_reg_for_arg(cg_instr *instr, unsigned arg_idx)
{
	int reg = -1;

	if (instr->args[arg_idx].kind == CG_INSTR_ARG_VREG)
	{
		cg_instr *arg = (cg_instr *)graph_edge_tail(instr->args[arg_idx].u.vreg);
		reg = arg->reg;
	}
	else if (instr->args[arg_idx].kind == CG_INSTR_ARG_HREG)
	{
		reg = instr->args[arg_idx].u.hreg;
	}

	return reg;
}

static void
do_phi_lifting(cg_func *func, cg_instr **phi_lift_movs, unsigned *n_phi_lift_movs)
{
	cg_bb *b;
	for (b = func->bb_first; b != NULL; b = b->bb_next)
	{
		cg_instr *phi;
		for (phi = b->instr_phi_first; phi != NULL; phi = phi->instr_next)
		{
			cg_instr *mov;
			cg_instr_phi_arg_iter it;
			cg_instr *args[func->n_bbs];
			cg_bb *arg_bbs[func->n_bbs];
			unsigned i, idx = 0;

			/* Insert copies for phi-args */
			cg_instr_phi_arg_iter_init(&it, phi);
			while ((args[idx] = cg_instr_phi_arg_iter_next(&it, &arg_bbs[idx])))
			{
				mov = cg_instr_build(arg_bbs[idx], CG_INSTR_OP_mov);
				grow_rinfo_as_needed(func);
				func->ra.rinfo[mov->reg].instr = mov;
				cg_instr_arg_set_vreg(mov, 0, args[idx]);
				cg_instr_link_last(mov);
				args[idx] = mov;
				idx++;
				assert(idx < func->n_bbs);
				phi_lift_movs[(*n_phi_lift_movs)++] = mov;
				assert(*n_phi_lift_movs < 1024);
			}
			graph_preds_delete(&func->vreg_graph_ctx, (graph_node *)phi);
			for (i = 0; i < idx; i++)
			{
				cg_instr_add_phi_arg(phi, arg_bbs[i], args[i]);
			}

			/* Insert copy for phi-result */
			mov = cg_instr_build(b, CG_INSTR_OP_mov);
			grow_rinfo_as_needed(func);
			func->ra.rinfo[mov->reg].instr = mov;
			cg_instr_replace_uses(phi, mov);
			cg_instr_arg_set_vreg(mov, 0, phi);
			cg_instr_link_first(mov);
			phi_lift_movs[(*n_phi_lift_movs)++] = mov;
		}
	}
}

static void
do_phi_mem_coalesce(cg_func *func, cg_instr **phi_lift_movs, unsigned n_phi_lift_movs)
{
	unsigned i;

	for (i = 0; i < n_phi_lift_movs; i++)
	{
		cg_instr *arg, *mov = phi_lift_movs[i];
		int eq_x, eq_y;
		assert(mov->op == CG_INSTR_OP_mov);
		/* x = mov y */
		eq_x = dset_find(func->ra.equiv_vreg, mov->reg);

		assert(mov->args[0].kind == CG_INSTR_ARG_VREG);
 		arg = (cg_instr *)graph_edge_tail(mov->args[0].u.vreg);
		eq_y = dset_find(func->ra.equiv_vreg, arg->reg);

		if (!range_test_intersect(func->ra.rinfo[eq_x].equiv_liverange, func->ra.rinfo[eq_y].equiv_liverange))
		{
			dset_union(func->ra.equiv_vreg, eq_x, eq_y);
			if (dset_find(func->ra.equiv_vreg, eq_x) == eq_x)
			{
				range_union(&func->ra.rinfo[eq_x].equiv_liverange, func->ra.rinfo[eq_y].liverange);
			}
			else
			{
				assert(dset_find(func->ra.equiv_vreg, eq_x) == eq_y);
				range_union(&func->ra.rinfo[eq_y].equiv_liverange, func->ra.rinfo[eq_x].liverange);
			}
			range_union(&func->ra.rinfo[arg->reg].liverange, func->ra.rinfo[mov->reg].liverange);
			cg_instr_replace_uses(mov, arg);
			func->ra.rinfo[mov->reg].liverange = NULL;
			graph_preds_delete(&func->vreg_graph_ctx, (graph_node *)mov);
			cg_instr_unlink(mov);
		}
	}
}

/* Given a natural loop defined by its header and back-edge tail, propagate liveness to all blocks */
static void
propagate_live_in_loop_body(cg_bb *header, cg_bb *tail, bset_set *live)
{
	cg_func *func = header->func;
	cg_bb *stack[header->func->n_bbs];
	unsigned si = 0;
	graph_marker marker;

	graph_marker_alloc(&func->cfg_graph_ctx, &marker);

	stack[si++] = tail;

	while (si > 0)
	{
		cg_bb *b = stack[--si];
		graph_edge *edge;
		unsigned i;

		graph_marker_set((graph_node *)b, &marker);

		for (i = 0; i < func->vreg_cntr; i++)
		{
			if (bset_has(live, i))
			{
				range_add_interval(&func->ra.rinfo[i].liverange, b->ra.ival_from, b->ra.ival_to);
				if (header != b)
				{
					bset_add(b->ra.livein, i);
				}
			}
		}

		if (b != header)
		{
			for (edge = graph_pred_first((graph_node *)b); edge != NULL; edge = graph_pred_next(edge))
			{
				cg_bb *pred = (cg_bb *)graph_edge_tail(edge);
				if (!graph_marker_is_set((graph_node *)pred, &marker))
				{
					stack[si++] = pred;
				}
			}
		}
	}

	graph_marker_free(&func->cfg_graph_ctx, &marker);
}

static void
do_lifetime_intervals(cg_func *func)
{
	unsigned i;
	int bpos = func->n_bbs - 1;
	bset_set *live = bset_create_set(func->vreg_cntr);
	struct interval *curr_ivals[func->vreg_cntr];

	memset(curr_ivals, 0, sizeof(curr_ivals));


	for (i = 0; i < func->n_bbs; i++)
	{
		int j, l, bidx = func->n_bbs - i - 1;
		cg_bb *b = func->ra.rpo[bidx];
		cg_instr *instr;
		graph_edge *edge;
		int ipos = b->n_instrs*IPOS_SPACING;

		bset_clear(live);

		for (edge = graph_succ_first((graph_node *)b); edge != NULL; edge = graph_succ_next(edge))
		{
			cg_bb *succ = (cg_bb *)graph_edge_head(edge);
			cg_instr *phi;

			if (succ->ra.livein)
			{
				bset_union(live, succ->ra.livein);
			}

			for (phi = succ->instr_phi_first; phi != NULL; phi = phi->instr_next)
			{
				bset_add(live, cg_instr_phi_input_of(phi, b)->reg);
			}
		}

		for (j = 0; j < func->vreg_cntr; j++) /* TODO:FIXME: Make quicker */
		{
			if (bset_has(live, j))
			{
				/* variable in liveout[b] */
				curr_ivals[j] = range_add_interval(&func->ra.rinfo[j].liverange, pos_make(bpos, IPOS_NEGINF), pos_make(bpos, IPOS_POSINF));
			}
		}

		b->ra.ival_to = pos_make(bpos, IPOS_POSINF);
		for (instr = b->instr_last; instr != NULL; instr = instr->instr_prev)
		{
			assert(instr->op != CG_INSTR_OP_phi);

			ipos -= IPOS_SPACING;

			instr->ra.pos = pos_make(bpos, ipos);

			if (instr->reg != -1)
			{
				curr_ivals[instr->reg]->from = pos_make(bpos, ipos);
				bset_remove(live, instr->reg);
				func->ra.rinfo[instr->reg].instr = instr;
			}

			for (l = 0; l < CG_INSTR_N_ARGS; l++)
			{
				int reg = get_reg_for_arg(instr, l);
				if (reg != -1 && !bset_has(live, reg))
				{
					curr_ivals[reg] = range_add_interval(&func->ra.rinfo[reg].liverange, pos_make(bpos, IPOS_NEGINF), pos_make(bpos, ipos));
					bset_add(live, reg);
				}
			}
		}

		b->ra.ival_from = pos_make(bpos, IPOS_NEGINF);

		for (instr = b->instr_phi_last; instr != NULL; instr = instr->instr_prev)
		{
			assert(instr->op == CG_INSTR_OP_phi);
			bset_remove(live, instr->reg);
			func->ra.rinfo[instr->reg].instr = instr;
		}

		/* If b is a loop header propagate live to all blocks */
		for (edge = graph_pred_first((graph_node *)b); edge != NULL; edge = graph_pred_next(edge))
		{
			cg_bb *pred = (cg_bb *)graph_edge_tail(edge);
			if (pred->ra.po < b->ra.po)
			{
				propagate_live_in_loop_body(b, pred, live);
			}
		}

		b->ra.livein = bset_create_set(func->vreg_cntr);
		bset_copy(b->ra.livein, live);
#if 0
		printf("bb%d: ", b->id);
		bset_print(stdout, b->ra.livein);
		printf("\n");
#endif
		bpos--;
	}

	for (i = 0; i < N_ARRAY_SIZE(func->args) && func->args[i]; i++)
	{
		cg_instr *arg = func->args[i];
		func->ra.rinfo[arg->reg].instr = arg;
	}
}

#if DEBUG_SSA_RA
static void
debug_print_ra_fp(FILE *fp, cg_func *func, int print_intervals)
{
	lifetime_tracker_ctx lctx;
	unsigned i;

	lifetime_tracker_init(&lctx, func);

	for (i = 0; i < func->n_bbs; i++)
	{
		cg_bb *b = func->ra.rpo[i];
		cg_instr *instr;
		graph_edge *edge;

		if (print_intervals)
		{
			lifetime_tracker_start(&lctx, b);
		}

		fprintf(fp, "    bb%d:\n", b->id);

		for (instr = b->instr_phi_first; instr != NULL; instr = instr->instr_next)
		{
			fprintf(fp, "      ");
			cg_print_instr(fp, instr);
			if (instr->ra.dbg_spill_id != -1)
			{
				int equiv = dset_find(func->ra.equiv_spill_id, instr->ra.dbg_spill_id);
				fprintf(fp, " [spill_id: %d]", equiv);
			}
			fprintf(fp, "\n");
		}
		for (instr = b->instr_first; instr != NULL; instr = instr->instr_next)
		{
			if (print_intervals)
			{
				fprintf(fp, "%d.%d:  ", instr->ra.pos.b, instr->ra.pos.i);
			}
			else
			{
				fprintf(fp, "           ");
			}
			cg_print_instr(fp, instr);
			if (instr->ra.dbg_spill_id != -1)
			{
				int equiv = dset_find(func->ra.equiv_spill_id, instr->ra.dbg_spill_id);
				fprintf(fp, " [spill_id: %d]", equiv);
			}
			if (print_intervals)
			{
				int live[func->vreg_cntr];
				unsigned n_live;
				cg_instr *tmp;

				tmp = lifetime_tracker_next(&lctx, &n_live);
				assert(tmp == instr);
				fprintf(fp, " { ");
				lifetime_tracker_get_live(&lctx, live);
				int i;
				for (i = 0; i < n_live; i++)
				{
					fprintf(fp, "%%v%d ", live[i]);
				}
				fprintf(fp, "}#%d", n_live);
			}
			fprintf(fp, "\n");
		}

		fprintf(fp, "      [ ");
		for (edge = graph_succ_first((graph_node *)b); edge != NULL; edge = graph_succ_next(edge))
		{
			cg_bb *t = (cg_bb *)graph_edge_head(edge);
			fprintf(fp, "%%bb%d ", t->id);
		}
		fprintf(fp, "]\n");
	}

	if (print_intervals)
	{
		fprintf(fp, "\nLifetime intervals:\n");
		for (i = 0; i < func->vreg_cntr; i++)
		{
			range_print(fp, func, i);
		}
	}
}

static void
debug_print_ra(cg_func *func, const char *str, int print_intervals)
{
	char path[128];
	snprintf(path, sizeof(path), "ssa_ra_%s_%s.txt", func->name, str);
	FILE *fp = fopen(path, "w");
	debug_print_ra_fp(fp, func, print_intervals);
	fclose(fp);
}
#endif

void
do_phi_analysis(cg_func *func)
{
	unsigned i;
	cg_bb *bb;

	func->ra.equiv_vreg = dset_create_universe(func->vreg_cntr);

	for (i = 0; i < func->vreg_cntr; i++)
	{
		dset_makeset(func->ra.equiv_vreg, i);
	}

	for (bb = func->bb_first; bb != NULL; bb = bb->bb_next)
	{
		cg_instr *phi;
		for (phi = bb->instr_phi_first; phi != NULL; phi = phi->instr_next)
		{
			cg_instr_phi_arg_iter pit;
			cg_instr *parg;
			cg_instr_phi_arg_iter_init(&pit, phi);

			while ((parg = cg_instr_phi_arg_iter_next(&pit, NULL)))
			{
				dset_union(func->ra.equiv_vreg, phi->reg, parg->reg);
			}
		}
	}

	for (i = 0; i < func->vreg_cntr; i++)
	{
		int eq = dset_find(func->ra.equiv_vreg, i);
		range_union(&func->ra.rinfo[eq].equiv_liverange, func->ra.rinfo[i].liverange);
	}
}

struct pref_pair {
	int points;
	int reg;
};

static int
pref_cmp(const void *a, const void *b)
{
	struct pref_pair *pa = (struct pref_pair *)a;
	struct pref_pair *pb = (struct pref_pair *)b;

	/* Note that we have reversed the comparison */
	if (pa->points < pb->points)
	{
		return 1;
	}
	else if (pa->points > pb->points)
	{
		return -1;
	}

	return 0;
}

static void
compute_preference(cg_instr *instr, int *preforder)
{
	graph_edge *edge;
	struct pref_pair score[CG_REG_VREG0];
	int i;

	for (i = 0; i < CG_REG_VREG0; i++)
	{
		score[i].points = 0;
		score[i].reg = i;
	}

	if (instr->op == CG_INSTR_OP_mov &&
	    instr->args[0].kind == CG_INSTR_ARG_HREG)
	{
		/* Pre-colored def */
		score[instr->args[0].u.hreg].points++;
	}
	else if (instr->op == CG_INSTR_OP_call && instr->reg != -1)
	{
		/* calling conventions return value in r0 */
		score[0].points++;
	}
	else if (instr->op == CG_INSTR_OP_phi)
	{
		cg_instr_phi_arg_iter it;
		cg_instr *arg;
		cg_bb *argbb;
		cg_instr_phi_arg_iter_init(&it, instr);
		while ((arg = cg_instr_phi_arg_iter_next(&it, &argbb)))
		{
			if (arg->reg < CG_REG_VREG0)
			{
				/* Phi-argument */
				score[arg->reg].points++;
			}
		}
	}

	for (edge = graph_succ_first((graph_node *)instr); edge != NULL; edge = graph_succ_next(edge))
	{
		cg_instr *use = (cg_instr *)graph_edge_head(edge);
		if (use->op == CG_INSTR_OP_call)
		{
			/* Pre-colored use */
			unsigned i;
			for (i = 0; i < CG_INSTR_N_ARGS; i++)
			{
				if (use->args[i].kind == CG_INSTR_ARG_VREG &&
				    use->args[i].u.vreg == edge)
				{
					score[i-1].points++;
				}
			}
		}
		else if (use->op == CG_INSTR_OP_mov &&
		    use->reg < CG_REG_VREG0)
		{
			/* Pre-colored use */
			score[use->reg].points++;
		}
		else if (use->op == CG_INSTR_OP_phi &&
		         use->reg < CG_REG_VREG0)
		{
			/* Phi-instruction */
			score[use->reg].points++;
		}
	}

	qsort(score, CG_REG_VREG0, sizeof(score[0]), pref_cmp);
	for (i = 0; i < CG_REG_VREG0; i++)
	{
		preforder[i] = score[i].reg;
	}
}


static unsigned
compute_spill_cost(cg_instr *instr)
{
	graph_edge *edge;
	unsigned cost;

	cost = 1 + cg_bb_loop_nest(instr->bb)*10;
	for (edge = graph_succ_first((graph_node *)instr); edge != NULL; edge = graph_succ_next(edge))
	{
		cg_instr *use = (cg_instr *)graph_edge_head(edge);
		cost += 1 + cg_bb_loop_nest(use->bb)*10;
	}

	return cost;
}

static int
select_virtual_to_spill(cg_func *f, int *live_virtuals, unsigned n_live, struct pos pos)
{
	unsigned i;
	unsigned min_cost = UINT_MAX;
	int min_virtual = -1;
	for (i = 0; i < n_live; i++)
	{
		int v = live_virtuals[i];
		if (v >= CG_REG_VREG0 && pos_cmp(f->ra.rinfo[v].liverange->from, pos) < 0)
		{
			unsigned cost = compute_spill_cost(f->ra.rinfo[v].instr);
			if (cost < min_cost)
			{
				min_cost = cost;
				min_virtual = v;
			}
		}
	}

	assert(min_virtual != -1);
	return min_virtual;
}

static void
do_select_spill(cg_func *func, unsigned max_regs)
{
	int live[func->vreg_cntr];
	int i, bix, curr_spill_id;
	struct {
		cg_instr *use;
		int idx;
		cg_instr *reload;
	} delayed_args[1024];
	int delayed_args_idx;
	lifetime_tracker_ctx lctx;

	lifetime_tracker_init(&lctx, func);

	curr_spill_id = 0;
	for (bix = 0; bix < func->n_bbs; bix++)
	{
		cg_bb *b = func->ra.rpo[bix];
		cg_instr *instr;

		lifetime_tracker_start(&lctx, b);
		for (instr = b->instr_first; instr != NULL; instr = instr->instr_next)
		{
			unsigned n_live;
			cg_instr *tmp;

			unsigned n_clobber = (instr->op == CG_INSTR_OP_call) ? 4 : 0;
			lctx.skip_vreg = (instr->op == CG_INSTR_OP_call) ? instr->reg : -1;

			tmp = lifetime_tracker_next(&lctx, &n_live);
			assert(tmp == instr);

			while (n_live + n_clobber > max_regs)
			{
				/* Choose virtual to spill. */
				lifetime_tracker_get_live(&lctx, live);
				int spillv = select_virtual_to_spill(func, live, n_live--, instr->ra.pos);
				cg_instr *spilli = func->ra.rinfo[spillv].instr;
				assert(spilli != instr);
				assert(spilli->op != CG_INSTR_OP_reload);

				if (spilli->op == CG_INSTR_OP_phi)
				{
					/* Remove entire range. */
					func->ra.rinfo[spillv].liverange = NULL;
					spilli->ra.dbg_spill_id = spilli->ra.spill_id = curr_spill_id;
				}
				else
				{
					cg_instr *spill;

					if (spilli->op == CG_INSTR_OP_arg)
					{
						/* Shrink range for arg to end at start of entry block */
						spill = cg_instr_build(func->ra.rpo[0], CG_INSTR_OP_spill);
						grow_rinfo_as_needed(func);
						spill->ra.pos = pos_make(0, 0);
						func->ra.rinfo[spillv].liverange->to = spill->ra.pos;
						func->ra.rinfo[spillv].liverange->next = NULL;
						cg_instr_link_first(spill);
					}
					else
					{
						/* Shrink range to single minimal interval that only contains definition. */
						spill = cg_instr_build(spilli->bb, CG_INSTR_OP_spill);
						grow_rinfo_as_needed(func);
						spill->ra.pos = pos_make(spilli->ra.pos.b, spilli->ra.pos.i + 1);
						func->ra.rinfo[spillv].liverange->to = spill->ra.pos;
						func->ra.rinfo[spillv].liverange->next = NULL;
						cg_instr_link_after(spilli, spill);
					}

					spill->reg = spill->ra.curr_reg = -1;
					cg_instr_arg_set_vreg(spill, 0, spilli);
					spill->ra.dbg_spill_id = spill->ra.spill_id = curr_spill_id;
					spilli->ra.dbg_spill_id = spilli->ra.spill_id = curr_spill_id;
				}

				lifetime_tracker_remove(&lctx, spillv);

				/* Insert short virtuals for reloads immediately before all uses */
				graph_edge *edge, *next_edge;
				delayed_args_idx = 0;
				graph_marker reload_marker; /* set if use instruction already has a reload for this spill */
				graph_marker_alloc(&func->vreg_graph_ctx, &reload_marker);
				for (edge = graph_succ_first((graph_node *)spilli); edge != NULL; edge = next_edge)
				{
					next_edge = graph_succ_next(edge);
					cg_instr *use = (cg_instr *)graph_edge_head(edge);

					/* We should not insert reloads for phi-uses since they
					   will be handled in the ssa_deconstruciton phase. */
					if (use->op != CG_INSTR_OP_phi && use->op != CG_INSTR_OP_spill &&
					    !graph_marker_set((graph_node *)use, &reload_marker))
					{
						cg_instr *reload = cg_instr_build(use->bb, CG_INSTR_OP_reload);
						grow_rinfo_as_needed(func);
						cg_instr_link_before(use, reload);
						func->ra.rinfo[reload->reg].instr = reload;
						reload->ra.dbg_spill_id = reload->ra.spill_id = curr_spill_id;

						struct pos from = pos_make(use->ra.pos.b, use->ra.pos.i - 1);

						range_add_interval(&func->ra.rinfo[reload->reg].liverange, from, use->ra.pos);
						reload->ra.pos = from;
						lifetime_tracker_add_local(&lctx, reload);

						for (i = 0; i < CG_INSTR_N_ARGS; i++)
						{
							if (use->args[i].kind == CG_INSTR_ARG_VREG &&
							    spilli == (cg_instr *)graph_edge_tail(use->args[i].u.vreg))
							{
								/* Delay update of use arguments since we are currently iterating over uses */
								delayed_args[delayed_args_idx].use = use;
								delayed_args[delayed_args_idx].idx = i;
								delayed_args[delayed_args_idx].reload = reload;
								delayed_args_idx++;
							}
						}
					}
				}
				graph_marker_free(&func->vreg_graph_ctx, &reload_marker);

				for (i = 0; i < delayed_args_idx; i++)
				{
					cg_instr_arg_set_vreg(delayed_args[i].use, delayed_args[i].idx, delayed_args[i].reload);
				}

				curr_spill_id++;
			}
		}
	}

	/* Now do a pass over all phi-nodes and do a UNION/FIND of spill_ids */
	cg_instr *phi;
	cg_bb *b;

	func->ra.equiv_spill_id = dset_create_universe(curr_spill_id);
	for (i = 0; i < curr_spill_id; i++)
	{
		dset_makeset(func->ra.equiv_spill_id, i);
	}

	for (b = func->bb_first; b != NULL; b = b->bb_next)
	{
		for (phi = b->instr_phi_first; phi != NULL; phi = phi->instr_next)
		{
			if (phi->ra.spill_id != -1)
			{
				int phi_equiv = dset_find(func->ra.equiv_spill_id, phi->ra.spill_id);
				cg_instr_phi_arg_iter it;
				cg_instr *arg;

				cg_instr_phi_arg_iter_init(&it, phi);
				while ((arg = cg_instr_phi_arg_iter_next(&it, NULL)))
				{
					if (arg->ra.spill_id != -1)
					{
						int arg_equiv = dset_find(func->ra.equiv_spill_id, arg->ra.spill_id);
						dset_union(func->ra.equiv_spill_id, phi_equiv, arg_equiv);
					}
				}
			}
		}
	}

	/* Allocate stack offsets for spill_ids */
	int size = curr_spill_id * sizeof(func->ra.spill_slot_offsets[0]);
	func->ra.spill_slot_offsets = malloc(size);
	memset(func->ra.spill_slot_offsets, -1, size);
	func->ra.n_spill_slots = 0;
	for (i = 0; i < curr_spill_id; i++)
	{
		int eq_idx = dset_find(func->ra.equiv_spill_id, i);
		if (func->ra.spill_slot_offsets[eq_idx] == -1)
		{
			func->ra.spill_slot_offsets[eq_idx] = func->ra.n_spill_slots++;
		}
	}
}

static void
assign_color(cg_func *func, cg_instr *instr, unsigned max_regs)
{
	int preforder[CG_REG_VREG0];
	unsigned i;

	compute_preference(instr, preforder);

	for (i = 0; i < max_regs; i++)
	{
		unsigned r = preforder[i];

		if (!range_test_intersect(func->ra.rinfo[r].liverange, func->ra.rinfo[instr->reg].liverange))
		{
			/* Reserve physical register for lifetime of virtual */
			range_union(&func->ra.rinfo[r].liverange, func->ra.rinfo[instr->reg].liverange);
			/* Update instruction to use physical register */
			instr->reg = instr->ra.curr_reg = r;
			return;
		}
	}
	assert(0 && "At this point we expect to find a color");
}

static void
gather_pre_call_post_regs(cg_func *func,
                          lifetime_tracker_ctx *lctx,
                          unsigned max_regs,
                          unsigned n_live,
                          cg_instr *instr,
                          cg_instr **pre_regs,
                          cg_instr **call_regs,
                          cg_instr **post_regs)
{
	int live[CG_REG_VREG0];
	cg_instr *live2[CG_REG_VREG0];
	unsigned i, n_live2 = 0;

	/* Get current and target register contents */
	lifetime_tracker_get_live(lctx, live);

	assert(instr->op == CG_INSTR_OP_call);

	for (i = 0; i < n_live; i++)
	{
		cg_instr *liveinstr = func->ra.rinfo[live[i]].instr;
		/* skip output of constrained instruction */
		if (liveinstr != instr)
		{
			pre_regs[liveinstr->ra.curr_reg] = liveinstr;
			post_regs[liveinstr->ra.curr_reg] = liveinstr;

			if (liveinstr->ra.curr_reg < 4)
			{
				live2[n_live2++] = liveinstr;
			}
			else
			{
				/* already in preserved register */
				call_regs[liveinstr->ra.curr_reg] = liveinstr;
			}
		}
	}
	for (i = 0; i < CG_INSTR_N_ARGS; i++)
	{
		if (instr->args[i].kind == CG_INSTR_ARG_VREG)
		{
			cg_instr *arg = (cg_instr *)graph_edge_tail(instr->args[i].u.vreg);
			assert(arg->ra.curr_reg >= 0 && arg->ra.curr_reg < CG_REG_VREG0);
			pre_regs[arg->ra.curr_reg] = arg;
			call_regs[i-1] = arg;
		}
	}
	for (i = 4; n_live2 > 0; i++)
	{
		assert(i < max_regs);
		if (call_regs[i] == NULL)
		{
			call_regs[i] = live2[--n_live2];
		}
	}
}

#if DEBUG_SSA_RA
static void
debug_regs(FILE *fp, char *str, cg_instr **regs, unsigned max_regs)
{
	unsigned i;
	fprintf(fp, "%s = [ ", str);
	for (i = 0; i < max_regs; i++)
	{
		cg_instr *tmp = regs[i];
		if (tmp)
		{
			fprintf(fp, "%%v%d ", tmp->ra.vreg);
		}
		else
		{
			fprintf(fp, "null ");
		}
	}
	fprintf(fp, "]\n");
}
#endif

static void
move_swap_current2target(cg_bb *b, cg_instr *instr, cg_instr **current_regs, cg_instr **target_regs)
{
	cg_func *func = b->func;
	int keep_going;
	int i;

	for (i = 0; i < CG_REG_VREG0; i++)
	{
		assert(!current_regs[i] || current_regs[i]->ra.curr_reg == i);
	}

	/* Insert moves */
	keep_going = 1;
	while (keep_going)
	{
		int i;
		keep_going = 0;
		for (i = 0; i < CG_REG_VREG0; i++)
		{
			if (current_regs[i] != target_regs[i] && !current_regs[i])
			{
				/* src -> dst */
				const int src_reg = target_regs[i]->ra.curr_reg;
				const int dst_reg = i;
				cg_instr *mov = cg_instr_build(b, CG_INSTR_OP_mov);
				grow_rinfo_as_needed(func);
				mov->args[0].kind = CG_INSTR_ARG_HREG;
				mov->args[0].u.hreg = src_reg;
				mov->reg = mov->ra.curr_reg = dst_reg;
				if (instr)
				{
					cg_instr_link_before(instr, mov);
				}
				else
				{
					cg_instr_link_last(mov);
				}

				current_regs[dst_reg] = current_regs[src_reg];
				current_regs[dst_reg]->ra.curr_reg = dst_reg;

				if (current_regs[src_reg] != target_regs[src_reg])
				{
					current_regs[src_reg] = NULL;
				}

				keep_going = 1;
			}
		}
	}

	/* Insert swaps */
	keep_going = 1;
	while (keep_going)
	{
		int i;
		keep_going = 0;
		for (i = 0; i < CG_REG_VREG0; i++)
		{
			if (current_regs[i] != target_regs[i])
			{
				assert(current_regs[i]->ra.curr_reg == i);
				/* src <-> dst */
				const int src_reg = target_regs[i]->ra.curr_reg;
				const int dst_reg = i;
				insert_single_swap(b, instr, current_regs[dst_reg], current_regs[src_reg]);
				grow_rinfo_as_needed(func);

				cg_instr *tmp = current_regs[dst_reg];
				current_regs[dst_reg] = current_regs[src_reg];
				current_regs[src_reg] = tmp;

				const int tmpi = current_regs[dst_reg]->ra.curr_reg;
				current_regs[dst_reg]->ra.curr_reg = current_regs[src_reg]->ra.curr_reg;
				current_regs[src_reg]->ra.curr_reg = tmpi;

				keep_going = 1;
			}
		}
	}

	for (i = 0; i < CG_REG_VREG0; i++)
	{
		assert(current_regs[i] == target_regs[i]);
		if (current_regs[i])
		{
			current_regs[i]->ra.curr_reg = i;
		}
	}
}

static void
do_color_assignment(cg_func *func, unsigned max_regs)
{
	unsigned bix;
	unsigned i;
	lifetime_tracker_ctx lctx;

	lifetime_tracker_init(&lctx, func);

	/* Assign registers to incomming parameters */
	for (i = 0; i < N_ARRAY_SIZE(func->args) && func->args[i]; i++)
	{
		cg_instr *arg = func->args[i];
		assert(arg->op == CG_INSTR_OP_arg);
		range_union(&func->ra.rinfo[i].liverange, func->ra.rinfo[arg->reg].liverange);
		arg->reg = arg->ra.curr_reg = i;
	}

	for (bix = 0; bix < func->n_bbs; bix++)
	{
		cg_bb *b = func->ra.rpo[bix];
		cg_instr *instr, *instr_next;

		lifetime_tracker_start(&lctx, b);

		for (instr = b->instr_phi_first; instr != NULL; instr = instr->instr_next)
		{
			assert(instr->reg >= CG_REG_VREG0);
			assign_color(func, instr, max_regs);
		}

		for (instr = b->instr_first; instr != NULL; instr = instr_next)
		{
			cg_instr *tmp;
			unsigned n_live;
			assert(instr->reg == -1 || instr->reg >= CG_REG_VREG0);
			tmp = lifetime_tracker_next(&lctx, &n_live);
			assert(tmp == instr);
			assert(n_live <= max_regs && "Spilling has not lowered register usage to max_regs");

			instr_next = instr->instr_next;

			cg_instr *pre_regs[CG_REG_VREG0];
			cg_instr *call_regs[CG_REG_VREG0];
			cg_instr *post_regs[CG_REG_VREG0];

			memset(pre_regs, 0, sizeof(pre_regs));
			memset(call_regs, 0, sizeof(call_regs));
			memset(post_regs, 0, sizeof(post_regs));

			/* Apply permutations to handle inputs of constrained instructions */
			if (instr->op == CG_INSTR_OP_call)
			{
				gather_pre_call_post_regs(func, &lctx, max_regs, n_live, instr, pre_regs, call_regs, post_regs);

				D(fprintf(stdout, "%s: ", func->name));
				D(cg_print_instr(stdout, instr));
				D(fprintf(stdout, "\n"));
				D(debug_regs(stdout, "pre  ", pre_regs, max_regs));
				D(debug_regs(stdout, "call ", call_regs, max_regs));

				move_swap_current2target(b, instr, pre_regs, call_regs);
			}
			else if (instr->op == CG_INSTR_OP_ret)
			{
				cg_instr *arg = (cg_instr *)graph_edge_tail(instr->args[0].u.vreg);
				assert(arg->ra.curr_reg >= 0 && arg->ra.curr_reg < CG_REG_VREG0);
				pre_regs[arg->ra.curr_reg] = arg;
				call_regs[0] = arg;

				D(fprintf(stdout, "%s: ", func->name));
				D(cg_print_instr(stdout, instr));
				D(fprintf(stdout, "\n"));
				D(debug_regs(stdout, "pre  ", pre_regs, max_regs));
				D(debug_regs(stdout, "ret  ", call_regs, max_regs));

				move_swap_current2target(b, instr, pre_regs, call_regs);
				/* For consistency we need to undo the arg->ra.curr_reg change */
				arg->ra.curr_reg = arg->reg;
			}

			/* Harden arguments */
			{
				int j;
				for (j = 0; j < CG_INSTR_N_ARGS; j++)
				{
					if (instr->args[j].kind == CG_INSTR_ARG_VREG)
					{
						cg_instr *arg = (cg_instr *)graph_edge_tail(instr->args[j].u.vreg);
						assert(arg->reg < CG_REG_VREG0);

						instr->args[j].kind = CG_INSTR_ARG_HREG;
						instr->args[j].u.hreg = arg->reg;
					}
				}
			}

			if (instr->reg != -1)
			{
				assign_color(func, instr, max_regs);
			}

			if (instr->op == CG_INSTR_OP_call)
			{
				int i;
				for (i = 0; i < 4; i++)
				{
					if (call_regs[i])
					{
						call_regs[i] = NULL;
					}
				}

				if (instr->reg != -1)
				{
					instr->ra.curr_reg = 0;
					call_regs[0] = instr;
					assert(!post_regs[instr->reg]);
					post_regs[instr->reg] = instr;
				}

				D(debug_regs(stdout, "call ", call_regs, max_regs));
				D(debug_regs(stdout, "post ", post_regs, max_regs));
				move_swap_current2target(b, instr->instr_next, call_regs, post_regs);
			}
		}
	}
}

static int
queue_has(cg_instr **q, unsigned qsize, cg_instr *p)
{
	unsigned i;
	for (i = 0; i < qsize; i++)
	{
		if (q[i] && q[i]->ra.spill_id == -1 && q[i]->reg == p->reg)
		{
			return 1;
		}
	}
	return 0;
}

static void
insert_safe_copies(cg_bb *b, cg_instr **src, cg_instr **dst, unsigned size)
{
	cg_func *func = b->func;
	unsigned i;
	int keep_going = 1;

	while (keep_going)
	{
		keep_going = 0;
		for (i = 0; i < size; i++)
		{
			if (dst[i] == NULL)
			{
				assert(src[i] == NULL);
				continue;
			}
			assert(src[i] != NULL);

			int dst_eq_spill_id = dst[i]->ra.spill_id != -1 ? dset_find(func->ra.equiv_spill_id, dst[i]->ra.spill_id) : -1;
			int src_eq_spill_id = src[i]->ra.spill_id != -1 ? dset_find(func->ra.equiv_spill_id, src[i]->ra.spill_id) : -1;

			if (dst_eq_spill_id != -1 && src_eq_spill_id == -1)
			{
				/* Insert store */
				int offset = func->ra.spill_slot_offsets[dst_eq_spill_id];
				cg_instr *store = cg_instr_build(b, CG_INSTR_OP_str);
				grow_rinfo_as_needed(func);
				store->reg = -1; /* No output */
				store->args[0].kind = CG_INSTR_ARG_HREG;
				store->args[0].u.hreg = src[i]->reg;
				store->args[1].kind = CG_INSTR_ARG_HREG;
				store->args[1].u.hreg = CG_REG_sp;
				store->args[1].offset = func->stack_frame_size + offset * 4;
				cg_instr_link_last(store);
				store->ra.dbg_spill_id = dst_eq_spill_id; /* For debug use only */
				/* Mark as done */
				src[i] = NULL;
				dst[i] = NULL;
				keep_going = 1;
			}
			else if (dst_eq_spill_id == -1 && src_eq_spill_id != -1)
			{
				if (!queue_has(src, size, dst[i]))
				{
					/* Insert load */
					int offset = func->ra.spill_slot_offsets[src_eq_spill_id];
					cg_instr *load = cg_instr_build(b, CG_INSTR_OP_ldr);
					grow_rinfo_as_needed(func);
					load->reg = dst[i]->reg;
					load->args[0].kind = CG_INSTR_ARG_HREG;
					load->args[0].u.hreg = CG_REG_sp;
					load->args[0].offset = func->stack_frame_size + offset * 4;
					cg_instr_link_last(load);
					load->ra.dbg_spill_id = src_eq_spill_id; /* For debug use only */
					/* Mark as done */
					src[i] = NULL;
					dst[i] = NULL;
					keep_going = 1;
				}
			}
			else if (dst_eq_spill_id != -1 && src_eq_spill_id != -1)
			{
				/* Do nothing, already same mem */
				assert(dst_eq_spill_id == src_eq_spill_id);
				/* Mark as done */
				src[i] = NULL;
				dst[i] = NULL;
				keep_going = 1;
			}
			else if (dst[i]->reg == src[i]->reg)
			{
				/* Do nothing but mark as done */
				src[i] = NULL;
				dst[i] = NULL;
				keep_going = 1;
			}
			else if (!queue_has(src, size, dst[i]))
			{
				/* Insert copy */
				cg_instr *mov = cg_instr_build(b, CG_INSTR_OP_mov);
				grow_rinfo_as_needed(func);
				mov->args[0].kind = CG_INSTR_ARG_HREG;
				mov->args[0].u.hreg = src[i]->reg;
				mov->reg = dst[i]->reg;
				cg_instr_link_last(mov);
				/* Mark as done */
				src[i] = NULL;
				dst[i] = NULL;
				keep_going = 1;
			}
		}
	}
}

static void
insert_single_swap(cg_bb *b, cg_instr *before, cg_instr *x, cg_instr *y)
{
	cg_instr *xor0 = cg_instr_build(b, CG_INSTR_OP_eor);
	cg_instr *xor1 = cg_instr_build(b, CG_INSTR_OP_eor);
	cg_instr *xor2 = cg_instr_build(b, CG_INSTR_OP_eor);

	xor0->args[0].kind = CG_INSTR_ARG_HREG;
	xor0->args[0].u.hreg = x->ra.curr_reg;
	xor0->args[1].kind = CG_INSTR_ARG_HREG;
	xor0->args[1].u.hreg = y->ra.curr_reg;
	xor0->reg = xor0->ra.curr_reg = x->ra.curr_reg;

	xor1->args[0].kind = CG_INSTR_ARG_HREG;
	xor1->args[0].u.hreg = x->ra.curr_reg;
	xor1->args[1].kind = CG_INSTR_ARG_HREG;
	xor1->args[1].u.hreg = y->ra.curr_reg;
	xor1->reg = xor1->ra.curr_reg = y->ra.curr_reg;

	xor2->args[0].kind = CG_INSTR_ARG_HREG;
	xor2->args[0].u.hreg = x->ra.curr_reg;
	xor2->args[1].kind = CG_INSTR_ARG_HREG;
	xor2->args[1].u.hreg = y->ra.curr_reg;
	xor2->reg = xor2->ra.curr_reg = x->ra.curr_reg;

	if (before)
	{
		cg_instr_link_before(before, xor0);
		cg_instr_link_before(before, xor1);
		cg_instr_link_before(before, xor2);
	}
	else
	{
		cg_instr_link_last(xor0);
		cg_instr_link_last(xor1);
		cg_instr_link_last(xor2);
	}
}

static void
insert_swaps(cg_bb *b, cg_instr **src, cg_instr **dst, unsigned size)
{
	cg_instr *permuted_src[size];
	cg_instr *permuted_dst[size];
	unsigned i, j, n = 0;

	for (i = 0; i < size; i++)
	{
		if (dst[i] == NULL)
		{
			assert(src[i] == NULL);
			continue;
		}
		assert(src[i] != NULL);

		assert(0 <= dst[i]->reg && dst[i]->reg < CG_REG_VREG0);
		assert(0 <= src[i]->reg && src[i]->reg < CG_REG_VREG0);

		permuted_src[n] = src[i];
		permuted_dst[n] = dst[i];
		n++;
	}
	if (n > 1)
	{
		for (i = 0; i < n - 1; i++)
		{
			if (permuted_dst[i]->reg != permuted_src[i]->reg)
			{
				for (j = i + 1; j < n; j++)
				{
					if (permuted_src[j]->reg == permuted_dst[i]->reg)
					{
						cg_instr *tmp = permuted_src[i];
						permuted_src[i] = permuted_src[j];
						permuted_src[j] = tmp;

						insert_single_swap(b, NULL, permuted_src[i], permuted_src[j]);
					}
				}
			}
		}
		assert(permuted_dst[n-1]->reg == permuted_src[n-1]->reg);
	}
}

static void
do_ssa_deconstruction(cg_func *func)
{
	cg_bb *b;
	for (b = func->bb_first; b != NULL; b = b->bb_next)
	{
		graph_edge *edge;

		if (b->n_phis == 0)
		{
			continue;
		}

		for (edge = graph_pred_first((graph_node *)b); edge != NULL; edge = graph_pred_next(edge))
		{
			cg_bb *pred = (cg_bb *)graph_edge_tail(edge);
			cg_instr *dst[b->n_phis]; /* Phi-instructions in b */
			cg_instr *src[b->n_phis]; /* Corresponding phi-args from pred */
			cg_instr *phi;
			unsigned idx = 0;

			for (phi = b->instr_phi_first; phi != NULL; phi = phi->instr_next)
			{
				dst[idx] = phi;
				src[idx] = cg_instr_phi_input_of(phi, pred);
				idx++;
			}

			/* Do as many safe copies as possible */
			insert_safe_copies(pred, src, dst, idx);

			/* Then break cycles with swaps */
			insert_swaps(pred, src, dst, idx);
		}
	}
}

static void
do_insert_spill(cg_func *func)
{
	cg_bb *b;

	for (b = func->bb_first; b != NULL; b = b->bb_next)
	{
		cg_instr *instr, *next_instr;

		for (instr = b->instr_first; instr != NULL; instr = next_instr)
		{
			assert(instr->op != CG_INSTR_OP_phi);
			next_instr = instr->instr_next;

			if (instr->ra.spill_id != -1)
			{
				int eq_spill_id = dset_find(func->ra.equiv_spill_id, instr->ra.spill_id);

				if (instr->op == CG_INSTR_OP_reload)
				{
					/* Insert load */
					int offset = func->ra.spill_slot_offsets[eq_spill_id];
					cg_instr *load = cg_instr_build(b, CG_INSTR_OP_ldr);
					load->reg = instr->reg;
					load->args[0].kind = CG_INSTR_ARG_HREG;
					load->args[0].u.hreg = CG_REG_sp;
					load->args[0].offset = func->stack_frame_size + offset * 4;
					cg_instr_link_after(instr, load);
					load->ra.dbg_spill_id = eq_spill_id; /* For debug use only */
				}
				else if (instr->op == CG_INSTR_OP_spill)
				{
					/* Insert store */
					int offset = func->ra.spill_slot_offsets[eq_spill_id];
					cg_instr *store = cg_instr_build(b, CG_INSTR_OP_str);
					store->reg = -1; /* No output */
					store->args[0].kind = CG_INSTR_ARG_HREG;
					store->args[0].u.hreg = instr->args[0].u.hreg;
					store->args[1].kind = CG_INSTR_ARG_HREG;
					store->args[1].u.hreg = CG_REG_sp;
					store->args[1].offset = func->stack_frame_size + offset * 4;
					cg_instr_link_after(instr, store);
					store->ra.dbg_spill_id = eq_spill_id; /* For debug use only */
				}
			}
		}
	}
}

static void
do_cleanup(cg_func *func)
{
	cg_bb *b;

	for (b = func->bb_first; b != NULL; b = b->bb_next)
	{
		cg_instr *instr, *next_instr;
		/* Drop phi-instructions */
		b->instr_phi_first = NULL;
		b->instr_phi_last = NULL;

		for (instr = b->instr_first; instr != NULL; instr = next_instr)
		{
			next_instr = instr->instr_next;

			switch (instr->op)
			{
				case CG_INSTR_OP_spill:
				case CG_INSTR_OP_reload:
				case CG_INSTR_OP_ret:
				case CG_INSTR_OP_undef:
					cg_instr_unlink(instr);
					continue;
				case CG_INSTR_OP_mov:
					if (instr->reg == get_reg_for_arg(instr, 0))
					{
						cg_instr_unlink(instr);
						continue;
					}
					break;
				default:
					break;
			}

			assert(instr->reg < CG_REG_VREG0);
			if (instr->reg >= CG_REG_r4)
			{
				func->clobber_mask |= (1 << instr->reg);
			}
		}
	}

	func->stack_frame_size += func->ra.n_spill_slots * 4;
}

static void
cg_regalloc_ssa_func(cg_func *func, unsigned max_regs)
{
	cg_instr *phi_lift_movs[1024]; /* TODO:FIXME: */
	unsigned n_phi_lift_movs = 0;
	unsigned cntr = 0;
	graph_marker marker;

	func->ra.rinfo = calloc(func->vreg_cntr, sizeof(func->ra.rinfo[0]));
	func->ra.n_rinfo = func->vreg_cntr;

	graph_marker_alloc(&func->cfg_graph_ctx, &marker);
	func->ra.rpo = calloc(func->n_bbs, sizeof(cg_bb *));
	cntr = 0;
	fill_rpo_rec(func, func->bb_first, &cntr, &marker);
	assert(cntr == func->n_bbs);
	graph_marker_free(&func->cfg_graph_ctx, &marker);

	/* Annotate each block with its depth in the dominator tree */
	cntr = 0;
	annotate_w_dtpo(func, func->bb_first, &cntr);


	/* Pristine */
	D(debug_print_ra(func, "00_pristine", 0));

	/* Phi-lifting */
	do_phi_lifting(func, phi_lift_movs, &n_phi_lift_movs);
	D(debug_print_ra(func, "01_phi_lifting", 0));

	/* Build lifetime intervals */
	do_lifetime_intervals(func);
	D(debug_print_ra(func, "02_lifetime_intervals", 1));

	/* Phi-analysis */
	do_phi_analysis(func);

	/* Phi-mem-coalesce */
	do_phi_mem_coalesce(func, phi_lift_movs, n_phi_lift_movs);
	D(debug_print_ra(func, "03_phi_mem_coalesce", 1));

	/* Select liveranges to spill */
	do_select_spill(func, max_regs);
	D(debug_print_ra(func, "04_select_spill", 1));

	/* Color assignment */
	do_color_assignment(func, max_regs);
	D(debug_print_ra(func, "05_color_assignment", 0));

	/* SSA deconstruction */
	do_ssa_deconstruction(func);
	D(debug_print_ra(func, "06_ssa_deconstruction", 0));

	/* Insert non-phi spills */
	do_insert_spill(func);
	D(debug_print_ra(func, "07_insert_spill", 0));

	/* Cleanup */
	do_cleanup(func);
	D(debug_print_ra(func, "08_cleanup", 0));
}

void
cg_regalloc_ssa_tu(cg_tu *tu, unsigned max_regs)
{
	cg_func *f;

	max_regs = (0 < max_regs && max_regs <= CG_REG_sp) ? max_regs : CG_REG_sp;

	for (f = tu->func_first; f != NULL; f = f->func_next)
	{
		cg_dom_setup_dom_info(f);
		cg_func_analyze_loops(f);
		cg_regalloc_ssa_func(f, max_regs);
	}
}

