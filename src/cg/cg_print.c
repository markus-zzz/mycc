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

#include "cg_print.h"
#include "cg_tu.h"
#include "cg_data.h"
#include "cg_func.h"
#include "cg_bb.h"
#include "cg_instr.h"
#include "cg_reg.h"
#include "util/bset.h"
#include "util/dset.h"

static const char *op2str[] = {
#define DEF_CG_INSTR(x) #x,
#include "cg_instr.def"
#undef DEF_CG_INSTR
};

static const char *reg2str[] = {
#define DEF_CG_REG(x) #x,
#include "cg_reg.def"
#undef DEF_CG_REG
};

static const char *cond2str[] = {
#define DEF_CG_COND(x) #x,
#include "cg/cg_cond.def"
#undef DEF_CG_COND
};

static int
is_load(cg_instr *instr)
{
	switch (instr->op) {
		case CG_INSTR_OP_ldrb:
		case CG_INSTR_OP_ldrh:
		case CG_INSTR_OP_ldr:
			return 1;
		default:
			return 0;
	}
}

static int
is_store(cg_instr *instr)
{
	switch (instr->op) {
		case CG_INSTR_OP_strb:
		case CG_INSTR_OP_strh:
		case CG_INSTR_OP_str:
			return 1;
		default:
			return 0;
	}
}

static const char *get_regstr(int reg)
{
	static char buf[16];
	if (reg >= CG_REG_VREG0)
	{
		snprintf(buf, sizeof(buf), "%%v%d", reg);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%%%s", reg2str[reg]);
	}
	return buf;
}

void
cg_print_instr(FILE *fp, cg_instr *instr)
{
	if (instr->op == CG_INSTR_OP_phi)
	{
		cg_instr_phi_arg_iter it;
		cg_instr *arg;
		cg_bb *arg_bb;
		unsigned idx = 0;

		fprintf(fp, "%s = %s ", get_regstr(instr->reg), op2str[instr->op]);

		cg_instr_phi_arg_iter_init(&it, instr);
		while ((arg = cg_instr_phi_arg_iter_next(&it, &arg_bb)))
		{
			assert(arg->reg >= 0);
			if (idx++ > 0)
			{
				fprintf(fp, ", ");
			}
			fprintf(fp, "[%s, %%bb%d]", get_regstr(arg->reg), arg_bb->id);
		}
	}
	else
	{
		int i;
		if (instr->reg >= 0)
		{
			fprintf(fp, "%s = ", get_regstr(instr->reg));
		}

		if (instr->cond == CG_COND_al)
		{
			fprintf(fp, "%s", op2str[instr->op]);
		}
		else
		{
			fprintf(fp, "%s{%s}", op2str[instr->op], cond2str[instr->cond]);
		}

		for (i = 0; i < CG_INSTR_N_ARGS; i++)
		{
			if (instr->args[i].kind == CG_INSTR_ARG_HREG || instr->args[i].kind == CG_INSTR_ARG_VREG)
			{
				int reg;
				if (instr->args[i].kind == CG_INSTR_ARG_HREG)
				{
					reg = instr->args[i].u.hreg;
				}
				else
				{
	 				cg_instr *arg = (cg_instr *)graph_edge_tail(instr->args[i].u.vreg);
					reg = arg->reg;
				}

				assert(reg >= 0);

				if ((is_load(instr) && i == 0) || (is_store(instr) && i == 1))
				{
					if (instr->args[i].offset > 0)
					{
						fprintf(fp, " [%s, #0x%x]", get_regstr(reg), instr->args[i].offset);
					}
					else
					{
						fprintf(fp, " [%s]", get_regstr(reg));
					}
				}
				else
				{
					fprintf(fp, " %s", get_regstr(reg));
				}
			}
			else if (instr->args[i].kind == CG_INSTR_ARG_IMM)
			{
				fprintf(fp, " #0x%x", instr->args[i].u.imm);
			}
			else if (instr->args[i].kind == CG_INSTR_ARG_SYM)
			{
				fprintf(fp, " @%s", instr->args[i].u.sym);
			}

			if (i == 0 && instr->args[1].kind != CG_INSTR_ARG_INVALID)
			{
				fprintf(fp, ",");
			}
		}
	}
}

void
cg_print_func(FILE *fp, cg_func *func)
{
	cg_bb *bb;
	unsigned i;

	fprintf(fp, "define @%s( ", func->name);
	for (i = 0; i < N_ARRAY_SIZE(func->args) && func->args[i]; i++)
	{
		fprintf(fp, "%s ", get_regstr(func->args[i]->reg));
	}
	fprintf(fp, ") [0x%x,0x%x] {\n", func->stack_frame_size, func->clobber_mask);

	for (bb = func->bb_first; bb != NULL; bb = bb->bb_next)
	{
		graph_edge *edge;
		cg_instr *instr;
		fprintf(fp, "bb%d: ;; loop{nest=%d, type=%d, pre=%d, rpost=%d}\n", bb->id, cg_bb_loop_nest(bb), bb->loop_info.type, bb->loop_info.pre, bb->loop_info.rpost);
		for (instr = bb->instr_phi_first; instr != NULL; instr = instr->instr_next)
		{
			fprintf(fp, "  ");
			cg_print_instr(fp, instr);
			fprintf(fp, "\n");
		}
		for (instr = bb->instr_first; instr != NULL; instr = instr->instr_next)
		{
			fprintf(fp, "  ");
			cg_print_instr(fp, instr);
			fprintf(fp, "\n");
		}

		if ((edge = graph_succ_first((graph_node *)bb)))
		{
			if (graph_succ_next(edge))
			{
				assert(bb->true_target != NULL);
				assert(bb->false_target != NULL);
				fprintf(fp, "  branch{%s} %%bb%d, %%bb%d\n", cond2str[bb->true_cond], bb->true_target->id, bb->false_target->id);
			}
			else
			{
				edge = graph_succ_first((graph_node *)bb);
				cg_bb *succ = (cg_bb *)graph_edge_head(edge);
				assert(succ != NULL);
				assert(graph_succ_next(edge) == NULL);
				assert(bb->true_target == NULL);
				assert(bb->false_target == NULL);
				fprintf(fp, "  branch %%bb%d\n", succ->id);
			}
		}
	}
	fprintf(fp, "}\n");
}

void
cg_print_tu(FILE *fp, cg_tu *tu)
{
	cg_data *d;
	cg_func *f;

	for (d = tu->data_first; d != NULL; d = d->data_next)
	{
		int i;
		fprintf(fp, "@%s = size(%d), align(%d)", d->name, d->size, d->align);

		if (d->init != NULL)
		{
			fprintf(fp, " , init(");
			for (i = 0; i < d->size; i++)
			{
				fprintf(fp, "0x%02x%s", d->init[i], (i < d->size - 1) ? ", " : "");
			}
			fprintf(fp, ")");
		}
		fprintf(fp, "\n");
	}

	fprintf(fp, "\n");

	for (f = tu->func_first; f != NULL; f = f->func_next)
	{
		cg_print_func(fp, f);
		fprintf(fp, "\n");
	}
}
