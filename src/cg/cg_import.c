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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cg/cg_tokens.h"
#include "cg/cg_tu.h"
#include "cg/cg_data.h"
#include "cg/cg_func.h"
#include "cg/cg_bb.h"
#include "cg/cg_reg.h"
#include "cg/cg_instr.h"

extern FILE *cg_yyin;
extern struct cg_yylval_type cg_yylval;
extern int cg_line;
extern int cg_column;

static struct {
	cg_instr *instr;
	int arg_idx;
	int arg_vreg;
} delayed_args[1024];
static int delayed_args_idx;

static struct {
	cg_instr *instr;
	int arg_vreg;
	int arg_bb;
} delayed_phi_args[1024];
static int delayed_phi_args_idx;

static struct {
	cg_bb *b;
	int n_succs;
	int succ_ids[2];
} delayed_succs[1024];
static int delayed_succs_idx;

int cg_yylex(void);

static cg_instr *vreg2instr[1024];
static cg_bb *bid2bb[1024];

static cg_token current_token;

static void
parse_error(void)
{
	fprintf(stderr, "Parse error at line %d, column %d\n", cg_line, cg_column);
	exit(1);
}

static cg_token
token_peek(void)
{
	return current_token;
}

static void
token_consume(void)
{
	current_token = cg_yylex();
}

static void
token_expect(cg_token tok, int consume)
{
	if (current_token == tok)
	{
		if (consume)
		{
			current_token = cg_yylex();
		}
	}
	else
	{
		parse_error();
	}
}

static void
instr_set_reg(cg_instr *instr, int reg)
{
	cg_func *f = instr->bb->func;
	instr->reg = reg;
	if (reg >= 0)
	{
		f->vreg_cntr = (f->vreg_cntr > reg) ? f->vreg_cntr : reg;
	}
}

static void
handle_define(cg_tu *tu)
{
	cg_func *f;
	cg_bb *b;

	token_expect(CG_TOK_define, 1);
	token_expect(CG_TOK_sym, 0);
	f = cg_func_build(tu, cg_yylval.strval);

	token_consume();
	token_expect(CG_TOK_lsquare, 1);
	token_expect(CG_TOK_integer, 0);
	f->stack_frame_size = cg_yylval.ival;
	token_consume();
	token_expect(CG_TOK_comma, 1);
	f->clobber_mask = cg_yylval.ival;
	token_consume();
	token_expect(CG_TOK_rsquare, 1);

	token_expect(CG_TOK_lbrace, 1);

	delayed_args_idx = 0;
	delayed_phi_args_idx = 0;
	delayed_succs_idx = 0;

	while (token_peek() != CG_TOK_rbrace)
	{
		token_expect(CG_TOK_bb_def, 0);
		b = cg_bb_build(f);
		b->id = cg_yylval.ival;
		cg_bb_link_last(b);
		token_consume();
		bid2bb[b->id] = b;
		while (token_peek() != CG_TOK_bb_def && token_peek() != CG_TOK_rbrace)
		{
			cg_instr_op op;
			cg_instr *instr;
			int reg;

			if (token_peek() == CG_TOK_reg)
			{
				reg = cg_yylval.ival;
				token_consume();
				token_expect(CG_TOK_assign, 1);
			}
			else
			{
				reg = -1;
			}

			op = (cg_instr_op)token_peek();
			token_consume();

			if (op == CG_INSTR_OP_phi)
			{
				int keep_going;

				instr = cg_instr_build_phi(b, vooid);
				instr_set_reg(instr, reg);

				do {
					delayed_phi_args[delayed_phi_args_idx].instr = instr;
					token_expect(CG_TOK_lsquare, 1);
					token_expect(CG_TOK_reg, 0);
					delayed_phi_args[delayed_phi_args_idx].arg_vreg = cg_yylval.ival;
					token_consume();
					token_expect(CG_TOK_comma, 1);
					token_expect(CG_TOK_bb_ref, 0);
					delayed_phi_args[delayed_phi_args_idx].arg_bb = cg_yylval.ival;
					token_consume();
					token_expect(CG_TOK_rsquare, 1);
					delayed_phi_args_idx++;
					if (token_peek() == CG_TOK_comma)
					{
						token_consume();
						keep_going = 1;
					}
					else
					{
						keep_going = 0;
					}
				} while (keep_going);
			}
			else if (op == CG_INSTR_OP_branch)
			{
				delayed_succs[delayed_succs_idx].b = b;
				if (token_peek() == CG_TOK_lbrace)
				{
					token_consume();
					b->true_cond = token_peek() - CG_TOK_eq;
					token_consume();
					token_expect(CG_TOK_rbrace, 1);

					token_expect(CG_TOK_bb_ref, 0);
					delayed_succs[delayed_succs_idx].succ_ids[0] = cg_yylval.ival;
					token_consume();

					token_expect(CG_TOK_comma, 1);

					token_expect(CG_TOK_bb_ref, 0);
					delayed_succs[delayed_succs_idx].succ_ids[1] = cg_yylval.ival;
					token_consume();
					delayed_succs[delayed_succs_idx].n_succs = 2;
				}
				else
				{
					token_expect(CG_TOK_bb_ref, 0);
					delayed_succs[delayed_succs_idx].succ_ids[0] = cg_yylval.ival;
					token_consume();
					delayed_succs[delayed_succs_idx].n_succs = 1;
				}
				delayed_succs_idx++;
			}
			else if ((cg_token)op < CG_TOK_define)
			{
				int keep_going = 1;
				int aidx = 0;

				instr = cg_instr_build(b, op);
				cg_instr_link_last(instr);
				instr_set_reg(instr, reg);

				if (token_peek() == CG_TOK_lbrace)
				{
					token_consume();
					instr->cond = token_peek() - CG_TOK_eq;
					token_consume();
					token_expect(CG_TOK_rbrace, 1);
				}

				if (op == CG_INSTR_OP_undef)
				{
					keep_going = 0;
				}

				while (keep_going) {
					if (token_peek() == CG_TOK_reg)
					{
						int areg = cg_yylval.ival;
						if (areg < CG_REG_VREG0)
						{
							instr->args[aidx].kind = CG_INSTR_ARG_HREG;
							instr->args[aidx].u.hreg = areg;
						}
						else
						{
							delayed_args[delayed_args_idx].instr = instr;
							delayed_args[delayed_args_idx].arg_idx = aidx;
							delayed_args[delayed_args_idx].arg_vreg = areg;
							delayed_args_idx++;
						}
					}
					else if (token_peek() == CG_TOK_imm)
					{
						instr->args[aidx].kind = CG_INSTR_ARG_IMM;
						instr->args[aidx].u.imm = cg_yylval.ival;
					}
					else if (token_peek() == CG_TOK_sym)
					{
						instr->args[aidx].kind = CG_INSTR_ARG_SYM;
						instr->args[aidx].u.sym = strdup(cg_yylval.strval);
					}
					else if (token_peek() == CG_TOK_lsquare)
					{
						int areg;
						token_consume();
						token_expect(CG_TOK_reg, 0);	
						areg = cg_yylval.ival;
						if (areg < CG_REG_VREG0)
						{
							instr->args[aidx].kind = CG_INSTR_ARG_HREG;
							instr->args[aidx].u.hreg = areg;
						}
						else
						{
							delayed_args[delayed_args_idx].instr = instr;
							delayed_args[delayed_args_idx].arg_idx = aidx;
							delayed_args[delayed_args_idx].arg_vreg = areg;
							delayed_args_idx++;
						}
						token_consume();
						if (token_peek() == CG_TOK_comma)
						{
							token_consume();
							token_expect(CG_TOK_imm, 0);
							instr->args[aidx].offset = cg_yylval.ival;
							token_consume();
						}
						token_expect(CG_TOK_rsquare, 0);
					}
					else
					{
						parse_error();
					}
    
					token_consume();
					aidx++;
    
					if (token_peek() == CG_TOK_comma)
					{
						token_consume();
						keep_going = 1;
					}
					else
					{
						keep_going = 0;
					}
				}
			}
			else
			{
				parse_error();
			}

			if (reg >= CG_REG_VREG0)
			{
				assert(reg < 1024);
				vreg2instr[reg] = instr;
			}

		}
	}

	{
		int i;
		for (i = 0; i < delayed_args_idx; i++)
		{
			cg_instr *instr = vreg2instr[delayed_args[i].arg_vreg];
			assert(instr);
			cg_instr_arg_set_vreg(delayed_args[i].instr, delayed_args[i].arg_idx, instr);
		}
		for (i = 0; i < delayed_phi_args_idx; i++)
		{
			cg_instr *instr = vreg2instr[delayed_phi_args[i].arg_vreg];
			cg_bb *b = bid2bb[delayed_phi_args[i].arg_bb];
			assert(instr);
			cg_instr_add_phi_arg(delayed_phi_args[i].instr, b, instr);
		}
		for (i = 0; i < delayed_succs_idx; i++)
		{
			cg_bb *b = delayed_succs[i].b;
			if (delayed_succs[i].n_succs == 2)
			{
				b->true_target = bid2bb[delayed_succs[i].succ_ids[0]];
				b->false_target = bid2bb[delayed_succs[i].succ_ids[1]];
				cg_bb_link_cfg(b, b->true_target);
				cg_bb_link_cfg(b, b->false_target);
			}
			else
			{
				assert(delayed_succs[i].n_succs == 1);
				cg_bb_link_cfg(b, bid2bb[delayed_succs[i].succ_ids[0]]);
			}
		}
	}
}

cg_tu *
cg_import(const char *path)
{
	cg_tu *tu = calloc(1, sizeof(cg_tu));

	cg_yyin = fopen(path, "r");
	current_token = cg_yylex();
	while (token_peek() != CG_TOK_end_of_file)
	{
		cg_token tok = token_peek();
		if (tok == CG_TOK_define)
		{
			handle_define(tu);
		}
		else if (tok == CG_TOK_sym)
		{
			char tmp_str[128];
			unsigned size, align;
			strncpy(tmp_str, cg_yylval.strval, sizeof(tmp_str));
			token_consume();
			token_expect(CG_TOK_assign, 1);

			token_expect(CG_TOK_size, 1);
			token_expect(CG_TOK_lparen, 1);
			token_expect(CG_TOK_integer, 0);
			size = cg_yylval.ival;
			token_consume();
			token_expect(CG_TOK_rparen, 1);
			token_expect(CG_TOK_comma, 1);
			
			token_expect(CG_TOK_align, 1);
			token_expect(CG_TOK_lparen, 1);
			token_expect(CG_TOK_integer, 0);
			align = cg_yylval.ival;
			token_consume();
			token_expect(CG_TOK_rparen, 1);

			if (token_peek() == CG_TOK_comma)
			{
				unsigned char tmp_init[1024];
				unsigned i;

				assert(size < sizeof(tmp_init));
				token_consume();
				token_expect(CG_TOK_init, 1);
				token_expect(CG_TOK_lparen, 1);
				for (i = 0; i < size; i++)
				{
					token_expect(CG_TOK_integer, 0);
					tmp_init[i] = cg_yylval.ival;
					token_consume();
					if (i < size - 1)
					{
						token_expect(CG_TOK_comma, 1);
					}
				}
				token_expect(CG_TOK_rparen, 1);

				(void)cg_data_build(tu, tmp_str, size, align, tmp_init);
			}
			else
			{
				(void)cg_data_build(tu, tmp_str, size, align, NULL);
			}
		}
		else
		{
			token_consume();
		}
	}
	fclose(cg_yyin);

	return tu;
}
