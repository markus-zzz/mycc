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
#include "ir_func.h"
#include "ir_print.h"
#include "ir_type.h"
#include "ir_validate.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static void
report_error(ir_node *n)
{
	ir_node *args[16];
	unsigned i, n_args;

	fprintf(stderr, "=== IR validation failure ===\n");

	if (n->op == IR_OP_phi)
	{
		ir_node_phi_arg_iter it;
		ir_node *arg;
		ir_node_phi_arg_iter_init(&it, n);
		while ((arg = ir_node_phi_arg_iter_next(&it, NULL)))
		{
			ir_print_node(stderr, arg);
			fprintf(stderr, "\n");
		}
	}
	else
	{
		ir_node_get_args(n, &n_args, args, sizeof(args)/sizeof(args[0]));
		for (i = 0; i < n_args; i++)
		{
			ir_print_node(stderr, args[i]);
			fprintf(stderr, "\n");
		}
	}

	ir_print_node(stderr, n);
	fprintf(stderr, "\n");

	exit(1);
}

void
ir_validate_node(ir_node *n)
{
	ir_node_phi_arg_iter it;
	ir_node *arg, *args[16];
	unsigned i, n_args;

	switch (n->op)
	{
	case IR_OP_const:
	case IR_OP_undef:
		/* Always valid */
		break;

	case IR_OP_phi:

		ir_node_phi_arg_iter_init(&it, n);
		while ((arg = ir_node_phi_arg_iter_next(&it, NULL)))
		{
			if (n->type != arg->type)
			{
				report_error(n);
			}
		}
		break;

	case IR_OP_ret:
	case IR_OP_br:
		break;
	case IR_OP_call:

		ir_node_get_args(n, &n_args, args, sizeof(args)/sizeof(args[0]));
		if (n_args < n->u.call.target->n_params ||
		    (!n->u.call.target->is_variadic && n_args > n->u.call.target->n_params))
		{
			report_error(n);
		}
		for (i = 0; i < n->u.call.target->n_params; i++)
		{
			if (args[i]->type != n->u.call.target->param_types[i])
			{
				report_error(n);
			}
		}
		break;

	case IR_OP_add:
	case IR_OP_sub:

		ir_node_get_args(n, &n_args, args, sizeof(args)/sizeof(args[0]));
		for (i = 0; i < n_args; i++)
		{
			if (ir_type_bytes(n->type) != ir_type_bytes(args[i]->type))
			{
				report_error(n);
			}
		}
		break;

	case IR_OP_neg:
	case IR_OP_mul:
	case IR_OP_udiv:
	case IR_OP_sdiv:
	case IR_OP_urem:
	case IR_OP_srem:

	case IR_OP_shl:
	case IR_OP_lshr:
	case IR_OP_ashr:

	case IR_OP_and:
	case IR_OP_not:
	case IR_OP_or:
	case IR_OP_xor:

		ir_node_get_args(n, &n_args, args, sizeof(args)/sizeof(args[0]));
		for (i = 0; i < n_args; i++)
		{
			if (n->type != args[i]->type)
			{
				report_error(n);
			}
		}
		break;

	case IR_OP_addr_of:
	case IR_OP_alloca:

		if (n->type != p32)
		{
			report_error(n);
		}
		break;

	case IR_OP_load:
	case IR_OP_store:

		ir_node_get_args(n, &n_args, args, sizeof(args)/sizeof(args[0]));
		if (args[0]->type != p32)
		{
			report_error(n);
		}
		if (n->op == IR_OP_store && n->type != args[1]->type)
		{
			report_error(n);
		}
		break;

	case IR_OP_trunc:

		ir_node_get_args(n, &n_args, args, sizeof(args)/sizeof(args[0]));
		if (ir_type_bytes(n->type) >= ir_type_bytes(args[0]->type))
		{
			report_error(n);
		}
		break;

	case IR_OP_sext:
	case IR_OP_zext:

		ir_node_get_args(n, &n_args, args, sizeof(args)/sizeof(args[0]));
		if (ir_type_bytes(n->type) <= ir_type_bytes(args[0]->type))
		{
			report_error(n);
		}
		break;

	case IR_OP_icmp_eq:
	case IR_OP_icmp_ne:
	case IR_OP_icmp_slt:
	case IR_OP_icmp_sle:
	case IR_OP_icmp_sgt:
	case IR_OP_icmp_sge:
	case IR_OP_icmp_ult:
	case IR_OP_icmp_ule:
	case IR_OP_icmp_ugt:
	case IR_OP_icmp_uge:

		ir_node_get_args(n, &n_args, args, sizeof(args)/sizeof(args[0]));
		if (n->type != args[0]->type || args[0]->type != args[1]->type)
		{
			report_error(n);
		}
		break;

	case IR_OP_term:
		break;

	case IR_OP_getparam:

		if (n->u.getparam.idx >= n->bb->func->n_params ||
		    n->type != n->bb->func->param_types[n->u.getparam.idx])
		{
			report_error(n);
		}
		break;

	}
}
