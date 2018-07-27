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

#include "frontend/ast_node.h"
#include "frontend/ast_type.h"
#include "frontend/symbol.h"
#include "ir/ir_bb.h"
#include "ir/ir_func.h"
#include "ir/ir_data.h"
#include "ir/ir_tu.h"
#include "ir/ir_node.h"
#include "ir/ir_type.h"
#include "ir/ir_print.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define D(x)

typedef struct ast2ir_ctx {
	ir_tu *tu;
	ir_func *func;
	ir_bb *entry_bb;
	ir_bb *exit_bb;
	ir_bb *current_bb;
	ir_bb *break_bb;
	ir_bb *continue_bb;
	ir_node *rval_alloca;
	int sym_cntr;
} ast2ir_ctx;

static ir_node * build_expr(ast2ir_ctx *ctx, ast_node *ast_expr);


static void fill_type_info(type_info *ti, ast_node *ts)
{
	assert(ts->op == AST_OP_TYPE_SPECIFIER);
	switch (ts->child->op)
	{
	case AST_OP_TYPE_NAME:
		fill_type_info(ti, ast_type_lookup(ts->child->u.strvalue));
		break;
	case AST_OP_INT:
		ti->basic_type = T_SIGNED_INT;
		break;
	case AST_OP_SHORT:
		ti->basic_type = T_SIGNED_SHORT;
		break;
	case AST_OP_CHAR:
		ti->basic_type = T_SIGNED_CHAR;
		break;
	default:
		ti->basic_type = T_INVALID;
		break;
	}
	ti->pointed_to_type = NULL;
}

static ir_node *
apply_usual_unary_conv(ast2ir_ctx *ctx, type_info *tin, ir_node *n, type_info *tout)
{
	ir_node *res;
	/* page 175 */
	switch (tin->basic_type)
	{
	case T_SIGNED_CHAR:
	case T_SIGNED_SHORT:
		tout->basic_type = T_SIGNED_INT;
		res = ir_node_build1(ctx->current_bb, IR_OP_sext, i32, n);
		break;
	case T_UNSIGNED_CHAR:
	case T_UNSIGNED_SHORT:
		tout->basic_type = T_SIGNED_INT;
		res = ir_node_build1(ctx->current_bb, IR_OP_zext, i32, n);
		break;
	default:
		tout->basic_type = tin->basic_type;
		res = n;
		break;
	}

	return res;
}

static type_info
apply_usual_binary_conv(ast2ir_ctx *ctx, type_info ti1, ir_node *ir1, type_info ti2, ir_node *ir2)
{
	assert(ti1.basic_type != T_INVALID);
	assert(ti1.basic_type == ti2.basic_type);
	return ti1;
}

static ir_node *
apply_usual_assign_conv(ast2ir_ctx *ctx, type_info *lhs_ti, type_info *rhs_ti, ir_node *rhs_n)
{
	ir_node *res = rhs_n;

	assert(lhs_ti->basic_type != T_INVALID);
	assert(rhs_ti->basic_type != T_INVALID);

	if (lhs_ti->basic_type != rhs_ti->basic_type)
	{
		switch (lhs_ti->basic_type)
		{
		case T_SIGNED_CHAR:
		case T_UNSIGNED_CHAR:
			switch (rhs_ti->basic_type)
			{
			case T_SIGNED_CHAR:
			case T_UNSIGNED_CHAR:
				/* do nothing */
				break;
			case T_SIGNED_SHORT:
			case T_UNSIGNED_SHORT:
			case T_SIGNED_INT:
			case T_UNSIGNED_INT:
			case T_SIGNED_LONG:
			case T_UNSIGNED_LONG:
				res = ir_node_build1(ctx->current_bb, IR_OP_trunc, i8, rhs_n);
				break;
			default:
				assert(0);
				break;
			}
			break;

		case T_SIGNED_SHORT:
		case T_UNSIGNED_SHORT:
			switch (rhs_ti->basic_type)
			{
			case T_SIGNED_CHAR:
				res = ir_node_build1(ctx->current_bb, IR_OP_sext, i16, rhs_n);
				break;
			case T_UNSIGNED_CHAR:
				res = ir_node_build1(ctx->current_bb, IR_OP_zext, i16, rhs_n);
				break;
			case T_SIGNED_SHORT:
			case T_UNSIGNED_SHORT:
				/* do nothing */
				break;
			case T_SIGNED_INT:
			case T_UNSIGNED_INT:
			case T_SIGNED_LONG:
			case T_UNSIGNED_LONG:
				res = ir_node_build1(ctx->current_bb, IR_OP_trunc, i16, rhs_n);
				break;
			default:
				assert(0);
				break;
			}
			break;

		case T_SIGNED_INT:
		case T_UNSIGNED_INT:
			switch (rhs_ti->basic_type)
			{
			case T_SIGNED_CHAR:
			case T_SIGNED_SHORT:
				res = ir_node_build1(ctx->current_bb, IR_OP_sext, i32, rhs_n);
				break;
			case T_UNSIGNED_CHAR:
			case T_UNSIGNED_SHORT:
				res = ir_node_build1(ctx->current_bb, IR_OP_zext, i32, rhs_n);
				break;
			case T_SIGNED_INT:
			case T_UNSIGNED_INT:
				/* do nothing */
				break;
			case T_SIGNED_LONG:
			case T_UNSIGNED_LONG:
				res = ir_node_build1(ctx->current_bb, IR_OP_trunc, i32, rhs_n);
				break;
			default:
				assert(0);
				break;
			}
			break;
		default:
			assert(0);
			break;
		}
	}

	return res;
}

static ir_type type_get_ir(ast_node *ts)
{
	ir_type type = i32;
	assert(ts->op == AST_OP_TYPE_SPECIFIER);
	switch (ts->child->op)
	{
	case AST_OP_TYPE_NAME:
		type = type_get_ir(ast_type_lookup(ts->child->u.strvalue));
		break;
	case AST_OP_INT:
		type = i32;
		break;
	case AST_OP_SHORT:
		type = i16;
		break;
	case AST_OP_CHAR:
		type = i8;
		break;
	case AST_OP_VOID:
		type = vooid;
		break;
	default:
		assert(0);
	}
	return type;
}

static ir_op ast_op_2_ir_op(ast_op aop)
{
	ir_op iop = IR_OP_undef;

	switch (aop)
	{
	case AST_OP_MUL:
	case AST_OP_MUL_ASSIGN:
		iop = IR_OP_mul;
		break;
	case AST_OP_DIV:
	case AST_OP_DIV_ASSIGN:
		iop = IR_OP_sdiv;
		break;
	case AST_OP_REM:
	case AST_OP_MOD_ASSIGN:
		iop = IR_OP_srem;
		break;
	case AST_OP_ADD:
	case AST_OP_ADD_ASSIGN:
		iop = IR_OP_add;
		break;
	case AST_OP_SUB:
	case AST_OP_SUB_ASSIGN:
		iop = IR_OP_sub;
		break;
	case AST_OP_SHIFT_LEFT:
	case AST_OP_LEFT_ASSIGN:
		iop = IR_OP_shl;
		break;
	case AST_OP_SHIFT_RIGHT:
	case AST_OP_RIGHT_ASSIGN:
		iop = IR_OP_ashr;
		break;
	case AST_OP_AND:
	case AST_OP_AND_ASSIGN:
		iop = IR_OP_and;
		break;
	case AST_OP_XOR:
	case AST_OP_XOR_ASSIGN:
		iop = IR_OP_xor;
		break;
	case AST_OP_OR:
	case AST_OP_OR_ASSIGN:
		iop = IR_OP_or;
		break;

	case AST_OP_LT: iop = IR_OP_icmp_slt; break;
	case AST_OP_LE: iop = IR_OP_icmp_sle; break;
	case AST_OP_GT: iop = IR_OP_icmp_sgt; break;
	case AST_OP_GE: iop = IR_OP_icmp_sge; break;
	case AST_OP_EQ: iop = IR_OP_icmp_eq;  break;
	case AST_OP_NE: iop = IR_OP_icmp_ne;  break;

	default:
		break;
	}

	assert(iop != IR_OP_undef);

	return iop;
}

static ir_node * build_addr(ast2ir_ctx *ctx, ast_node *n, ast_node **type_spec, int *is_pointer, int *is_array)
{
	ir_node *addr;

	if (type_spec) *type_spec = NULL;
	if (is_pointer) *is_pointer = 0;
	if (is_array) *is_array = 0;

	if (n->op == AST_OP_IDENTIFIER)
	{
		symbol *sym = symbol_lookup(n->u.strvalue);
		addr = sym->alloca;
		if (type_spec) *type_spec = sym->type_spec;
		if (is_pointer) *is_pointer = sym->is_pointer;
		if (is_array) *is_array = sym->is_array;

		fill_type_info(&n->ti, sym->type_spec);
		if (sym->is_pointer)
		{
			n->ti.basic_type = T_POINTER;
		}
	}
	else if (n->op == AST_OP_MEMBER)
	{
		ast_node *child0 = n->child;
		ast_node *child1 = n->child->sibling;
		ast_node *type0;
		ast_node *type1;
		ir_node *off;
		unsigned u;
		int is_pointer0 = 0;
		int is_pointer1 = 0;

		addr = build_addr(ctx, child0, &type0, &is_pointer0, NULL);
		if (is_pointer0)
		{
			addr = ir_node_build1(ctx->current_bb, IR_OP_load, p32, addr);
		}
		assert(child1->op == AST_OP_IDENTIFIER);
		/* Find offset of child1 in type0. child1 has type1 */
		u = ast_type_get_member_offset(type0, child1->u.strvalue, &type1, &is_pointer1);
		off = ir_node_build_const(ctx->current_bb, i32, u);
		addr = ir_node_build2(ctx->current_bb, IR_OP_add, p32, addr, off);
		if (type_spec) *type_spec = type1;
		if (is_pointer) *is_pointer = is_pointer1;
	}
	else
	{
		assert(n->op == AST_OP_INDEX);

		ast_node *child0 = n->child;
		ast_node *child1 = n->child->sibling;
		ast_node *type0;
		ir_node *esize;
		ir_node *idx;
		ir_node *off;
		unsigned u;
		int is_pointer0;
		int is_array0;

		addr = build_addr(ctx, child0, &type0, &is_pointer0, &is_array0);
		assert(is_pointer0 || is_array0);
		if (is_pointer0)
		{
			addr = ir_node_build1(ctx->current_bb, IR_OP_load, p32, addr);
		}
		idx = build_expr(ctx, child1);
		u = ast_type_get_size(type0);
		esize = ir_node_build_const(ctx->current_bb, i32, u);
		off = ir_node_build2(ctx->current_bb, IR_OP_mul, i32, idx, esize);
		addr = ir_node_build2(ctx->current_bb, IR_OP_add, p32, addr, off);
		if (type_spec) *type_spec = type0;

		fill_type_info(&n->ti, type0);
	}

	return addr;
}

static ir_node * build_expr(ast2ir_ctx *ctx, ast_node *ast_expr)
{
	switch (ast_expr->op)
	{
	case AST_OP_CONSTANT:
		{
			ast_expr->ti.basic_type = T_SIGNED_INT;
			return ir_node_build_const(ctx->current_bb, i32, ast_expr->u.ivalue);
		}
		break;
	case AST_OP_ADDR_OF:
		{
			ir_node *addr;
			ast_node *type_spec;
			addr = build_addr(ctx, ast_expr->child, &type_spec, NULL, NULL);
			ast_expr->ti.basic_type = T_POINTER;
			ast_expr->ti.pointed_to_type = type_spec;
			return addr;
		}
		break;
	case AST_OP_VALUE_OF:
		{
			ir_node *addr;
			ir_node *tmp;
			ast_node *type_spec;
			addr = build_addr(ctx, ast_expr->child, &type_spec, NULL, NULL);
			ir_type type = type_get_ir(type_spec);
			tmp = ir_node_build1(ctx->current_bb, IR_OP_load, p32, addr);
			return ir_node_build1(ctx->current_bb, IR_OP_load, type, tmp);
		}
		break;
	case AST_OP_STRING_LITERAL:
		{
			ir_data *sym;
			char sym_name[128];
			char tmp_init[1024];
			char *p = ast_expr->u.strvalue;
			char *q = tmp_init;
			unsigned size = 0;

			assert(*p == '"');
			p++;
			while (*p != '"')
			{
				if (*p == '\\')
				{
					p++;
					switch (*p++)
					{
						case 'n': *q = '\n'; break;
						case 'r': *q = '\r'; break;
						case 't': *q = '\t'; break;
						case '"': *q = '\"'; break;
						case '0': *q = '\0'; break;
					}
				}
				else
				{
					*q = *p++;
				}
				q++;
				size++;
				assert(size < sizeof(tmp_init) - 1);
			}
			*q++ = '\0';
			size++;

			snprintf(sym_name, sizeof(sym_name), "string_literal_%03d", ctx->sym_cntr++);
			sym = ir_data_build(ctx->tu, sym_name, size, 4, (unsigned char *)tmp_init);
			return ir_node_build_addr_of(ctx->current_bb, sym);
		}
		break;
	case AST_OP_IDENTIFIER:
	case AST_OP_INDEX:
	case AST_OP_MEMBER:
		{
			ir_node *addr;
			ast_node *type_spec;
			int is_pointer;
			int is_array;
			addr = build_addr(ctx, ast_expr, &type_spec, &is_pointer, &is_array);
			ast_expr->type = type_spec;
			ast_expr->is_pointer = is_pointer;

			if (is_pointer)
			{
				ast_expr->ti.basic_type = T_POINTER;
				ast_expr->ti.pointed_to_type = type_spec;
			}
			else
			{
				fill_type_info(&ast_expr->ti, type_spec);
			}

			if (is_array)
			{
				return addr;
			}
			else
			{
				ir_type type = is_pointer ? p32 : type_get_ir(type_spec);
				return ir_node_build1(ctx->current_bb, IR_OP_load, type, addr);
			}
		}
		break;
	case AST_OP_CALL:
		{
			ast_node *target_id = ast_expr->child;
			ast_node *ast_func;
			ast_node *arg;
			ir_func *target_func;
			ir_node *args[16];
			unsigned n_args;

			for (target_func = ctx->tu->first_ir_func; target_func != NULL; target_func = target_func->tu_list_next)
			{
				if (strcmp(target_id->u.strvalue, target_func->name) == 0)
				{
					break;
				}
			}

			assert(target_func && "Call target not found");
			ast_func = (ast_node *)target_func->scratch;
			n_args = 0;
			for (arg = ast_expr->child->sibling; arg != NULL; arg = arg->sibling)
			{
				ir_node *tmp = build_expr(ctx, arg);
				/* tmp = apply_usual_assign_conv(ctx, &lhs->ti, &arg->ti, tmp); */
				args[n_args++] = tmp;
			}

			fill_type_info(&ast_expr->ti, ast_func->child->child);

			return ir_node_build_call(ctx->current_bb, target_func, target_func->ret_type, n_args, args);
		}
		break;
	case AST_OP_CAST:
		{
			ast_node *rhs = ast_expr->child->sibling;
			ast_node *ts;
			ir_node *rhs_ir;
			assert(ast_expr->child->op == AST_OP_SPECIFIER_QUALIFIER_LIST);
			ts = ast_expr->child->child;
			assert(ts->op == AST_OP_TYPE_SPECIFIER);
			fill_type_info(&ast_expr->ti, ts);
			rhs_ir = build_expr(ctx, rhs);
			return apply_usual_assign_conv(ctx, &ast_expr->ti, &rhs->ti, rhs_ir);
		}
		break;
	case AST_OP_ASSIGN:
		{
			ast_node *lhs = ast_expr->child;
			ast_node *rhs = ast_expr->child->sibling;
			ast_node *lhs_type;
			int lhs_is_pointer;
			ir_node *lhs_addr;
			ir_node *rhs_ir;
			ir_type type;

			lhs_addr = build_addr(ctx, lhs, &lhs_type, &lhs_is_pointer, NULL);
			type = lhs_is_pointer ? p32 : type_get_ir(lhs_type);
			rhs_ir = build_expr(ctx, rhs);
			rhs_ir = apply_usual_assign_conv(ctx, &lhs->ti, &rhs->ti, rhs_ir);
			(void)ir_node_build2(ctx->current_bb, IR_OP_store, type, lhs_addr, rhs_ir);
		}
		break;
	case AST_OP_MUL_ASSIGN:
	case AST_OP_DIV_ASSIGN:
	case AST_OP_MOD_ASSIGN:
	case AST_OP_ADD_ASSIGN:
	case AST_OP_SUB_ASSIGN:
	case AST_OP_LEFT_ASSIGN:
	case AST_OP_RIGHT_ASSIGN:
	case AST_OP_AND_ASSIGN:
	case AST_OP_XOR_ASSIGN:
	case AST_OP_OR_ASSIGN:
		{
			ast_node *lhs = ast_expr->child;
			ast_node *rhs = ast_expr->child->sibling;
			ast_node *lhs_type;
			ir_node *lhs_addr;
			ir_node *lhs_load;
			ir_node *rhs_ir;
			ir_op op;
			ir_type type;

			op = ast_op_2_ir_op(ast_expr->op);
			rhs_ir = build_expr(ctx, rhs);
			lhs_addr = build_addr(ctx, lhs, &lhs_type, NULL, NULL);
			type = type_get_ir(lhs_type);
			lhs_load = ir_node_build1(ctx->current_bb, IR_OP_load, type, lhs_addr);

			type_info ti1, ti2;
			lhs_load = apply_usual_unary_conv(ctx, &lhs->ti, lhs_load, &ti1);
			rhs_ir = apply_usual_unary_conv(ctx, &rhs->ti, rhs_ir, &ti2);
			ast_expr->ti = apply_usual_binary_conv(ctx, ti1, lhs_load, ti2, rhs_ir);

			rhs_ir = ir_node_build2(ctx->current_bb, op, ir_node_type(rhs_ir), lhs_load, rhs_ir);
			rhs_ir = apply_usual_assign_conv(ctx, &lhs->ti, &ti2, rhs_ir);

			(void)ir_node_build2(ctx->current_bb, IR_OP_store, type, lhs_addr, rhs_ir);
		}
		break;

	case AST_OP_PRE_DEC:
	case AST_OP_PRE_INC:
	case AST_OP_POST_DEC:
	case AST_OP_POST_INC:
		{
			ast_node *lhs = ast_expr->child;
			ast_node *lhs_type;
			ir_node *lhs_addr;
			ir_node *lhs_load;
			ir_node *lhs_tmp;
			ir_type type;
			ir_op op;
			int lhs_is_pointer;

			if (ast_expr->op == AST_OP_PRE_DEC || ast_expr->op == AST_OP_POST_DEC)
			{
				op = IR_OP_sub;
			}
			else
			{
				op = IR_OP_add;
			}

			lhs_addr = build_addr(ctx, lhs, &lhs_type, &lhs_is_pointer, NULL);
			ast_expr->ti = lhs->ti;
			if (lhs_is_pointer)
			{
				lhs_load = ir_node_build1(ctx->current_bb, IR_OP_load, p32, lhs_addr);
				lhs_tmp = ir_node_build_const(ctx->current_bb, i32, ast_type_get_size(lhs_type));
				lhs_tmp = ir_node_build2(ctx->current_bb, op, p32, lhs_load, lhs_tmp);
				(void)ir_node_build2(ctx->current_bb, IR_OP_store, p32, lhs_addr, lhs_tmp);
			}
			else
			{
				type = type_get_ir(lhs_type);
				lhs_load = ir_node_build1(ctx->current_bb, IR_OP_load, type, lhs_addr);
				lhs_tmp = ir_node_build_const(ctx->current_bb, type, 1);
				lhs_tmp = ir_node_build2(ctx->current_bb, op, type, lhs_load, lhs_tmp);
				(void)ir_node_build2(ctx->current_bb, IR_OP_store, type, lhs_addr, lhs_tmp);
			}

			if (ast_expr->op == AST_OP_PRE_DEC || ast_expr->op == AST_OP_PRE_INC)
			{
				return lhs_tmp;
			}
			else
			{
				return lhs_load;
			}
		}
		break;

	case AST_OP_ADD:
	case AST_OP_SUB:
	case AST_OP_MUL:
	case AST_OP_DIV:
	case AST_OP_REM:

	case AST_OP_SHIFT_LEFT:
	case AST_OP_SHIFT_RIGHT:

	case AST_OP_AND:
	case AST_OP_OR:
	case AST_OP_XOR:

	case AST_OP_LT:
	case AST_OP_LE:
	case AST_OP_GT:
	case AST_OP_GE:
	case AST_OP_EQ:
	case AST_OP_NE:

	case AST_OP_LOGICAL_AND:
	case AST_OP_LOGICAL_OR:
		{
			ast_node *child1 = ast_expr->child;
			ast_node *child2 = child1->sibling;
			ir_node *ir1 = build_expr(ctx, child1);
			ir_node *ir2 = build_expr(ctx, child2);

			type_info ti1, ti2;

			ir1 = apply_usual_unary_conv(ctx, &child1->ti, ir1, &ti1);
			ir2 = apply_usual_unary_conv(ctx, &child2->ti, ir2, &ti2);

			ast_expr->ti = apply_usual_binary_conv(ctx, ti1, ir1, ti2, ir2);

			ir_type type;
			ir_op op = ast_op_2_ir_op(ast_expr->op);
			type = ir_node_type(ir1);
			if (op == IR_OP_add || op == IR_OP_sub)
			{
				ir_node *stride;
			    if (ir_node_type(ir1) == p32)
				{
					assert(child1->is_pointer);
					assert(ir_node_type(ir2) != p32);
					type = ir_node_type(ir1);
					stride = ir_node_build_const(ctx->current_bb, ir_node_type(ir2), ast_type_get_size(child1->type));
					ir2 = ir_node_build2(ctx->current_bb, IR_OP_mul, ir_node_type(ir2), ir2, stride);
				}
				else if (ir_node_type(ir2) == p32)
				{
					assert(child2->is_pointer);
					assert(ir_node_type(ir1) != p32);
					type = ir_node_type(ir2);
					stride = ir_node_build_const(ctx->current_bb, ir_node_type(ir1), ast_type_get_size(child2->type));
					ir1 = ir_node_build2(ctx->current_bb, IR_OP_mul, ir_node_type(ir1), ir1, stride);
				}
			}
			ast_expr->is_pointer = child1->is_pointer | child2->is_pointer;
			ast_expr->type = child1->is_pointer ? child1->type : child2->type;
			return ir_node_build2(ctx->current_bb, op, type, ir1, ir2);
		}
		break;

	case AST_OP_CONDITIONAL:
		{
			ast_node *cond_expr  = ast_expr->child;
			ast_node *true_expr  = cond_expr->sibling;
			ast_node *false_expr = true_expr->sibling;
			ir_bb *true_bb, *false_bb, *cont_bb;
			ir_node *cond, *phi, *true_tmp, *false_tmp;
			ir_type type = i32;

			true_bb  = ir_bb_build(ctx->func);
			false_bb = ir_bb_build(ctx->func);
			cont_bb  = ir_bb_build(ctx->func);

			ir_bb_build_br(true_bb, cont_bb);
			ir_bb_build_br(false_bb, cont_bb);

			cond = build_expr(ctx, cond_expr);
			ir_bb_build_cond_br(ctx->current_bb, cond, true_bb, false_bb);
			phi = ir_node_build_phi(cont_bb, type);

			type_info ti1, ti2;

			ctx->current_bb = true_bb;
			true_tmp = build_expr(ctx, true_expr);
			true_tmp = apply_usual_unary_conv(ctx, &true_expr->ti, true_tmp, &ti1);

			ctx->current_bb = false_bb;
			false_tmp = build_expr(ctx, false_expr);
			false_tmp = apply_usual_unary_conv(ctx, &false_expr->ti, false_tmp, &ti2);

			ast_expr->ti = apply_usual_binary_conv(ctx, ti1, true_tmp, ti2, false_tmp);

			ir_node_add_phi_arg(phi, true_bb, true_tmp);
			ir_node_add_phi_arg(phi, false_bb, false_tmp);

			ctx->current_bb = cont_bb;
			return phi;
		}
		break;
	default:
		assert(0);
		break;
	}

	return NULL;
}

static void build_stmt(ast2ir_ctx *ctx, ast_node *ast_stmt)
{
	switch (ast_stmt->op)
	{
	case AST_OP_COMPOUND_STATEMENT:
		{
			ast_node *stmt;
			symbol_scope_push();
			for (stmt = ast_stmt->child; stmt != NULL; stmt = stmt->sibling)
			{
				build_stmt(ctx, stmt);
			}
			symbol_scope_pop();
		}
		break;

	case AST_OP_DECLARATION_LIST:
	case AST_OP_STATEMENT_LIST:
		{
			ast_node *stmt;
			for (stmt = ast_stmt->child; stmt != NULL; stmt = stmt->sibling)
			{
				build_stmt(ctx, stmt);
			}
		}
		break;

	case AST_OP_DECLARATION:
		{
			ast_node *n;
			symbol *sym;
			unsigned type_size;

			assert(ast_stmt->child->op == AST_OP_DECLARATION_SPECIFIERS);
			assert(ast_stmt->child->sibling->op == AST_OP_INIT_DECLARATOR_LIST);
			if (ast_stmt->child->child->op == AST_OP_TYPE_SPECIFIER &&
			    ast_stmt->child->child->child->op == AST_OP_STRUCT_OR_UNION_SPECIFIER &&
			    ast_stmt->child->child->child->child->op == AST_OP_STRUCT &&
			    ast_stmt->child->child->child->child->sibling->op == AST_OP_IDENTIFIER &&
			    ast_stmt->child->child->child->child->sibling->sibling != NULL)
			{
				const char *str = ast_stmt->child->child->child->child->sibling->u.strvalue;
				ast_type_insert(str, ast_stmt->child->child);
			}
			type_size = ast_type_get_size(ast_stmt->child->child);

			assert(ast_stmt->child->sibling->op == AST_OP_INIT_DECLARATOR_LIST);
			for (n = ast_stmt->child->sibling->child; n != NULL; n = n->sibling)
			{
				ast_node *id;
				ast_node *dd;
				unsigned array_size = 1;
				int is_pointer = 0;
				int is_array = 0;
				assert(n->op == AST_OP_INIT_DECLARATOR);
				assert(n->child->op == AST_OP_DECLARATOR);
				if (n->child->child->op == AST_OP_POINTER)
				{
					is_pointer = 1;
					dd = n->child->child->sibling;
				}
				else
				{
					dd = n->child->child;
				}
				assert(dd->op == AST_OP_DIRECT_DECLARATOR);
				if (dd->child->op == AST_OP_ARRAY)
				{
					ast_node *array = dd->child;
					assert(array->child->op == AST_OP_DIRECT_DECLARATOR);
					is_array = 1;
					id = array->child->child;
					if (array->child->sibling)
					{
						assert(array->child->sibling->op == AST_OP_CONSTANT);
						array_size = array->child->sibling->u.ivalue;
					}
					else
					{
						/* Not legal C anyway */
						array_size = 0;
					}
				}
				else
				{
					id = dd->child;
				}
				assert(id->op == AST_OP_IDENTIFIER);

				assert(!symbol_lookup(id->u.strvalue) &&
				       "Redeclaration of symbol!");

				sym = symbol_insert(id->u.strvalue);
				sym->type_spec = ast_stmt->child->child;
				sym->alloca = ir_node_build_alloca(ctx->entry_bb, (is_pointer ? 4 : type_size)*array_size, 1);
				sym->is_pointer = is_pointer;
				sym->is_array = is_array;
				D(printf("alloca %%%d is '%s'\n", sym->alloca->id, id->u.strvalue));



				if (n->child->sibling)
				{
					ir_node *in = build_expr(ctx, n->child->sibling);
					type_info lhs_ti;

					fill_type_info(&lhs_ti, sym->type_spec);
					in = apply_usual_assign_conv(ctx, &lhs_ti, &n->child->sibling->ti, in);

					ir_node_build2(ctx->current_bb, IR_OP_store, ir_node_type(in), sym->alloca, in);
				}
			 }
		}
		break;

	case AST_OP_STMT_IF:
		{
			ast_node *if_cond = ast_stmt->child;
			ast_node *if_then = if_cond->sibling;
			ir_node *cond;
			ir_bb *then_bb;
			ir_bb *cont_bb;

			then_bb = ir_bb_build(ctx->func);
			cont_bb = ir_bb_build(ctx->func);

			cond = build_expr(ctx, if_cond);
			ir_bb_build_cond_br(ctx->current_bb, cond, then_bb, cont_bb);

			ctx->current_bb = then_bb;
			build_stmt(ctx, if_then);
			ir_bb_build_br(ctx->current_bb, cont_bb);

			ctx->current_bb = cont_bb;
		}
		break;
	case AST_OP_STMT_IF_ELSE:
		{
			ast_node *if_cond = ast_stmt->child;
			ast_node *if_then = if_cond->sibling;
			ast_node *if_else = if_then->sibling;
			ir_node *cond;
			ir_bb *then_bb;
			ir_bb *else_bb;
			ir_bb *cont_bb;

			then_bb = ir_bb_build(ctx->func);
			else_bb = ir_bb_build(ctx->func);
			cont_bb = ir_bb_build(ctx->func);

			cond = build_expr(ctx, if_cond);
			ir_bb_build_cond_br(ctx->current_bb, cond, then_bb, else_bb);

			ctx->current_bb = then_bb;
			build_stmt(ctx, if_then);
			ir_bb_build_br(ctx->current_bb, cont_bb);

			ctx->current_bb = else_bb;
			build_stmt(ctx, if_else);
			ir_bb_build_br(ctx->current_bb, cont_bb);

			ctx->current_bb = cont_bb;
		}
		break;
	case AST_OP_STMT_FOR:
		{
			ast_node *init_stmt = ast_stmt->child;
			ast_node *cond_stmt = init_stmt->sibling;
			ast_node *incr_expr = cond_stmt->sibling;
			ast_node *loop_stmt = incr_expr->sibling;

			ir_node *cond;
			ir_bb *cond_bb;
			ir_bb *loop_bb;
			ir_bb *incr_bb;
			ir_bb *cont_bb;
			ir_bb *old_break_bb;
			ir_bb *old_continue_bb;

			cond_bb = ir_bb_build(ctx->func);
			loop_bb = ir_bb_build(ctx->func);
			incr_bb = ir_bb_build(ctx->func);
			cont_bb = ir_bb_build(ctx->func);

			old_break_bb = ctx->break_bb;
			ctx->break_bb = cont_bb;
			old_continue_bb = ctx->continue_bb;
			ctx->continue_bb = incr_bb;

			if (init_stmt->op != AST_OP_NIL)
			{
				build_stmt(ctx, init_stmt);
			}

			ir_bb_build_br(ctx->current_bb, cond_bb);

			ctx->current_bb = cond_bb;
			if (cond_stmt->op == AST_OP_EXPRESSION_STATEMENT && cond_stmt->child != NULL)
			{
				cond = build_expr(ctx, cond_stmt->child);
			}
			else
			{
				cond = ir_node_build_const(ctx->current_bb, i32, 1);
			}
			ir_bb_build_cond_br(ctx->current_bb, cond, loop_bb, cont_bb);

			ctx->current_bb = loop_bb;
			build_stmt(ctx, loop_stmt);
			ir_bb_build_br(ctx->current_bb, incr_bb);

			ctx->current_bb = incr_bb;
			if (incr_expr->op != AST_OP_NIL)
			{
				build_expr(ctx, incr_expr);
			}
			ir_bb_build_br(ctx->current_bb, cond_bb);

			ctx->current_bb = cont_bb;
			ctx->break_bb = old_break_bb;
			ctx->continue_bb = old_continue_bb;
		}
		break;
	case AST_OP_STMT_WHILE:
		{
			ast_node *cond_expr = ast_stmt->child;
			ast_node *loop_stmt = cond_expr->sibling;
			ir_node *cond;
			ir_bb *cond_bb;
			ir_bb *loop_bb;
			ir_bb *cont_bb;
			ir_bb *old_break_bb;
			ir_bb *old_continue_bb;

			cond_bb = ir_bb_build(ctx->func);
			loop_bb = ir_bb_build(ctx->func);
			cont_bb = ir_bb_build(ctx->func);

			old_break_bb = ctx->break_bb;
			ctx->break_bb = cont_bb;
			old_continue_bb = ctx->continue_bb;
			ctx->continue_bb = cond_bb;

			ir_bb_build_br(ctx->current_bb, cond_bb);

			ctx->current_bb = cond_bb;
			cond = build_expr(ctx, cond_expr);
			ir_bb_build_cond_br(ctx->current_bb, cond, loop_bb, cont_bb);

			ctx->current_bb = loop_bb;
			build_stmt(ctx, loop_stmt);
			ir_bb_build_br(ctx->current_bb, cond_bb);

			ctx->current_bb = cont_bb;
			ctx->break_bb = old_break_bb;
			ctx->continue_bb = old_continue_bb;
		}
		break;
	case AST_OP_STMT_DO_WHILE:
		{
			ast_node *loop_stmt = ast_stmt->child;
			ast_node *cond_expr = loop_stmt->sibling;
			ir_node *cond;
			ir_bb *cond_bb;
			ir_bb *loop_bb;
			ir_bb *cont_bb;
			ir_bb *old_break_bb;
			ir_bb *old_continue_bb;

			loop_bb = ir_bb_build(ctx->func);
			cond_bb = ir_bb_build(ctx->func);
			cont_bb = ir_bb_build(ctx->func);

			old_break_bb = ctx->break_bb;
			ctx->break_bb = cont_bb;
			old_continue_bb = ctx->continue_bb;
			ctx->continue_bb = cond_bb;

			ir_bb_build_br(ctx->current_bb, loop_bb);

			ctx->current_bb = loop_bb;
			build_stmt(ctx, loop_stmt);
			ir_bb_build_br(ctx->current_bb, cond_bb);

			ctx->current_bb = cond_bb;
			cond = build_expr(ctx, cond_expr);
			ir_bb_build_cond_br(ctx->current_bb, cond, loop_bb, cont_bb);

			ctx->current_bb = cont_bb;
			ctx->break_bb = old_break_bb;
			ctx->continue_bb = old_continue_bb;
		}
		break;
	case AST_OP_STMT_BREAK:
		{
			ir_bb_build_br(ctx->current_bb, ctx->break_bb);
			ctx->current_bb = ir_bb_build(ctx->func);
		}
		break;
	case AST_OP_STMT_CONTINUE:
		{
			ir_bb_build_br(ctx->current_bb, ctx->continue_bb);
			ctx->current_bb = ir_bb_build(ctx->func);
		}
		break;
	case AST_OP_STMT_RETURN:
		{
			if (ast_stmt->child != NULL)
			{
				ir_node *rval = build_expr(ctx, ast_stmt->child);
				assert(ctx->rval_alloca && "Function declared to return void");
				ir_node_build2(ctx->current_bb, IR_OP_store, ctx->func->ret_type, ctx->rval_alloca, rval);
			}
			ir_bb_build_br(ctx->current_bb, ctx->exit_bb);
			/*	TODO:FIXME: uncomment		ctx->current_bb = ir_bb_build(ctx->func); */
		}
		break;
	case AST_OP_EXPRESSION_STATEMENT:
		{
			(void)build_expr(ctx, ast_stmt->child);
		}
		break;
	default:
		assert(0);
		break;
	}
}

static void handle_declaration(ast2ir_ctx *ctx, ast_node *ast_decl)
{
	ast_node *dd, *ds, *dr, *id, *pd, *tmp;

	assert(ast_decl->op == AST_OP_DECLARATION);

	if (ast_decl->child->op == AST_OP_DECLARATION_SPECIFIERS &&
	    ast_decl->child->child->op == AST_OP_STORAGE_CLASS_SPECIFIER &&
	    ast_decl->child->child->child->op == AST_OP_TYPEDEF)
	{
		/* typedefs already handled */
		return;
	}

	ds = ast_decl->child;
	assert(ds->op == AST_OP_DECLARATION_SPECIFIERS);
	dr = ast_decl->child->sibling;
	assert(dr->op == AST_OP_INIT_DECLARATOR_LIST);
	assert(dr->child->op == AST_OP_INIT_DECLARATOR);
	assert(dr->child->child->op == AST_OP_DECLARATOR);
	dr = dr->child->child;

	tmp = dr->child->op == AST_OP_POINTER ? dr->child->sibling : dr->child;
	assert(tmp->op == AST_OP_DIRECT_DECLARATOR);
	dd = tmp->child->op == AST_OP_DIRECT_DECLARATOR ? tmp->child : tmp;
	id = dd->child;
	assert(id->op == AST_OP_IDENTIFIER);

	if (dd->sibling && dd->sibling->op == AST_OP_PARAMETER_TYPE_LIST)
	{
		/* function declaration (with prototype) */
		ir_type ret_type, param_types[16];
		ir_func *func;
		unsigned n_params = 0;
		int is_variadic = 0;

		ret_type = dr->child->op == AST_OP_POINTER ? p32 : type_get_ir(ds->child);

		for (pd = dd->sibling->child; pd != NULL; pd = pd->sibling)
		{
			if (pd->op == AST_OP_PARAMETER_DECLARATION)
			{
				assert(pd->child->op == AST_OP_DECLARATION_SPECIFIERS);
				if (pd->child->sibling && pd->child->sibling->child->op == AST_OP_POINTER)
				{
					param_types[n_params++] = p32;
				}
				else
				{
					param_types[n_params++] = type_get_ir(pd->child->child);
				}
			}
			else
			{
				assert(pd->op == AST_OP_ELLIPSIS);
				assert(pd->sibling == NULL);
				is_variadic = 1;
			}
		}

		if (is_variadic)
		{
			func = ir_func_variadic_build(ctx->tu, id->u.strvalue, ret_type, n_params, param_types);
		}
		else
		{
			func = ir_func_build(ctx->tu, id->u.strvalue, ret_type, n_params, param_types);
		}

		func->scratch = ast_decl;
	}
	else
	{
		/* global variable declaration */
		unsigned size = ast_type_get_size(ds->child);
		(void)ir_data_build(ctx->tu, id->u.strvalue, size, 4, NULL);
	}
}

static void handle_function_definition(ast2ir_ctx *ctx, ast_node *ast_func) /* TODO:FIXME: Can merge with build_function_declaration? */
{
	ast_node *ds, *dr, *cs, *id, *pd;
	ir_type ret_type, param_types[16];
	unsigned n_params, idx;

	assert(ast_func->op == AST_OP_FUNCTION_DEF);
	ds = ast_func->child;
	dr = ast_func->child->sibling;
	cs = ast_func->child->sibling->sibling;
	assert(ds->op == AST_OP_DECLARATION_SPECIFIERS);
	assert(dr->op == AST_OP_DECLARATOR);
	assert(cs->op == AST_OP_COMPOUND_STATEMENT);

	ret_type = type_get_ir(ds->child);

	assert(dr->child->op == AST_OP_DIRECT_DECLARATOR);
	assert(dr->child->child->op == AST_OP_DIRECT_DECLARATOR);
	assert(dr->child->child->child->op == AST_OP_IDENTIFIER);
	id = dr->child->child->child;

	assert(dr->child->child->sibling->op == AST_OP_PARAMETER_TYPE_LIST);
	n_params = 0;
	for (pd = dr->child->child->sibling->child; pd != NULL; pd = pd->sibling)
	{
		assert(pd->op == AST_OP_PARAMETER_DECLARATION);
		assert(pd->child->op == AST_OP_DECLARATION_SPECIFIERS);
		if (pd->child->sibling && pd->child->sibling->child->op == AST_OP_POINTER)
		{
			param_types[n_params++] = p32;
		}
		else
		{
			param_types[n_params++] = type_get_ir(pd->child->child);
		}
	}

	ctx->func = ir_func_build(ctx->tu, id->u.strvalue, ret_type, n_params, param_types);
	ctx->entry_bb = ir_bb_build(ctx->func);
	ctx->exit_bb = ir_bb_build(ctx->func);

	ctx->func->scratch = ast_func;

	idx = 0;
	for (pd = dr->child->child->sibling->child; pd != NULL; pd = pd->sibling)
	{
		symbol *sym;
		unsigned size;
		int is_pointer = 0;
		ast_node *tmp;
		ir_node *n;

		assert(pd->op == AST_OP_PARAMETER_DECLARATION);
		assert(pd->child->op == AST_OP_DECLARATION_SPECIFIERS);
		if (pd->child->child->op == AST_OP_TYPE_SPECIFIER &&
		    pd->child->child->child->op == AST_OP_VOID)
		{
			break;
		}
		assert(pd->child->sibling->op == AST_OP_DECLARATOR);
		tmp = pd->child->sibling->child;
		if (tmp->op == AST_OP_POINTER)
		{
			is_pointer = 1;
			tmp = tmp->sibling;
		}
		assert(tmp->op == AST_OP_DIRECT_DECLARATOR);
		assert(tmp->child->op == AST_OP_IDENTIFIER);

		id = tmp->child;
		size = ast_type_get_size(pd->child->child);

		sym = symbol_insert(id->u.strvalue);
		sym->type_spec = pd->child->child;
		sym->alloca = ir_node_build_alloca(ctx->entry_bb, is_pointer ? 4 : size, 1);
		sym->is_pointer = is_pointer;

		n = ir_node_build_getparam(ctx->entry_bb, param_types[idx], idx);
		n = ir_node_build2(ctx->entry_bb, IR_OP_store, param_types[idx], sym->alloca, n);
		idx++;
	}

	if (ret_type != vooid)
	{
		ir_node *rval;
		ctx->rval_alloca = ir_node_build_alloca(ctx->entry_bb, 4, 1);
		rval = ir_node_build1(ctx->exit_bb, IR_OP_load, ctx->func->ret_type, ctx->rval_alloca);
		ir_bb_build_value_ret(ctx->exit_bb, rval);
	}
	else
	{
		ir_bb_build_ret(ctx->exit_bb);
	}

	ctx->current_bb = ctx->entry_bb;
	build_stmt(ctx, cs);

	ir_bb_build_br(ctx->current_bb, ctx->exit_bb);
}

ir_tu * ast_to_ir(ast_node *root)
{
	ast2ir_ctx ctx_, *ctx = &ctx_;
	ast_node *tu;
	ast_node *child;
	assert(root->op == AST_OP_TU);
	tu = root;

	memset(ctx, 0, sizeof(*ctx));

	ctx->tu = calloc(1, sizeof(ir_tu));

	for (child = tu->child; child != NULL; child = child->sibling)
	{
		if (child->op == AST_OP_FUNCTION_DEF)
		{
			symbol_scope_push();
			handle_function_definition(ctx, child);
			symbol_scope_pop();
		}
		else
		{
			assert(child->op == AST_OP_DECLARATION);
			handle_declaration(ctx, child);
		}
	}

	return ctx->tu;
}

