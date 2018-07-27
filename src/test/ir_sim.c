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

/* The IR dumps are best viewed by passing them through: column -t -s $'\t' */

#include "ir/ir.h"
#include "ir/ir_tu.h"
#include "ir/ir_bb.h"
#include "ir/ir_data.h"
#include "ir/ir_func.h"
#include "ir/ir_node.h"
#include "ir/ir_type.h"
#include "ir/ir_print.h"
#include "test/ir_sim.h"
#include "test/mem.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <glib.h>

#define BSOP(type,op,res,operand1,operand2) \
do { \
	res->undef_mask = operand1->undef_mask | operand2->undef_mask; \
	switch (type) { \
		case i8:  res->u.i8  = operand1->u.i8  op operand2->u.i8;  break; \
		case i16: res->u.i16 = operand1->u.i16 op operand2->u.i16; break; \
		case p32: \
		case i32: res->u.i32 = operand1->u.i32 op operand2->u.i32; break; \
		case p64: \
		case i64: res->u.i64 = operand1->u.i64 op operand2->u.i64; break; \
		default: assert(0); break; \
	} \
} while (0)

#define BUOP(type,op,res,operand1,operand2) \
do { \
	res->undef_mask = operand1->undef_mask | operand2->undef_mask; \
	switch (type) { \
		case i8:  res->u.u8  = operand1->u.u8  op operand2->u.u8;  break; \
		case i16: res->u.u16 = operand1->u.u16 op operand2->u.u16; break; \
		case p32: \
		case i32: res->u.u32 = operand1->u.u32 op operand2->u.u32; break; \
		case p64: \
		case i64: res->u.u64 = operand1->u.u64 op operand2->u.u64; break; \
		default: assert(0); break; \
	} \
} while (0)

#define USOP(type,op,res,operand1) \
do { \
	res->undef_mask = operand1->undef_mask; \
	switch (type) { \
		case i8:  res->u.i8  = op operand1->u.i8;  break; \
		case i16: res->u.i16 = op operand1->u.i16; break; \
		case p32: \
		case i32: res->u.i32 = op operand1->u.i32; break; \
		case p64: \
		case i64: res->u.i64 = op operand1->u.i64; break; \
		default: assert(0); break; \
	} \
} while (0)

#define UUOP(type,op,res,operand1) \
do { \
	res->undef_mask = operand1->undef_mask; \
	switch (type) { \
		case i8:  res->u.u8  = op operand1->u.u8;  break; \
		case i16: res->u.u16 = op operand1->u.u16; break; \
		case p32: \
		case i32: res->u.u32 = op operand1->u.u32; break; \
		case p64: \
		case i64: res->u.u64 = op operand1->u.u64; break; \
		default: assert(0); break; \
	} \
} while (0)

typedef struct ssa_value {
	union {
		int8_t i8;
		int16_t i16;
		int32_t i32;
		int64_t i64;
		uint8_t u8;
		uint16_t u16;
		uint32_t u32;
		uint64_t u64;
	} u;
	uint64_t undef_mask;
} ssa_value;

static void ssa_values_for_args(GHashTable *map, ir_node *n, unsigned max_args, ir_node **args, ssa_value **vargs)
{
	unsigned n_args, i;

	ir_node_get_args(n, &n_args, args, max_args);
	for (i = 0; i < n_args; i++)
	{
		vargs[i] = g_hash_table_lookup(map, args[i]);
		assert(vargs[i]);
	}
}

static void newline(FILE *fp, unsigned indent)
{
	unsigned i;
	fprintf(fp, "\n");
	for (i = 0; i < indent; i++)
	{
		fprintf(fp, " ");
	}
}

static void print_node(FILE *fp, ir_node *n)
{
	fprintf(fp, "%%bb%d: ", ir_bb_id(ir_node_bb(n)));
	ir_print_node(fp, n);
}

static uint64_t get_mask_for_type(ir_type type)
{
	uint64_t mask = 0;

	switch (type) {
	case i8:  mask = 0xff; break;
	case i16: mask = 0xffff; break;
	case p32:
	case i32: mask = 0xffffffff; break;
	case p64:
	case i64: mask = 0xffffffffffffffffull; break;
	default: assert(type == vooid); break;
	}

	return mask;
}

static void printvalue(FILE *fp, ir_type type, ssa_value *v)
{
	if (v->undef_mask & get_mask_for_type(type))
	{
		fprintf(fp, "\t-- [UNDEF]");
		return;
	}

	switch (type) {
	case i8:  fprintf(fp, "\t-- [0x%02x]",  v->u.u8); break;
	case i16: fprintf(fp, "\t-- [0x%04x]",  v->u.u16); break;
	case p32:
	case i32: fprintf(fp, "\t-- [0x%08x]",  v->u.u32); break;
	case p64:
	case i64: fprintf(fp, "\t-- [0x%016"PRIx64"]", v->u.u64); break;
	default: assert(type == vooid); break;
	}
}

void ir_sim_func_1(FILE *fp, memctx *mctx, unsigned indent, unsigned sp,
                   ir_func *func, ssa_value *fret, unsigned n_fargs, ssa_value **fargs)
{
	ir_bb *bb = func->entry;
	ir_bb *prev_bb = NULL;
	ir_node_iter nit;
	ir_node *n;
	GHashTable *valuemap;

	valuemap = g_hash_table_new(NULL, NULL);
	ssa_value values_table[func->n_ir_nodes];
	unsigned values_table_idx = 0;
	unsigned phi_stack_idx;
	struct {
		ir_node *n;
		ssa_value *vn;
		ssa_value tmp_vn;
	} phi_stack[128];

	while (bb != NULL)
	{
		phi_stack_idx = 0;
		ir_node_iter_init(&nit, bb);
		while ((n = ir_node_iter_next(&nit)))
		{
			ssa_value *vn;
			ssa_value *vargs[16];
			ir_node *args[16];

			if ((vn = g_hash_table_lookup(valuemap, n)) == NULL)
			{
				vn = &values_table[values_table_idx++];
				g_hash_table_insert(valuemap, n, vn);
				assert(values_table_idx <= func->n_ir_nodes);
			}

			if (ir_node_op(n) != IR_OP_phi)
			{
				ssa_values_for_args(valuemap, n, 16, args, vargs);
				/* copy pending phi-values */
				while (phi_stack_idx > 0)
				{
					phi_stack_idx--;
					phi_stack[phi_stack_idx].vn->u = phi_stack[phi_stack_idx].tmp_vn.u;
					phi_stack[phi_stack_idx].vn->undef_mask = phi_stack[phi_stack_idx].tmp_vn.undef_mask;
					newline(fp, indent);
					print_node(fp, phi_stack[phi_stack_idx].n);
					printvalue(fp, ir_node_type(phi_stack[phi_stack_idx].n), phi_stack[phi_stack_idx].vn);
				}
				newline(fp, indent);
				print_node(fp, n);
			}

			switch (ir_node_op(n))
			{
			case IR_OP_addr_of:
				vn->u.u32 = (unsigned)(unsigned long)ir_node_addr_of_data(n)->scratch;
				vn->undef_mask = 0;
				break;

			case IR_OP_alloca:
				sp += ir_node_alloca_align(n) - 1;
				sp &= ~(ir_node_alloca_align(n) - 1);
				vn->u.u32 = sp;
				vn->undef_mask = 0;
				sp += ir_node_alloca_size(n);
				break;

			case IR_OP_phi:
				{
					ir_node_phi_arg_iter pit;
					ir_node *tmp;
					ir_bb *tmp_bb;
					ir_node_phi_arg_iter_init(&pit, n);
					while ((tmp = ir_node_phi_arg_iter_next(&pit, &tmp_bb)))
					{
						if (tmp_bb == prev_bb)
						{
							ssa_value *vtmp;
							vtmp = g_hash_table_lookup(valuemap, tmp);
							assert(phi_stack_idx < sizeof(phi_stack)/sizeof(phi_stack[0]));
							phi_stack[phi_stack_idx].n = n;
							phi_stack[phi_stack_idx].vn = vn;
							phi_stack[phi_stack_idx].tmp_vn.u = vtmp->u;
							phi_stack[phi_stack_idx].tmp_vn.undef_mask = vtmp->undef_mask;
							phi_stack_idx++;
							break;
						}
					}
				}
				break;

			case IR_OP_const:
				{
					switch (ir_node_type(n)) {
					case i8: vn->u.u8 = ir_node_const_as_u64(n); break;
					case i16: vn->u.u16 = ir_node_const_as_u64(n); break;
					case p32:
					case i32: vn->u.u32 = ir_node_const_as_u64(n); break;
					case p64:
					case i64: vn->u.u64 = ir_node_const_as_u64(n); break;
					default: assert(0); break;
					}
					vn->undef_mask = 0;
				}
				break;

			case IR_OP_undef:
				vn->undef_mask = -1;
				break;

			case IR_OP_load:
				{
					const unsigned size = ir_type_bytes(ir_node_type(n));
					uint8_t memory[8];
					uint8_t valid[8];
					int i;
					vn->undef_mask = 0;
					for (i = 0; i < size; i++)
					{
						mem_read(mctx, vargs[0]->u.u32 + i, &memory[i], &valid[i]);
						if (valid[i] != 0xff) vn->undef_mask |= (0xffull << i*8);
					}
					vn->undef_mask &= get_mask_for_type(ir_node_type(n));

					switch (ir_node_type(n)) {
					case i8:  vn->u.u8  = *((uint8_t  *)&memory[0]); break;
					case i16: vn->u.u16 = *((uint16_t *)&memory[0]); break;
					case p32:
					case i32: vn->u.u32 = *((uint32_t *)&memory[0]); break;
					case p64:
					case i64: vn->u.u64 = *((uint64_t *)&memory[0]); break;
					default: assert(0); break;
					}
				}
				break;

			case IR_OP_store:
				{
					const unsigned size = ir_type_bytes(ir_node_type(n));
					uint8_t memory[8];
					unsigned i;
					switch (ir_node_type(n)) {
					case i8:  *((uint8_t  *)&memory[0]) = vargs[1]->u.u8;  break;
					case i16: *((uint16_t *)&memory[0]) = vargs[1]->u.u16; break;
					case p32:
					case i32: *((uint32_t *)&memory[0]) = vargs[1]->u.u32; break;
					case p64:
					case i64: *((uint64_t *)&memory[0]) = vargs[1]->u.u64; break;
					default: assert(0); break;
					}
					for (i = 0; i < size; i++)
					{
						uint8_t valid = ~(vargs[1]->undef_mask << i*8) & 0xff;
						mem_write(mctx, vargs[0]->u.u32 + i, memory[i], valid);
					}
					vn->u = vargs[1]->u;  /* Just for show */
					vn->undef_mask = vargs[1]->undef_mask;
				}
				break;

			case IR_OP_call:
				{
					ssa_value rvalue_, *rvalue = &rvalue_;
					ir_sim_func_1(fp, mctx, indent + 2, sp, ir_node_call_target(n), rvalue, 16 /* TODO:FIXME */, vargs);
					if (ir_node_type(n) != vooid)
					{
						UUOP(ir_node_type(n), , vn, rvalue);
					}
				}
				break;

			case IR_OP_getparam:
				{
					unsigned getparam_idx = ir_node_getparam_idx(n);
					assert(getparam_idx < n_fargs);
					switch (ir_node_type(n)) {
					case i8:  vn->u.u8  = fargs[getparam_idx]->u.u8; break;
					case i16: vn->u.u16 = fargs[getparam_idx]->u.u16; break;
					case p32:
					case i32: vn->u.u32 = fargs[getparam_idx]->u.u32; break;
					case p64:
					case i64: vn->u.u64 = fargs[getparam_idx]->u.u64; break;
					default: assert(0); break;
					}
					vn->undef_mask = fargs[getparam_idx]->undef_mask;
				}
				break;

			case IR_OP_add: BUOP(ir_node_type(n), +, vn, vargs[0], vargs[1]); break;
			case IR_OP_sub: BUOP(ir_node_type(n), -, vn, vargs[0], vargs[1]); break;
			case IR_OP_neg: UUOP(ir_node_type(n), -, vn, vargs[0]); break;
			case IR_OP_mul: BUOP(ir_node_type(n), *, vn, vargs[0], vargs[1]); break;
			case IR_OP_udiv: BUOP(ir_node_type(n), /, vn, vargs[0], vargs[1]); break;
			case IR_OP_sdiv: BSOP(ir_node_type(n), /, vn, vargs[0], vargs[1]); break;
			case IR_OP_urem: BUOP(ir_node_type(n), %, vn, vargs[0], vargs[1]); break;
			case IR_OP_srem: BSOP(ir_node_type(n), %, vn, vargs[0], vargs[1]); break;

			case IR_OP_and: BUOP(ir_node_type(n), &, vn, vargs[0], vargs[1]); break;
			case IR_OP_or:  BUOP(ir_node_type(n), |, vn, vargs[0], vargs[1]); break;
			case IR_OP_xor: BUOP(ir_node_type(n), ^, vn, vargs[0], vargs[1]); break;
			case IR_OP_not: UUOP(ir_node_type(n), ~, vn, vargs[0]); break;

			case IR_OP_shl:  BUOP(ir_node_type(n), <<, vn, vargs[0], vargs[1]); break;
			case IR_OP_lshr: BUOP(ir_node_type(n), >>, vn, vargs[0], vargs[1]); break;
			case IR_OP_ashr: BSOP(ir_node_type(n), >>, vn, vargs[0], vargs[1]); break;

			case IR_OP_icmp_slt: BSOP(ir_node_type(n),  <, vn, vargs[0], vargs[1]); break;
			case IR_OP_icmp_sle: BSOP(ir_node_type(n), <=, vn, vargs[0], vargs[1]); break;
			case IR_OP_icmp_sgt: BSOP(ir_node_type(n),  >, vn, vargs[0], vargs[1]); break;
			case IR_OP_icmp_sge: BSOP(ir_node_type(n), >=, vn, vargs[0], vargs[1]); break;
			case IR_OP_icmp_ult: BUOP(ir_node_type(n),  <, vn, vargs[0], vargs[1]); break;
			case IR_OP_icmp_ule: BUOP(ir_node_type(n), <=, vn, vargs[0], vargs[1]); break;
			case IR_OP_icmp_ugt: BUOP(ir_node_type(n),  >, vn, vargs[0], vargs[1]); break;
			case IR_OP_icmp_uge: BUOP(ir_node_type(n), >=, vn, vargs[0], vargs[1]); break;
			case IR_OP_icmp_eq:  BUOP(ir_node_type(n), ==, vn, vargs[0], vargs[1]); break;
			case IR_OP_icmp_ne:  BUOP(ir_node_type(n), !=, vn, vargs[0], vargs[1]); break;

			case IR_OP_sext:
			{
				int64_t tmp_i64 = 0;
				switch (ir_node_type(args[0])) {
				case i8:  tmp_i64 = vargs[0]->u.i8;  break;
				case i16: tmp_i64 = vargs[0]->u.i16; break;
				case p32: tmp_i64 = vargs[0]->u.i32; break;
				case i32: tmp_i64 = vargs[0]->u.i32; break;
				case p64: tmp_i64 = vargs[0]->u.i64; break;
				case i64: tmp_i64 = vargs[0]->u.i64; break;
				default: assert(0); break;
				}

				switch (ir_node_type(n)) {
				case i8:  vn->u.i8  = tmp_i64; break;
				case i16: vn->u.i16 = tmp_i64; break;
				case p32: vn->u.i32 = tmp_i64; break;
				case i32: vn->u.i32 = tmp_i64; break;
				case p64: vn->u.i64 = tmp_i64; break;
				case i64: vn->u.i64 = tmp_i64; break;
				default: assert(0); break;
				}
				vn->undef_mask = (vargs[0]->undef_mask & get_mask_for_type(ir_node_type(args[0]))) ? get_mask_for_type(ir_node_type(n)) : 0;
			}
			break;

			case IR_OP_zext:
			{
				uint64_t tmp_u64 = 0;
				switch (ir_node_type(args[0])) {
				case i8:  tmp_u64 = vargs[0]->u.u8;  break;
				case i16: tmp_u64 = vargs[0]->u.u16; break;
				case p32: tmp_u64 = vargs[0]->u.u32; break;
				case i32: tmp_u64 = vargs[0]->u.u32; break;
				case p64: tmp_u64 = vargs[0]->u.u64; break;
				case i64: tmp_u64 = vargs[0]->u.u64; break;
				default: assert(0); break;
				}

				switch (ir_node_type(n)) {
				case i8:  vn->u.u8  = tmp_u64; break;
				case i16: vn->u.u16 = tmp_u64; break;
				case p32: vn->u.u32 = tmp_u64; break;
				case i32: vn->u.u32 = tmp_u64; break;
				case p64: vn->u.u64 = tmp_u64; break;
				case i64: vn->u.u64 = tmp_u64; break;
				default: assert(0); break;
				}
				vn->undef_mask = (vargs[0]->undef_mask & get_mask_for_type(ir_node_type(args[0]))) ? get_mask_for_type(ir_node_type(n)) : 0;
			}
			break;

			case IR_OP_trunc:
			{
				switch (ir_node_type(n)) {
				case i8:  vn->u.u8  = vargs[0]->u.u8;  break;
				case i16: vn->u.u16 = vargs[0]->u.u16; break;
				case p32: vn->u.u32 = vargs[0]->u.u32; break;
				case i32: vn->u.u32 = vargs[0]->u.u32; break;
				case p64: vn->u.u64 = vargs[0]->u.u64; break;
				case i64: vn->u.u64 = vargs[0]->u.u64; break;
				default: assert(0); break;
				}
				vn->undef_mask = (vargs[0]->undef_mask & get_mask_for_type(ir_node_type(args[0]))) ? get_mask_for_type(ir_node_type(n)) : 0;
			}
			break;

			default: assert(0); break;
			}

			if (ir_node_op(n) != IR_OP_phi)
			{
				printvalue(fp, ir_node_type(n), vn);
			}
		}

		/* copy pending phi-values (if block was only phi-nodes) */
		while (phi_stack_idx > 0)
		{
			phi_stack_idx--;
			phi_stack[phi_stack_idx].vn->u = phi_stack[phi_stack_idx].tmp_vn.u;
			phi_stack[phi_stack_idx].vn->undef_mask = phi_stack[phi_stack_idx].tmp_vn.undef_mask;
			newline(fp, indent);
			print_node(fp, phi_stack[phi_stack_idx].n);
			printvalue(fp, ir_node_type(phi_stack[phi_stack_idx].n), phi_stack[phi_stack_idx].vn);
		}

		if (func->exit != bb)
		{
			prev_bb = bb;
			if (ir_bb_get_term_node(bb) != NULL)
			{
				ir_node *ncond = ir_bb_get_term_node(bb);
				ssa_value *vcond;
				int cond;
				vcond = g_hash_table_lookup(valuemap, ncond);
				switch (ir_node_type(ncond)) {
				case i8:  cond = (vcond->u.u8  != 0); break;
				case i16: cond = (vcond->u.u16 != 0); break;
				case i32: cond = (vcond->u.u32 != 0); break;
				case i64: cond = (vcond->u.u64 != 0); break;
				default: assert(0); break;
				}
				bb = cond ? ir_bb_get_true_target(bb) : ir_bb_get_false_target(bb);
			}
			else
			{
				bb = ir_bb_get_default_target(bb);
			}
		}
		else
		{
			if (ir_bb_get_term_node(bb) != NULL)
			{
				ir_node *tn = ir_bb_get_term_node(bb);
				ssa_value *vn;
				vn = g_hash_table_lookup(valuemap, tn);
				UUOP(func->ret_type, , fret, vn);
				newline(fp, indent);
				fprintf(fp, "ret %%%d", ir_node_id(tn));
			}
			else
			{
				newline(fp, indent);
				fprintf(fp, "ret");
			}
			bb = NULL;
		}
	}

	g_hash_table_destroy(valuemap);
}

void ir_sim_func(FILE *fp, ir_tu *tu, const char *fname)
{
	const unsigned data_start  = 0xe0000000;
	const unsigned stack_start = 0xf0000000;
	ir_data *d;
	ir_func *f;
	unsigned dp = data_start;
	memctx *mctx = mem_new();

	for (d = tu->first_ir_data; d != NULL; d = d->tu_next)
	{
		assert((d->align & (d->align - 1)) == 0);
		dp = (dp + (d->align - 1)) & ~(d->align - 1);
		d->scratch = (void*)(unsigned long)dp;
		if (d->init)
		{
			unsigned i;
			for (i = 0; i < d->size; i++)
			{
				mem_write(mctx, dp + i, d->init[i], 0xff);
			}
		}
		dp += d->size;
	}

	for (f = tu->first_ir_func; f != NULL; f = f->tu_list_next)
	{
		if (strcmp(fname, f->name) == 0)
		{
			ssa_value rval_, *rval = &rval_;
			ir_sim_func_1(fp, mctx, 0, stack_start, f, rval, 0, NULL);
			printvalue(fp, f->ret_type, rval);
			fprintf(fp, "\n");
			break;
		}
	}

	mem_free(mctx);
}
