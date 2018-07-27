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

#include "ir_print.h"

#include "ir_tu.h"
#include "ir_data.h"
#include "ir_func.h"
#include "ir_node_private.h"
#include "ir_bb_private.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

static const char *op2str[] = {
#define DEF_IR_OP(x) #x,
#include "ir_op.def"
#undef DEF_IR_OP
};

static const char *type2str[] = {
	"void",
	"i1",
	"i8",
	"i16",
	"i32",
	"i64",
	"p32",
	"p64"
};

void
ir_print_tu(FILE *fp, ir_tu *tu)
{
	ir_func *f;
	for (f = tu->first_ir_func; f != NULL; f = f->tu_list_next)
	{
		ir_print_func(fp, f);
		fprintf(fp, "\n");
	}
}

void
ir_print_func(FILE *fp, ir_func *func)
{
	const char *str;
	ir_bb_iter bit;
	ir_bb *bb;
	unsigned i;

	if (ir_func_is_definition(func))
	{
		str = "define";
	}
	else
	{
		assert(ir_func_is_declaration(func));
		str = "declare";
	}

	fprintf(fp, "%s %s @%s(", str, type2str[func->ret_type], func->name);
	for (i = 0; i < func->n_params; i++)
	{
		fprintf(fp, "%s%s", type2str[func->param_types[i]], i < func->n_params - 1 ? ", " : "");
	}
	fprintf(fp, ")");

	if (ir_func_is_definition(func))
	{
		fprintf(fp, " {\n");
		ir_bb_iter_init(&bit, func);
		while ((bb = ir_bb_iter_next(&bit)))
		{
			ir_print_bb(fp, bb);
		}
		fprintf(fp, "}\n");
	}
	fprintf(fp, "\n");
}

void
ir_print_bb(FILE *fp, ir_bb *bb)
{
	ir_node_iter nit;
	ir_node *n;

	fprintf(fp, " bb%d:\n", bb->id);
	ir_node_iter_init(&nit, bb);
	while ((n = ir_node_iter_next(&nit)))
	{
		fprintf(fp, "  ");
		ir_print_node(fp, n);
		fprintf(fp, "\n");
	}
	if (bb->term_kind == IR_BB_TERM_BR)
	{
		if (ir_bb_get_term_node(bb) == NULL)
		{
			ir_bb *target_bb = ir_bb_get_default_target(bb);
			fprintf(fp, "  br label %%bb%d\n", target_bb->id);
		}
		else
		{
			ir_bb *true_bb = ir_bb_get_true_target(bb);
			ir_bb *false_bb = ir_bb_get_false_target(bb);
			fprintf(fp, "  br %%%d, label %%bb%d, label %%bb%d\n", ir_bb_get_term_node(bb)->id, true_bb->id, false_bb->id);
		}
	}
	else
	{
		assert(bb->term_kind == IR_BB_TERM_RET);
		if (ir_bb_get_term_node(bb) == NULL)
		{
			fprintf(fp, "  ret\n");
		}
		else
		{
			fprintf(fp, "  ret %s %%%d\n", type2str[ir_bb_get_term_node(bb)->type], ir_bb_get_term_node(bb)->id);
		}
	}
	fprintf(fp, "\n");
}

void
ir_print_node(FILE *fp, ir_node *n)
{
	fprintf(fp, "%%%d = %s %s ", n->id, op2str[n->op], type2str[n->type]);

	if (n->op == IR_OP_addr_of)
	{
		fprintf(fp, "@%s", n->u.addr_of.sym->name);
	}
	else if (n->op == IR_OP_alloca)
	{
		fprintf(fp, "%d, %d", n->u.alloca.size, n->u.alloca.align);
	}
	else if (n->op == IR_OP_getparam)
	{
		fprintf(fp, "%d", n->u.getparam.idx);
	}
	else if (n->op == IR_OP_const)
	{
		switch (n->type) {
		case i1:
			fprintf(fp, "%s", n->u.constant.u.i1 ? "true" : "false");
			break;
		case i8:
			fprintf(fp, "0x%02hhx", n->u.constant.u.i8);
			break;
		case i16:
			fprintf(fp, "0x%04hx", n->u.constant.u.i16);
			break;
		case i32:
		case p32:
			fprintf(fp, "0x%08x", n->u.constant.u.i32);
			break;
		case i64:
		case p64:
			fprintf(fp, "0x%016llx", n->u.constant.u.i64);
			break;
		default:
			assert(0);
			break;
		}
	}
	else if (n->op == IR_OP_phi)
	{
		ir_node_phi_arg_iter it;
		ir_node *arg;
		ir_bb *arg_bb;
		unsigned idx = 0;

		ir_node_phi_arg_iter_init(&it, n);
		while ((arg = ir_node_phi_arg_iter_next(&it, &arg_bb)))
		{
			if (idx++ > 0)
			{
				fprintf(fp, ", ");
			}
			fprintf(fp, "[%%%d, %%bb%d]", arg->id, arg_bb->id);
		}
	}
	else
	{
		ir_node *args[8];
		unsigned n_args;
		unsigned i;

		if (n->op == IR_OP_call)
		{
			fprintf(fp, "@%s ", n->u.call.target->name);
		}

		ir_node_get_args(n, &n_args, args, sizeof(args)/sizeof(args[0]));
		assert(n_args <= sizeof(args)/sizeof(args[0]));
		for (i = 0; i < n_args; i++)
		{
			fprintf(fp, "%%%d", args[i]->id);
			if (i < n_args - 1)
			{
				fprintf(fp, ", ");
			}
		}
	}
}
