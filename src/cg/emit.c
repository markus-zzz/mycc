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

#include "emit.h"
#include "cg/cg_data.h"
#include "cg/cg_func.h"
#include "cg/cg_tu.h"
#include "cg/cg_bb.h"
#include "cg/cg_instr.h"
#include "cg/cg_reg.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

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

void
cg_emit_instr(FILE *fp, cg_instr *instr)
{
	int i;
	int need_comma = 0;
	const char *mnemonic = op2str[instr->op];

	assert(instr->reg < CG_REG_VREG0);

	if (instr->op == CG_INSTR_OP_mov && (instr->args[0].kind == CG_INSTR_ARG_SYM || (instr->args[0].kind == CG_INSTR_ARG_IMM && instr->args[0].u.imm > 0xff)))
	{
		mnemonic = "ldr";
	}
	if (instr->op == CG_INSTR_OP_call)
	{
		mnemonic = "blx";
	}

	if (instr->cond == CG_COND_al)
	{
		fprintf(fp, "\t%s", mnemonic);
	}
	else
	{
		fprintf(fp, "\t%s%s", mnemonic, cond2str[instr->cond]);
	}

	if (instr->reg >= 0 && instr->op != CG_INSTR_OP_call)
	{
		fprintf(fp, " %s", reg2str[instr->reg]);
		need_comma = 1;
	}

	for (i = 0; i < CG_INSTR_N_ARGS; i++)
	{
		if (instr->args[i].kind != CG_INSTR_ARG_INVALID && need_comma)
		{
			fprintf(fp, ",");
		}

		assert(instr->args[i].kind != CG_INSTR_ARG_VREG && "Should be all HREG when we get here!");
		if (instr->args[i].kind == CG_INSTR_ARG_HREG)
		{
			if ((is_load(instr) && i == 0) || (is_store(instr) && i == 1))
			{
				if (instr->args[i].offset > 0)
				{
					fprintf(fp, " [%s, #0x%x]", reg2str[instr->args[i].u.hreg], instr->args[i].offset);
				}
				else
				{
					fprintf(fp, " [%s]", reg2str[instr->args[i].u.hreg]);
				}
			}
			else
			{
				fprintf(fp, " %s", reg2str[instr->args[i].u.hreg]);
			}
		}
		else if (instr->args[i].kind == CG_INSTR_ARG_IMM)
		{
			if (instr->op == CG_INSTR_OP_mov && instr->args[i].u.imm > 0xff)
			{
				fprintf(fp, " =#0x%x", instr->args[i].u.imm);
			}
			else
			{
				fprintf(fp, " #0x%x", instr->args[i].u.imm);
			}
		}
		else if (instr->args[i].kind == CG_INSTR_ARG_SYM)
		{
			if (instr->op == CG_INSTR_OP_mov)
			{
				fprintf(fp, " =%s", instr->args[i].u.sym);
			}
			else
			{
				fprintf(fp, " %s", instr->args[i].u.sym);
			}
		}
		need_comma = 1;

		if (instr->op == CG_INSTR_OP_call)
		{
			/* only emit first argument */
			break;
		}
	}

	fprintf(fp, "\n");
}

void
cg_emit_func(FILE *fp, cg_func *f)
{
	cg_bb *bb;

	fprintf(fp, "\n");
	fprintf(fp, "\t.align 4\n");
	fprintf(fp, "\t.global %s\n", f->name);
	fprintf(fp, "\t.arm\n");
	fprintf(fp, "\t.type %s, %%function\n", f->name);
	fprintf(fp, "%s:\n", f->name);

	{
		unsigned i;
		fprintf(fp, "\tstmdb sp!, {");
		for (i = 0; i < CG_REG_VREG0; i++)
		{
			if (f->clobber_mask & (1 << i))
			{
				fprintf(fp, "%s%s", reg2str[i], (f->clobber_mask >> (i + 1)) ? "," : "");
			}
		}
		fprintf(fp, "}\n");
		if (f->stack_frame_size > 0)
		{
			fprintf(fp, "\tsub sp, sp, #0x%x\n", f->stack_frame_size);
		}
	}

	for (bb = f->bb_first; bb != NULL; bb = bb->bb_next)
	{
		cg_instr *instr;
		fprintf(fp, ".%s_%03d:\n", f->name, bb->id);
		for (instr = bb->instr_first; instr != NULL; instr = instr->instr_next)
		{
			cg_emit_instr(fp, instr);
		}

		if (bb->true_target != NULL)
		{
			assert(bb->true_target != bb->bb_next);
			fprintf(fp, "\tb%s .%s_%03d\n", cond2str[bb->true_cond], f->name, bb->true_target->id);
		}
		else if (graph_succ_first((graph_node *)bb))
		{
			cg_bb *succ = (cg_bb *)graph_edge_head(graph_succ_first((graph_node *)bb));
			if (succ != bb->bb_next)
			{
				fprintf(fp, "\tb .%s_%03d\n", f->name, succ->id);
			}
		}
		else
		{
			unsigned i;

			if (f->stack_frame_size > 0)
			{
				fprintf(fp, "\tadd sp, sp, #0x%x\n", f->stack_frame_size);
			}
			fprintf(fp, "\tldmia sp!, {");
			for (i = 0; i < CG_REG_VREG0; i++)
			{
				if (f->clobber_mask & (1 << i))
				{
					fprintf(fp, "%s%s", reg2str[i], (f->clobber_mask >> (i + 1)) ? "," : "");
				}
			}
			fprintf(fp, "}\n");
			fprintf(fp, "\tbx lr\n");
		}
	}
}

void
cg_emit_tu(FILE *fp, cg_tu *tu)
{
	cg_data *d;
	cg_func *f;

	fprintf(fp, "\t.syntax unified\n");
	fprintf(fp, "\t.arch armv7-a\n");

	fprintf(fp, "\t.section .data\n\n");

	for (d = tu->data_first; d != NULL; d = d->data_next)
	{
		unsigned i;
		fprintf(fp, "\t.align %d\n", d->align);
		fprintf(fp, "%s:\n", d->name);
		if (d->init != NULL)
			{
			for (i = 0; i < d->size; i++)
			{
				fprintf(fp, "\t.byte 0x%02x\n", d->init[i]);
			}
		}
		fprintf(fp, "\n");
	}

	for (f = tu->func_first; f != NULL; f = f->func_next)
	{
		cg_emit_func(fp, f);
	}
}
