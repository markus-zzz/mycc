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
#include <stdio.h>
#include <string.h>

#include "iselect.h"
#include "cg_tu.h"
#include "cg_data.h"
#include "cg_func.h"
#include "cg_bb.h"
#include "cg_instr.h"
#include "cg_reg.h"
#include "ir/ir_tu.h"
#include "ir/ir_data.h"
#include "ir/ir_func.h"
#include "ir/ir_bb.h"
#include "ir/ir_node.h"

struct use {
	cg_instr *instr;
	union {
		unsigned arg_idx;
		cg_bb *phi_arg_bb;
	} u;
};

struct info {
	cg_instr *instr;
	struct use *uses;
	unsigned uses_idx;
	unsigned n_uses_left;
	union {
		struct {
			unsigned sp_offset;
		} alloca;
	} u;
};

static graph_marker scratch_marker;

static struct info * get_info(ir_node *n)
{
	if (!graph_marker_is_set((graph_node *)n, &scratch_marker))
	{
		struct info *tmp;
		ir_node_use_iter uit;
		unsigned n_uses = 0;

		ir_node_use_iter_init(&uit, n);
		while (ir_node_use_iter_next(&uit, NULL)) n_uses++;

		tmp = calloc(1, sizeof(struct info));
		tmp->n_uses_left = n_uses;
		tmp->uses = calloc(n_uses, sizeof(struct use));

		graph_marker_set((graph_node *)n, &scratch_marker);
		ir_node_scratch_set(n, tmp);
	}

	return (struct info *)ir_node_scratch(n);
}

static void
update_uses_with_instr(struct info *irni, cg_instr *instr)
{
	unsigned i;
	for (i = 0; i < irni->uses_idx; i++)
	{
		if (irni->uses[i].instr->op == CG_INSTR_OP_phi)
		{
			cg_instr_add_phi_arg(irni->uses[i].instr, irni->uses[i].u.phi_arg_bb, instr);
		}
		else
		{
			cg_instr_arg_set_vreg(irni->uses[i].instr, irni->uses[i].u.arg_idx, instr);
		}
		irni->n_uses_left--;
	}
	irni->uses_idx = 0;
}

static void
add_delayed_arg(cg_instr *instr, unsigned arg_idx, ir_node *arg)
{
	struct info *ai = get_info(arg);
	ai->uses[ai->uses_idx].instr = instr;
	ai->uses[ai->uses_idx++].u.arg_idx = arg_idx;
	if (ai->instr != NULL)
	{
		update_uses_with_instr(ai, ai->instr);
	}
}

static cg_instr *
iselect_cmp(cg_bb *cgb, ir_node *irn)
{
	cg_instr *cgi;
	unsigned n_args;
	ir_node *args[2];
	cg_instr_op iop = CG_INSTR_OP_cmp;

	cgi = cg_instr_build(cgb, iop);

	ir_node_get_args(irn, &n_args, args, sizeof(args)/sizeof(args[0]));
	/* Add arg later when available */
	add_delayed_arg(cgi, 0, args[0]);
	if (ir_node_op(args[1]) == IR_OP_const && ir_node_const_as_u64(args[1]) <= 0xff)
	{
		cgi->args[1].kind = CG_INSTR_ARG_IMM;
		cgi->args[1].u.imm = ir_node_const_as_u64(args[1]);
		get_info(args[1])->n_uses_left--;
	}
	else
	{
		/* Add arg later when available */
		add_delayed_arg(cgi, 1, args[1]);
	}

	return cgi;
}

static cg_instr *
iselect_binop(cg_bb *cgb, ir_node *irn)
{
	cg_instr *cgi;
	unsigned n_args;
	ir_node *args[2];
	cg_instr_op iop = CG_INSTR_OP_mov;
	int is_op2 = 0;

	switch (ir_node_op(irn)) {
		case IR_OP_add: iop = CG_INSTR_OP_add; is_op2 = 1; break;
		case IR_OP_sub: iop = CG_INSTR_OP_sub; is_op2 = 1; break;
		case IR_OP_mul: iop = CG_INSTR_OP_mul; is_op2 = 0; break;
		case IR_OP_and: iop = CG_INSTR_OP_and; is_op2 = 1; break;
		case IR_OP_or:  iop = CG_INSTR_OP_orr; is_op2 = 1; break;
		case IR_OP_xor: iop = CG_INSTR_OP_eor; is_op2 = 1; break;
		case IR_OP_shl: iop = CG_INSTR_OP_lsl; is_op2 = 1; break;
		case IR_OP_ashr:iop = CG_INSTR_OP_asr; is_op2 = 1; break;
		case IR_OP_lshr:iop = CG_INSTR_OP_lsr; is_op2 = 1; break;
		default: assert(0); break;
	}

	cgi = cg_instr_build(cgb, iop);

	ir_node_get_args(irn, &n_args, args, sizeof(args)/sizeof(args[0]));
	/* Add arg later when available */
	add_delayed_arg(cgi, 0, args[0]);
	if (is_op2 && ir_node_op(args[1]) == IR_OP_const && ir_node_const_as_u64(args[1]) <= 0xff)
	{
		cgi->args[1].kind = CG_INSTR_ARG_IMM;
		cgi->args[1].u.imm = ir_node_const_as_u64(args[1]);
		get_info(args[1])->n_uses_left--;
	}
	else
	{
		/* Add arg later when available */
		add_delayed_arg(cgi, 1, args[1]);
	}

	return cgi;
}

static cg_instr *
iselect_conv(cg_bb *cgb, ir_node *irn)
{
	cg_instr *cgi;
	unsigned n_args;
	ir_node *args[1];
	cg_instr_op iop;

	ir_node_get_args(irn, &n_args, args, sizeof(args)/sizeof(args[0]));

	if (ir_node_op(irn) == IR_OP_sext)
	{
		if (ir_node_type(args[0]) == i8)
		{
			iop = CG_INSTR_OP_sxtb;
		}
		else
		{
			assert(ir_node_type(args[0]) == i16);
			iop = CG_INSTR_OP_sxth;
		}
	}
	else if (ir_node_op(irn) == IR_OP_zext)
	{
		if (ir_node_type(args[0]) == i8)
		{
			iop = CG_INSTR_OP_uxtb;
		}
		else
		{
			assert(ir_node_type(args[0]) == i16);
			iop = CG_INSTR_OP_uxth;
		}
	}
	else
	{
		assert(ir_node_op(irn) == IR_OP_trunc);
		if (ir_node_type(irn) == i8)
		{
			iop = CG_INSTR_OP_uxtb;
		}
		else
		{
			assert(ir_node_type(irn) == i16);
			iop = CG_INSTR_OP_uxth;
		}
	}

	cgi = cg_instr_build(cgb, iop);

	/* Add arg later when available */
	add_delayed_arg(cgi, 0, args[0]);

	return cgi;
}

static cg_instr *
iselect_const(cg_bb *cgb, ir_node *irn)
{
	cg_instr *cgi;

	cgi = cg_instr_build(cgb, CG_INSTR_OP_mov);

	cgi->args[0].kind = CG_INSTR_ARG_IMM;
	cgi->args[0].u.imm = ir_node_const_as_u64(irn);

	return cgi;
}

static cg_instr *
iselect_addr_of(cg_bb *cgb, ir_node *irn)
{
	cg_instr *cgi;

	cgi = cg_instr_build(cgb, CG_INSTR_OP_mov);
	cgi->args[0].kind = CG_INSTR_ARG_SYM;
	cgi->args[0].u.sym = strdup(ir_node_addr_of_data(irn)->name);

	return cgi;
}

static cg_instr *
iselect_alloca(cg_bb *cgb, ir_node *irn)
{
	cg_instr *cgi;
	unsigned sp_offset = get_info(irn)->u.alloca.sp_offset;

	if (sp_offset > 0)
	{
		cgi = cg_instr_build(cgb, CG_INSTR_OP_add);
		cgi->args[1].kind = CG_INSTR_ARG_IMM;
		cgi->args[1].u.imm = sp_offset;
	}
	else
	{
		cgi = cg_instr_build(cgb, CG_INSTR_OP_mov);
	}

	cgi->args[0].kind = CG_INSTR_ARG_HREG;
	cgi->args[0].u.hreg = CG_REG_sp;

	return cgi;
}

static cg_instr *
iselect_load_store(cg_bb *cgb, ir_node *irn)
{
	ir_node *args[2];
	cg_instr *cgi;
	cg_instr_op op;

	ir_node_get_args(irn, NULL, args, 2);

	if (ir_node_op(irn) == IR_OP_load)
	{
		switch (ir_node_type(irn)) {
			case i8:  op = CG_INSTR_OP_ldrb; break;
			case i16: op = CG_INSTR_OP_ldrh; break;
			case i32: op = CG_INSTR_OP_ldr;  break;
			case p32: op = CG_INSTR_OP_ldr;  break;
			default: assert(0); break;
		}
		cgi = cg_instr_build(cgb, op);
		add_delayed_arg(cgi, 0, args[0]);
	}
	else
	{
		assert(ir_node_op(irn) == IR_OP_store);
		switch (ir_node_type(irn)) {
			case i8:  op = CG_INSTR_OP_strb; break;
			case i16: op = CG_INSTR_OP_strh; break;
			case i32: op = CG_INSTR_OP_str;  break;
			case p32: op = CG_INSTR_OP_str;  break;
			default: assert(0); break;
		}
		cgi = cg_instr_build(cgb, op);
		add_delayed_arg(cgi, 0, args[1]);
		add_delayed_arg(cgi, 1, args[0]);
		cgi->reg = -1; /* No output */
	}

	return cgi;
}

static cg_func *
cg_iselect_func(cg_tu *ctu, ir_func *irf)
{
	cg_func *cgf;
	ir_bb_iter irbit;
	ir_bb *irb;
	ir_node_iter irnit;
	ir_node *irn;
	unsigned sp_offset;

	cgf = cg_func_build(ctu, irf->name);

	graph_marker_alloc(&irf->ssa_graph_ctx, &scratch_marker);

	cgf->clobber_mask |= (1 << CG_REG_lr);

	/* Build blocks */
	ir_bb_iter_init(&irbit, irf);
	while ((irb = ir_bb_iter_next(&irbit)))
	{
		cg_bb *cgb;
		cgb = cg_bb_build(cgf);
		cg_bb_link_last(cgb);
		ir_bb_scratch_set(irb, cgb);
	}

	/* Allocate allocas */
	sp_offset = 0;
	ir_node_iter_init(&irnit, irf->entry);
	while ((irn = ir_node_iter_next(&irnit)))
	{
		if (ir_node_op(irn) == IR_OP_alloca)
		{
			get_info(irn)->u.alloca.sp_offset = sp_offset;
			sp_offset += ir_node_alloca_size(irn);
		}
	}
	cgf->stack_frame_size = sp_offset;

	/* Setup phi-nodes */
	ir_bb_iter_init(&irbit, irf);
	while ((irb = ir_bb_iter_next(&irbit)))
	{
		ir_node_iter irnit;
		ir_node *irn;
		cg_bb *cgb = ir_bb_scratch(irb);

		ir_node_iter_init(&irnit, irb);
		while ((irn = ir_node_iter_next(&irnit)))
		{
			cg_instr *cgi;
			ir_node *parg;
			ir_bb *pargbb;
			ir_node_phi_arg_iter it;

			if (ir_node_op(irn) != IR_OP_phi)
			{
				break;
			}

			cgi = cg_instr_build_phi(cgb, vooid);
			get_info(irn)->instr = cgi;
			ir_node_phi_arg_iter_init(&it, irn);
			while ((parg = ir_node_phi_arg_iter_next(&it, &pargbb)))
			{
				struct info *ai = get_info(parg);
				ai->uses[ai->uses_idx].instr = cgi;
				ai->uses[ai->uses_idx].u.phi_arg_bb = ir_bb_scratch(pargbb);
				ai->uses_idx++;
			}
		}

		if (irb != irf->exit)
		{
			if (ir_bb_get_term_node(irb) != NULL)
			{
				ir_node *ircmp = ir_bb_get_term_node(irb);
				cg_instr *cgcmp;
				ir_bb *irb_true = ir_bb_get_true_target(irb);
				ir_bb *irb_false = ir_bb_get_false_target(irb);

				cg_bb_link_cfg(cgb, ir_bb_scratch(irb_true));
				cg_bb_link_cfg(cgb, ir_bb_scratch(irb_false));

				cgb->true_target = ir_bb_scratch(irb_true);
				cgb->false_target = ir_bb_scratch(irb_false);
				switch (ir_node_op(ircmp)) {
					case IR_OP_icmp_eq:  cgb->true_cond = CG_COND_eq; break;
					case IR_OP_icmp_ne:  cgb->true_cond = CG_COND_ne; break;
					case IR_OP_icmp_slt: cgb->true_cond = CG_COND_lt; break;
					case IR_OP_icmp_sle: cgb->true_cond = CG_COND_le; break;
					case IR_OP_icmp_sgt: cgb->true_cond = CG_COND_gt; break;
					case IR_OP_icmp_sge: cgb->true_cond = CG_COND_ge; break;
					default: assert(0); break;
				}

				if (cgb->true_target == cgb->bb_next)
				{
					/* True target should never be fall-through. */
					cgb->true_cond = cg_cond_inv(cgb->true_cond);
					cgb->true_target = ir_bb_scratch(irb_false);
					cgb->false_target = ir_bb_scratch(irb_true);
				}

				cgcmp = iselect_cmp(cgb, ircmp);
				cgcmp->reg = -1; /* No output */
				cg_instr_link_last(cgcmp);
				get_info(ircmp)->n_uses_left--;
			}
			else
			{
				ir_bb *irb_default = ir_bb_get_default_target(irb);

				cg_bb_link_cfg(cgb, ir_bb_scratch(irb_default));
				cgb->true_cond = CG_COND_al;
			}
		}
	}

	/* Setup return value MOV */
	if (irf->ret_type != vooid)
	{
		cg_bb *cgb = ir_bb_scratch(irf->exit);
		cg_instr *ret;
		ret = cg_instr_build(cgb, CG_INSTR_OP_ret);
		ret->reg = -1;
		add_delayed_arg(ret, 0, ir_bb_get_term_node(irf->exit));
		cg_instr_link_last(ret);
	}

	/* Do the actual iselect */
	ir_bb_iter_rev_init(&irbit, irf);
	while ((irb = ir_bb_iter_next(&irbit)))
	{
		ir_node_iter irnit;
		ir_node *irn;
		cg_bb *cgb = ir_bb_scratch(irb);

		ir_node_iter_rev_init(&irnit, irb);
		while ((irn = ir_node_iter_next(&irnit)))
		{
			struct info *irni = get_info(irn);
			cg_instr *cgi = NULL;

			if (ir_node_op(irn) == IR_OP_call)
			{
				cg_instr *call;
				ir_node_arg_iter ait;
				ir_node_use_iter uit;
				ir_node *arg;
				unsigned idx;

				call = cg_instr_build(cgb, CG_INSTR_OP_call);

				/* Setup return value if call has a use */
				ir_node_use_iter_init(&uit, irn);
				if (ir_node_use_iter_next(&uit, NULL))
				{
					update_uses_with_instr(irni, call);
				}
				else
				{
					call->reg = -1; /* no output */
				}

				/* Build call for actual call */
				call->args[0].kind = CG_INSTR_ARG_SYM;
				call->args[0].u.sym = strdup(ir_node_call_target(irn)->name);
				cg_instr_link_first(call);

				/* Setup parameters */
				ir_node_arg_iter_init(&ait, irn);
				while ((arg = ir_node_arg_iter_next(&ait, &idx)))
				{
					add_delayed_arg(call, idx + 1, arg);
				}
				continue;
			}
			else if (ir_node_op(irn) == IR_OP_getparam)
			{
				/* Handled later */
				continue;
			}
			else if (ir_node_op(irn) == IR_OP_phi)
			{
				/* Already inserted */
				cgi = irni->instr;
			}
			else
			{
				if (irni->n_uses_left > 0 || ir_node_op(irn) == IR_OP_store)
				{
					switch (ir_node_op(irn)) {
						case IR_OP_addr_of:
							cgi = iselect_addr_of(cgb, irn);
							break;
						case IR_OP_alloca:
							cgi = iselect_alloca(cgb, irn);
							break;
						case IR_OP_const:
							cgi = iselect_const(cgb, irn);
							break;
						case IR_OP_undef:
							cgi = cg_instr_build(cgb, CG_INSTR_OP_undef);
							break;
						case IR_OP_load:
						case IR_OP_store:
							cgi = iselect_load_store(cgb, irn);
							break;
						case IR_OP_icmp_slt:
						case IR_OP_icmp_sgt:
							cgi = iselect_cmp(cgb, irn);
							break;
						case IR_OP_add:
						case IR_OP_sub:
						case IR_OP_mul:
						case IR_OP_and:
						case IR_OP_or:
						case IR_OP_xor:
						case IR_OP_shl:
						case IR_OP_ashr:
						case IR_OP_lshr:
							cgi = iselect_binop(cgb, irn);
							break;
						case IR_OP_sext:
						case IR_OP_zext:
						case IR_OP_trunc:
							cgi = iselect_conv(cgb, irn);
							break;
						default:
							assert(0);
							break;
					}

					cg_instr_link_first(cgi);
				}
			}

			update_uses_with_instr(irni, cgi);
		}
	}

	/* Setup parameter ARGs */
	{
		ir_node_iter irnit;
		ir_node *irn;

		ir_node_iter_init(&irnit, irf->entry);
		while ((irn = ir_node_iter_next(&irnit)))
		{
			if (ir_node_op(irn) == IR_OP_getparam)
			{
				cg_instr *arg = cg_instr_build(cgf->bb_first, CG_INSTR_OP_arg);
				assert(0 <= ir_node_getparam_idx(irn) && ir_node_getparam_idx(irn) < N_ARRAY_SIZE(cgf->args));
				cgf->args[ir_node_getparam_idx(irn)] = arg;

				update_uses_with_instr(get_info(irn), arg);
			}
		}
	}

	graph_marker_free(&irf->ssa_graph_ctx, &scratch_marker);

	return cgf;
}

cg_tu *
cg_iselect_tu(ir_tu *irt)
{
	cg_tu *ctu = calloc(1, sizeof(cg_tu));
	ir_data *d;
	ir_func *f;

	for (d = irt->first_ir_data; d != NULL; d = d->tu_next)
	{
		(void)cg_data_build(ctu, d->name, d->size, d->align, d->init);
	}

	for (f = irt->first_ir_func; f != NULL; f = f->tu_list_next)
	{
		if (!ir_func_is_definition(f))
		{
			continue;
		}

		(void)cg_iselect_func(ctu, f);
	}

	return ctu;
}
