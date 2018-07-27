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

#ifndef CG_TOKENS_H
#define CG_TOKENS_H

typedef enum cg_token {
#define DEF_CG_INSTR(x) CG_TOK_##x,
#include "cg_instr.def"
#undef DEF_CG_INSTR
#define DEF_CG_COND(x) CG_TOK_##x,
#include "cg_cond.def"
#undef DEF_CG_COND
	CG_TOK_define,
	CG_TOK_lbrace,
	CG_TOK_rbrace,
	CG_TOK_comma,
	CG_TOK_assign,
	CG_TOK_lsquare,
	CG_TOK_rsquare,
	CG_TOK_lparen,
	CG_TOK_rparen,

	CG_TOK_size,
	CG_TOK_align,
	CG_TOK_init,

	CG_TOK_reg,
	CG_TOK_imm,
	CG_TOK_sym,
	CG_TOK_bb_def,
	CG_TOK_bb_ref,
	CG_TOK_integer,
	CG_TOK_end_of_file
} cg_token;

struct cg_yylval_type {
	int ival;
	char *strval;
};

#endif
