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
#include "frontend/ast_to_ir.h"
#include "c95.tab.h"
#include "ir/ir_tu.h"
#include "ir/ir_bb.h"
#include "ir/ir_dom.h"
#include "ir/ir_func.h"
#include "ir/ir_node.h"
#include "ir/ir_pass.h"
#include "ir/ir_print.h"
#include "ir_passes/mem2reg.h"
#include "test/ir_sim.h"
#include "cg/cg_import.h"
#include "cg/iselect.h"
#include "cg/regalloc_ssa.h"
#include "cg/cg_print.h"
#include "cg/emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ast_node *root;
extern FILE *yyin;
int yyparse();

void
cg_branch_predication_tu(cg_tu *tu);

static void dummy(ir_func *f)
{
	(void)f;
}

static ir_pass pristine = {
	"pristine",
	dummy
};

ir_pass *passlist[] = {
	&pristine,
	&mem2reg,
	NULL
};

static void help_exit(const char *prog)
{
	fprintf(stderr, "Usage: %s <input> [OPTIONS]\n", prog);
	fprintf(stderr, "  --dump-(all|ast|ir|cg)\n");
	fprintf(stderr, "  --sim-ir=<func>\n");
	fprintf(stderr, "  --cg-max-regs=<n>\n");
	fprintf(stderr, " The following options are for when codegen IR is imported only.\n");
	fprintf(stderr, "  --cg-import=<path>\n");
	fprintf(stderr, "  --cg-dump=<path>\n");
	fprintf(stderr, "  --cg-run-ra\n");
	fprintf(stderr, "  --cg-run-branch-predication\n");
	fprintf(stderr, "  --cg-run-emit=<path>\n");
	fprintf(stderr, "\n");

	exit(1);
}

static const char *match_opt_with_value(const char *argv, const char *str)
{
	if (!strncmp(argv, str, strlen(str)))
	{
		const char *tmp = &argv[strlen(str)];
		return (*tmp != '\0') ? tmp : NULL;
	}
	return NULL;
}

int main(int argc, char **argv)
{
	FILE *in = NULL;
	FILE *out = NULL;
	ir_tu *itu = NULL;
	cg_tu *ctu = NULL;
	int i;

	struct {
		int dump_ast, dump_ir, dump_cg;
		int cg_max_regs;
		const char *input;
		const char *sim_ir_func;
	} opt;

	memset(&opt, 0, sizeof(opt));

	for (i = 1; i < argc; i++)
	{
		const char *value;

		if (!strcmp(argv[i], "--dump-all"))
		{
			opt.dump_ast = 1;
			opt.dump_ir = 1;
			opt.dump_cg = 1;
		}
		else if (!strcmp(argv[i], "--dump-ast"))
		{
			opt.dump_ast = 1;
		}
		else if (!strcmp(argv[i], "--dump-ir"))
		{
			opt.dump_ir = 1;
		}
		else if (!strcmp(argv[i], "--dump-cg"))
		{
			opt.dump_cg = 1;
		}
		else if ((value = match_opt_with_value(argv[i], "--sim-ir=")))
		{
			opt.sim_ir_func = value;
		}
		else if ((value = match_opt_with_value(argv[i], "--cg-max-regs=")))
		{
			opt.cg_max_regs = strtol(value, NULL, 0);
		}
		else if ((value = match_opt_with_value(argv[i], "--cg-import=")))
		{
			ctu = cg_import(value);
		}
		else if ((value = match_opt_with_value(argv[i], "--cg-dump=")))
		{
			out = fopen(value, "w");
			cg_print_tu(out, ctu);
			fclose(out);
		}
		else if (!strcmp(argv[i], "--cg-run-ra"))
		{
			cg_regalloc_ssa_tu(ctu, opt.cg_max_regs);
			if (opt.dump_cg)
			{
				out = fopen("cg_01_regalloc.txt", "w");
				cg_print_tu(out, ctu);
				fclose(out);
			}
		}
		else if (!strcmp(argv[i], "--cg-run-branch-predication"))
		{
			cg_branch_predication_tu(ctu);
		}
		else if ((value = match_opt_with_value(argv[i], "--cg-run-emit=")))
		{
			out = fopen(value, "w");
			cg_emit_tu(out, ctu);
			fclose(out);
			exit(0);
		}
		else if (!strncmp(argv[i], "--", 2))
		{
			help_exit(argv[0]);
		}
		else
		{
			if (opt.input)
			{
				help_exit(argv[0]);
			}
			opt.input = argv[i];
		}
	}

	if (!opt.input)
	{
		help_exit(argv[0]);
	}

	if ((in = fopen(opt.input, "r")) == NULL)
	{
		fprintf(stderr, "%s: failed to open '%s'\n", argv[0], opt.input);
		exit(1);
	}
	yyin = in;
	do {
		yyparse();
	} while (!feof(in));
	fclose(in);

	if (opt.dump_ast)
	{
		out = fopen("ast_00_pristine.txt", "w");
		ast_node_dump_tree(out, root);
		fclose(out);
	}

	itu = ast_to_ir(root);

	for (i = 0; passlist[i] != NULL; i++)
	{
		char path[128];
		ir_func *f;
		for (f = itu->first_ir_func; f != NULL; f = f->tu_list_next)
		{
			if (ir_func_is_definition(f))
			{
				passlist[i]->func(f);
				ir_func_free_unused_nodes(f);
			}
		}

		if (opt.dump_ir)
		{
			snprintf(path, sizeof(path), "ir_%02d_%s.txt", i, passlist[i]->name);
			out = fopen(path, "w");
			ir_print_tu(out, itu);
			fclose(out);
		}

		if (opt.sim_ir_func)
		{
			snprintf(path, sizeof(path), "sim_%02d_%s.txt", i, passlist[i]->name);
			out = fopen(path, "w");
			ir_sim_func(out, itu, opt.sim_ir_func);
			fclose(out);
		}
	}

	{
		char path[128];
		ctu = cg_iselect_tu(itu);
		if (opt.dump_cg)
		{
			out = fopen("cg_00_iselect.txt", "w");
			cg_print_tu(out, ctu);
			fclose(out);
		}

		cg_regalloc_ssa_tu(ctu, opt.cg_max_regs);
		if (opt.dump_cg)
		{
			out = fopen("cg_01_regalloc.txt", "w");
			cg_print_tu(out, ctu);
			fclose(out);
		}

		cg_branch_predication_tu(ctu);
		if (opt.dump_cg)
		{
			out = fopen("cg_02_branch_predication.txt", "w");
			cg_print_tu(out, ctu);
			fclose(out);
		}

		snprintf(path, sizeof(path), "%s.s", opt.input);
		out = fopen(path, "w");
		cg_emit_tu(out, ctu);
		fclose(out);
	}

	return 0;
}
