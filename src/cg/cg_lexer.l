%{
#include <string.h>
#include "cg/cg_tokens.h"
#include "cg/cg_reg.h"

static const char *str2tok[] = {
#define DEF_CG_INSTR(x) #x,
#include "cg/cg_instr.def"
#undef DEF_CG_INSTR
#define DEF_CG_COND(x) #x,
#include "cg/cg_cond.def"
#undef DEF_CG_COND
};

struct cg_yylval_type cg_yylval;
static void count(void);
%}

%option nounput noinput prefix="cg_yy"

%%

<<EOF>>    { count(); return CG_TOK_end_of_file; }
"define"   { count(); return CG_TOK_define; }
"size"     { count(); return CG_TOK_size; }
"align"    { count(); return CG_TOK_align; }
"init"     { count(); return CG_TOK_init; }
"{"        { count(); return CG_TOK_lbrace; }
"}"        { count(); return CG_TOK_rbrace; }
"("        { count(); return CG_TOK_lparen; }
")"        { count(); return CG_TOK_rparen; }
","        { count(); return CG_TOK_comma; }
"="        { count(); return CG_TOK_assign; }
"["        { count(); return CG_TOK_lsquare; }
"]"        { count(); return CG_TOK_rsquare; }

"%sp"              {count(); cg_yylval.ival = CG_REG_sp; return CG_TOK_reg; }
"%lr"              {count(); cg_yylval.ival = CG_REG_lr; return CG_TOK_reg; }
"%pc"              {count(); cg_yylval.ival = CG_REG_pc; return CG_TOK_reg; }
"%"[rv][0-9]+      {count(); cg_yylval.ival = strtol(&yytext[2], NULL, 10); return CG_TOK_reg; }
"#"[0-9]+          {count(); cg_yylval.ival = strtol(&yytext[1], NULL, 16); return CG_TOK_imm; }
"#0x"[0-9a-f]+     {count(); cg_yylval.ival = strtol(&yytext[3], NULL, 16); return CG_TOK_imm; }
[0-9]+             {count(); cg_yylval.ival = strtol(&yytext[0], NULL, 10); return CG_TOK_integer; }
"0x"[0-9a-f]+      {count(); cg_yylval.ival = strtol(&yytext[2], NULL, 16); return CG_TOK_integer; }
"bb"[0-9]+":"      {count(); cg_yylval.ival = strtol(&yytext[2], NULL, 10); return CG_TOK_bb_def; }
"%bb"[0-9]+        {count(); cg_yylval.ival = strtol(&yytext[3], NULL, 10); return CG_TOK_bb_ref; }
"@"[_a-zA-Z][_a-zA-Z0-9]* {count(); cg_yylval.strval = &yytext[1]; return CG_TOK_sym; }

[a-z]+	{
	int i;
	count();
	for (i = 0; i < sizeof(str2tok)/sizeof(str2tok[0]); i++)
	{
		if (!strcmp(cg_yytext, str2tok[i]))
		{
			return i;
		}
	}
}

[ \t\v\n\f] { count(); }
[;].*$      { /* ignore comments */ }

%%


int cg_yywrap(void)
{
	return(1);
}

int cg_column = 0;
int cg_line = 1;

static void count(void)
{
	int i;

	for (i = 0; cg_yytext[i] != '\0'; i++)
	{
		if (cg_yytext[i] == '\n')
		{
			cg_column = 0;
			cg_line++;
		}
		else if (cg_yytext[i] == '\t')
		{
			cg_column += 8 - (cg_column % 8);
		}
		else
		{
			cg_column++;
		}
	}
}

