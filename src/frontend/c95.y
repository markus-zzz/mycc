%{
#include "frontend/ast_node.h"
#include "frontend/ast_type.h"
#include <assert.h>
#include <stdio.h>

extern ast_node *root;
int yyerror(const char *);
int yylex(void);
%}

%union {ast_op op; ast_node *node; unsigned long ivalue; char *strvalue;}

%token IDENTIFIER CONSTANT STRING_LITERAL SIZEOF
%token PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN
%token SUB_ASSIGN LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN
%token XOR_ASSIGN OR_ASSIGN TYPE_NAME

%token TYPEDEF EXTERN STATIC AUTO REGISTER
%token CHAR SHORT INT LONG SIGNED UNSIGNED FLOAT DOUBLE CONST VOLATILE VOID
%token STRUCT UNION ENUM ELLIPSIS

%token CASE DEFAULT IF ELSE SWITCH WHILE DO FOR GOTO CONTINUE BREAK RETURN

%type <node>
abstract_declarator
additive_expression
and_expression
argument_expression_list
assignment_expression
cast_expression
compound_statement
conditional_expression
constant_expression
declaration
declaration_list
declaration_specifiers
declarator
direct_abstract_declarator
direct_declarator
enum_specifier
enumerator
enumerator_list
equality_expression
exclusive_or_expression
expression
expression_statement
external_declaration
function_definition
identifier_list
inclusive_or_expression
init_declarator
init_declarator_list
initializer
iteration_statement
jump_statement
logical_and_expression
logical_or_expression
multiplicative_expression
parameter_declaration
parameter_list
parameter_type_list
pointer
postfix_expression
primary_expression
relational_expression
root
selection_statement
shift_expression
specifier_qualifier_list
statement
statement_list
storage_class_specifier
struct_declaration
struct_declaration_list
struct_declarator
struct_declarator_list
struct_or_union
struct_or_union_specifier
translation_unit
type_name
type_qualifier
type_qualifier_list
type_specifier
unary_expression

%type <op>
assignment_operator
unary_operator

%type <ivalue> CONSTANT
%type <strvalue> IDENTIFIER TYPE_NAME STRING_LITERAL

%start root
%%

primary_expression
	: IDENTIFIER {
		$$ = ast_node_build0(AST_OP_IDENTIFIER); $$->u.strvalue = $1;
	}
	| CONSTANT {
		$$ = ast_node_build0(AST_OP_CONSTANT); $$->u.ivalue = $1;
	}
	| STRING_LITERAL {
		$$ = ast_node_build0(AST_OP_STRING_LITERAL); $$->u.strvalue = $1;
	}
	| '(' expression ')' {
		$$ = $2;
	}
	;

postfix_expression
	: primary_expression
	| postfix_expression '[' expression ']' {
		$$ = ast_node_build2(AST_OP_INDEX, $1, $3);
	}
	| postfix_expression '(' ')' {
		$$ = ast_node_build1(AST_OP_CALL, $1);
	}
	| postfix_expression '(' argument_expression_list ')' {
		$$ = ast_node_build2(AST_OP_CALL, $1, $3);
	}
	| postfix_expression '.' IDENTIFIER {
		ast_node *id = ast_node_build0(AST_OP_IDENTIFIER);
		id->u.strvalue = $3;
		$$ = ast_node_build2(AST_OP_MEMBER, $1, id);
	}
	| postfix_expression PTR_OP IDENTIFIER {
		ast_node *id = ast_node_build0(AST_OP_IDENTIFIER);
		id->u.strvalue = $3;
		$$ = ast_node_build2(AST_OP_MEMBER, $1, id);
	}
	| postfix_expression INC_OP {
		$$ = ast_node_build1(AST_OP_POST_INC, $1);
	}
	| postfix_expression DEC_OP {
		$$ = ast_node_build1(AST_OP_POST_DEC, $1);
	}
	;

argument_expression_list
	: assignment_expression
	| argument_expression_list ',' assignment_expression {
		$$ = ast_node_append_sibling($1, $3);
	}
	;

unary_expression
	: postfix_expression
	| INC_OP unary_expression {
		$$ = ast_node_build1(AST_OP_PRE_INC, $2);
	}
	| DEC_OP unary_expression {
		$$ = ast_node_build1(AST_OP_PRE_DEC, $2);
	}
	| unary_operator cast_expression {
		$$ = ast_node_build1($1, $2);
	}
	| SIZEOF unary_expression
	| SIZEOF '(' type_name ')'
	;

unary_operator
	: '&' {$$ = AST_OP_ADDR_OF;}
	| '*' {$$ = AST_OP_VALUE_OF;}
	| '+'
	| '-'
	| '~'
	| '!'
	;

cast_expression
	: unary_expression
	| '(' type_name ')' cast_expression {
		$$ = ast_node_build2(AST_OP_CAST, $2, $4);
	}
	;

multiplicative_expression
	: cast_expression
	| multiplicative_expression '*' cast_expression {$$ = ast_node_build2(AST_OP_MUL, $1, $3);}
	| multiplicative_expression '/' cast_expression {$$ = ast_node_build2(AST_OP_DIV, $1, $3);}
	| multiplicative_expression '%' cast_expression {$$ = ast_node_build2(AST_OP_REM, $1, $3);}
	;

additive_expression
	: multiplicative_expression
	| additive_expression '+' multiplicative_expression {$$ = ast_node_build2(AST_OP_ADD, $1, $3);}
	| additive_expression '-' multiplicative_expression {$$ = ast_node_build2(AST_OP_SUB, $1, $3);}
	;

shift_expression
	: additive_expression
	| shift_expression LEFT_OP additive_expression {$$ = ast_node_build2(AST_OP_SHIFT_LEFT, $1, $3);}
	| shift_expression RIGHT_OP additive_expression {$$ = ast_node_build2(AST_OP_SHIFT_RIGHT, $1, $3);}
	;

relational_expression
	: shift_expression
	| relational_expression '<' shift_expression {$$ = ast_node_build2(AST_OP_LT, $1, $3);}
	| relational_expression '>' shift_expression {$$ = ast_node_build2(AST_OP_GT, $1, $3);}
	| relational_expression LE_OP shift_expression {$$ = ast_node_build2(AST_OP_LE, $1, $3);}
	| relational_expression GE_OP shift_expression {$$ = ast_node_build2(AST_OP_GE, $1, $3);}
	;

equality_expression
	: relational_expression
	| equality_expression EQ_OP relational_expression {$$ = ast_node_build2(AST_OP_EQ, $1, $3);}
	| equality_expression NE_OP relational_expression {$$ = ast_node_build2(AST_OP_NE, $1, $3);}
	;

and_expression
	: equality_expression
	| and_expression '&' equality_expression {$$ = ast_node_build2(AST_OP_AND, $1, $3);}
	;

exclusive_or_expression
	: and_expression
	| exclusive_or_expression '^' and_expression {$$ = ast_node_build2(AST_OP_XOR, $1, $3);}
	;

inclusive_or_expression
	: exclusive_or_expression
	| inclusive_or_expression '|' exclusive_or_expression {$$ = ast_node_build2(AST_OP_OR, $1, $3);}
	;

logical_and_expression
	: inclusive_or_expression
	| logical_and_expression AND_OP inclusive_or_expression {$$ = ast_node_build2(AST_OP_LOGICAL_AND, $1, $3);}
	;

logical_or_expression
	: logical_and_expression
	| logical_or_expression OR_OP logical_and_expression {$$ = ast_node_build2(AST_OP_LOGICAL_OR, $1, $3);}
	;

conditional_expression
	: logical_or_expression
	| logical_or_expression '?' expression ':' conditional_expression {
		$$ = ast_node_build3(AST_OP_CONDITIONAL, $1, $3, $5);
	}
	;

assignment_expression
	: conditional_expression
	| unary_expression assignment_operator assignment_expression {$$ = ast_node_build2($2, $1, $3);}
	;

assignment_operator
	: '=' {$$ = AST_OP_ASSIGN;}
	| MUL_ASSIGN {$$ = AST_OP_MUL_ASSIGN;}
	| DIV_ASSIGN {$$ = AST_OP_DIV_ASSIGN;}
	| MOD_ASSIGN {$$ = AST_OP_MOD_ASSIGN;}
	| ADD_ASSIGN {$$ = AST_OP_ADD_ASSIGN;}
	| SUB_ASSIGN {$$ = AST_OP_SUB_ASSIGN;}
	| LEFT_ASSIGN {$$ = AST_OP_LEFT_ASSIGN;}
	| RIGHT_ASSIGN {$$ = AST_OP_RIGHT_ASSIGN;}
	| AND_ASSIGN {$$ = AST_OP_AND_ASSIGN;}
	| XOR_ASSIGN {$$ = AST_OP_XOR_ASSIGN;}
	| OR_ASSIGN {$$ = AST_OP_OR_ASSIGN;}
	;

expression
	: assignment_expression
	| expression ',' assignment_expression
	;

constant_expression
	: conditional_expression
	;

declaration
	: declaration_specifiers ';' {
		$$ = ast_node_build1(AST_OP_DECLARATION, $1);
	}
	| declaration_specifiers init_declarator_list ';' {
		ast_node *lst = ast_node_build1(AST_OP_INIT_DECLARATOR_LIST, $2);
		$$ = ast_node_build2(AST_OP_DECLARATION, $1, lst);
		/* This is where we need to register the typedefed name if any */
		ast_type_handle_typedef($$);
	}
	;

declaration_specifiers
	: storage_class_specifier {
		$$ = ast_node_build1(AST_OP_DECLARATION_SPECIFIERS, $1);
	}
	| storage_class_specifier declaration_specifiers {
		$$ = ast_node_prepend_child($2, $1);
	}
	| type_specifier {
		$$ = ast_node_build1(AST_OP_DECLARATION_SPECIFIERS, $1);
	}
	| type_specifier declaration_specifiers {
		$$ = ast_node_prepend_child($2, $1);
	}
	| type_qualifier {
		$$ = ast_node_build1(AST_OP_DECLARATION_SPECIFIERS, $1);
	}
	| type_qualifier declaration_specifiers {
		$$ = ast_node_prepend_child($2, $1);
	}
	;

init_declarator_list
	: init_declarator
	| init_declarator_list ',' init_declarator {
		$$ = ast_node_append_sibling($1, $3);
	}
	;

init_declarator
	: declarator {
		$$ = ast_node_build1(AST_OP_INIT_DECLARATOR, $1);
	}
	| declarator '=' initializer {
		$$ = ast_node_build2(AST_OP_INIT_DECLARATOR, $1, $3);
	}
	;

storage_class_specifier
	: TYPEDEF {
		ast_node *sclass = ast_node_build0(AST_OP_TYPEDEF);
		$$ = ast_node_build1(AST_OP_STORAGE_CLASS_SPECIFIER, sclass);
	}
	| EXTERN {
		ast_node *sclass = ast_node_build0(AST_OP_EXTERN);
		$$ = ast_node_build1(AST_OP_STORAGE_CLASS_SPECIFIER, sclass);
	}
	| STATIC {
		ast_node *sclass = ast_node_build0(AST_OP_STATIC);
		$$ = ast_node_build1(AST_OP_STORAGE_CLASS_SPECIFIER, sclass);
	}
	| AUTO {
		ast_node *sclass = ast_node_build0(AST_OP_AUTO);
		$$ = ast_node_build1(AST_OP_STORAGE_CLASS_SPECIFIER, sclass);
	}
	| REGISTER {
		ast_node *sclass = ast_node_build0(AST_OP_REGISTER);
		$$ = ast_node_build1(AST_OP_STORAGE_CLASS_SPECIFIER, sclass);
	}
	;

type_specifier
	: VOID {
		ast_node *type = ast_node_build0(AST_OP_VOID);
		$$ = ast_node_build1(AST_OP_TYPE_SPECIFIER, type);
	}
	| CHAR {
		ast_node *type = ast_node_build0(AST_OP_CHAR);
		$$ = ast_node_build1(AST_OP_TYPE_SPECIFIER, type);
	}
	| SHORT {
		ast_node *type = ast_node_build0(AST_OP_SHORT);
		$$ = ast_node_build1(AST_OP_TYPE_SPECIFIER, type);
	}
	| INT {
		ast_node *type = ast_node_build0(AST_OP_INT);
		$$ = ast_node_build1(AST_OP_TYPE_SPECIFIER, type);
	}
	| LONG {
		ast_node *type = ast_node_build0(AST_OP_LONG);
		$$ = ast_node_build1(AST_OP_TYPE_SPECIFIER, type);
	}
	| FLOAT {
		ast_node *type = ast_node_build0(AST_OP_FLOAT);
		$$ = ast_node_build1(AST_OP_TYPE_SPECIFIER, type);
	}
	| DOUBLE {
		ast_node *type = ast_node_build0(AST_OP_DOUBLE);
		$$ = ast_node_build1(AST_OP_TYPE_SPECIFIER, type);
	}
	| SIGNED {
		ast_node *type = ast_node_build0(AST_OP_SIGNED);
		$$ = ast_node_build1(AST_OP_TYPE_SPECIFIER, type);
	}
	| UNSIGNED {
		ast_node *type = ast_node_build0(AST_OP_UNSIGNED);
		$$ = ast_node_build1(AST_OP_TYPE_SPECIFIER, type);
	}
	| struct_or_union_specifier {
		$$ = ast_node_build1(AST_OP_TYPE_SPECIFIER, $1);
	}
	| enum_specifier {
		$$ = ast_node_build1(AST_OP_TYPE_SPECIFIER, $1);
	}
	| TYPE_NAME {
		ast_node *type = ast_node_build0(AST_OP_TYPE_NAME);
		type->u.strvalue = $1;
		$$ = ast_node_build1(AST_OP_TYPE_SPECIFIER, type);
	}
	;

struct_or_union_specifier
	: struct_or_union IDENTIFIER '{' struct_declaration_list '}' {
		ast_node *id = ast_node_build0(AST_OP_IDENTIFIER);
		id->u.strvalue = $2;
		ast_node *lst = ast_node_build1(AST_OP_STRUCT_DECLARATION_LIST, $4);
		$$ = ast_node_build3(AST_OP_STRUCT_OR_UNION_SPECIFIER, $1, id, lst);
	}
	| struct_or_union '{' struct_declaration_list '}' {
		ast_node *lst = ast_node_build1(AST_OP_STRUCT_DECLARATION_LIST, $3);
		$$ = ast_node_build2(AST_OP_STRUCT_OR_UNION_SPECIFIER, $1, lst);
	}
	| struct_or_union IDENTIFIER {
		ast_node *id = ast_node_build0(AST_OP_IDENTIFIER);
		id->u.strvalue = $2;
		$$ = ast_node_build2(AST_OP_STRUCT_OR_UNION_SPECIFIER, $1, id);
	}
	;

struct_or_union
	: STRUCT {
		$$ = ast_node_build0(AST_OP_STRUCT);
	}
	| UNION {
		$$ = ast_node_build0(AST_OP_UNION);
	}
	;

struct_declaration_list
	: struct_declaration
	| struct_declaration_list struct_declaration {
		$$ = ast_node_append_sibling($1, $2);
	}
	;

struct_declaration
	: specifier_qualifier_list struct_declarator_list ';' {
		ast_node *lst1 = ast_node_build1(AST_OP_SPECIFIER_QUALIFIER_LIST, $1);
		ast_node *lst2 = ast_node_build1(AST_OP_STRUCT_DECLARATOR_LIST, $2);
		$$ = ast_node_build2(AST_OP_STRUCT_DECLARATION, lst1, lst2);
	}
	;

specifier_qualifier_list
	: type_specifier specifier_qualifier_list {
		$$ = ast_node_append_sibling($1, $2);
	}
	| type_specifier
	| type_qualifier specifier_qualifier_list {
		$$ = ast_node_append_sibling($1, $2);
	}
	| type_qualifier
	;

struct_declarator_list
	: struct_declarator
	| struct_declarator_list ',' struct_declarator {
		$$ = ast_node_append_sibling($1, $3);
	}
	;

struct_declarator
	: declarator {
		$$ = ast_node_build1(AST_OP_STRUCT_DECLARATOR, $1);
	}
	| ':' constant_expression {
		$$ = ast_node_build1(AST_OP_STRUCT_DECLARATOR, $2);
	}
	| declarator ':' constant_expression {
		$$ = ast_node_build2(AST_OP_STRUCT_DECLARATOR, $1, $3);
	}
	;

enum_specifier
	: ENUM '{' enumerator_list '}' {
		ast_node *lst = ast_node_build1(AST_OP_ENUMERATOR_LIST, $3);
		$$ = ast_node_build1(AST_OP_ENUM_SPECIFIER, lst);
	}
	| ENUM IDENTIFIER '{' enumerator_list '}' {
		ast_node *id = ast_node_build0(AST_OP_IDENTIFIER);
		id->u.strvalue = $2;
		ast_node *lst = ast_node_build1(AST_OP_ENUMERATOR_LIST, $4);
		$$ = ast_node_build2(AST_OP_ENUM_SPECIFIER, id, lst);
	}
	| ENUM IDENTIFIER {
		ast_node *id = ast_node_build0(AST_OP_IDENTIFIER);
		id->u.strvalue = $2;
		$$ = ast_node_build1(AST_OP_ENUM_SPECIFIER, id);
	}
	;

enumerator_list
	: enumerator
	| enumerator_list ',' enumerator {
		$$ = ast_node_append_sibling($1, $3);
	}
	;

enumerator
	: IDENTIFIER {
		ast_node *id = ast_node_build0(AST_OP_IDENTIFIER);
		id->u.strvalue = $1;
		$$ = ast_node_build1(AST_OP_ENUMERATOR, id);
	}
	| IDENTIFIER '=' constant_expression {
		ast_node *id = ast_node_build0(AST_OP_IDENTIFIER);
		id->u.strvalue = $1;
		$$ = ast_node_build2(AST_OP_ENUMERATOR, id, $3);
	}
	;

type_qualifier
	: CONST {
		ast_node *op = ast_node_build0(AST_OP_CONST);
		$$ = ast_node_build1(AST_OP_TYPE_QUALIFIER, op);
	}
	| VOLATILE {
		ast_node *op = ast_node_build0(AST_OP_VOLATILE);
		$$ = ast_node_build1(AST_OP_TYPE_QUALIFIER, op);
	}
	;

declarator
	: pointer direct_declarator {
		$$ = ast_node_build2(AST_OP_DECLARATOR, $1, $2);
	}
	| direct_declarator {
		$$ = ast_node_build1(AST_OP_DECLARATOR, $1);
	}
	;

direct_declarator
	: IDENTIFIER {
		ast_node *id = ast_node_build0(AST_OP_IDENTIFIER);
		id->u.strvalue = $1;
		$$ = ast_node_build1(AST_OP_DIRECT_DECLARATOR, id);
	}
	| '(' declarator ')' {
		$$ = ast_node_build1(AST_OP_DIRECT_DECLARATOR, $2);
	}
	| direct_declarator '[' constant_expression ']' {
		ast_node *array = ast_node_build2(AST_OP_ARRAY, $1, $3);
		$$ = ast_node_build1(AST_OP_DIRECT_DECLARATOR, array);
	}
	| direct_declarator '[' ']' {
		ast_node *array = ast_node_build1(AST_OP_ARRAY, $1);
		$$ = ast_node_build1(AST_OP_DIRECT_DECLARATOR, array);
	}
	| direct_declarator '(' parameter_type_list ')' {
		ast_node *lst = ast_node_build1(AST_OP_PARAMETER_TYPE_LIST, $3);
		$$ = ast_node_build2(AST_OP_DIRECT_DECLARATOR, $1, lst);
	}
	| direct_declarator '(' identifier_list ')' {
		ast_node *lst = ast_node_build1(AST_OP_IDENTIFIER_LIST, $3);
		$$ = ast_node_build2(AST_OP_DIRECT_DECLARATOR, $1, lst);
	}
	| direct_declarator '(' ')' {
		ast_node *lst = ast_node_build0(AST_OP_PARAMETER_TYPE_LIST);
		$$ = ast_node_build2(AST_OP_DIRECT_DECLARATOR, $1, lst);
	}
	;

pointer
	: '*' {
		$$ = ast_node_build0(AST_OP_POINTER);
	}
	| '*' type_qualifier_list {
		ast_node *lst = ast_node_build1(AST_OP_TYPE_QUALIFIER_LIST, $2);
		$$ = ast_node_build1(AST_OP_POINTER, lst);
	}
	| '*' pointer {
		$$ = ast_node_build1(AST_OP_POINTER, $2);
	}
	| '*' type_qualifier_list pointer {
		ast_node *lst = ast_node_build1(AST_OP_TYPE_QUALIFIER_LIST, $2);
		$$ = ast_node_build2(AST_OP_POINTER, lst, $3);
	}
	;

type_qualifier_list
	: type_qualifier
	| type_qualifier_list type_qualifier {
		$$ = ast_node_append_sibling($1, $2);
	}
	;


parameter_type_list
	: parameter_list
	| parameter_list ',' ELLIPSIS {
		ast_node *n = ast_node_build0(AST_OP_ELLIPSIS);
		$$ = ast_node_append_sibling($1, n);
	}
	;

parameter_list
	: parameter_declaration
	| parameter_list ',' parameter_declaration {
		$$ = ast_node_append_sibling($1, $3);
	}
	;

parameter_declaration
	: declaration_specifiers declarator {
		$$ = ast_node_build2(AST_OP_PARAMETER_DECLARATION, $1, $2);
	}
	| declaration_specifiers abstract_declarator {
		$$ = ast_node_build2(AST_OP_PARAMETER_DECLARATION, $1, $2);
	}
	| declaration_specifiers {
		$$ = ast_node_build1(AST_OP_PARAMETER_DECLARATION, $1);
	}
	;

identifier_list
	: IDENTIFIER {
		ast_node *id = ast_node_build0(AST_OP_IDENTIFIER);
		id->u.strvalue = $1;
		$$ = id;
	}
	| identifier_list ',' IDENTIFIER {
		ast_node *id = ast_node_build0(AST_OP_IDENTIFIER);
		id->u.strvalue = $3;
		$$ = ast_node_append_sibling($1, id);
	}
	;

type_name
	: specifier_qualifier_list {
		$$ = ast_node_build1(AST_OP_SPECIFIER_QUALIFIER_LIST, $1);
	}
	| specifier_qualifier_list abstract_declarator {
		$$ = ast_node_build2(AST_OP_SPECIFIER_QUALIFIER_LIST, $1, $2);
	}
	;

abstract_declarator
	: pointer {
		$$ = ast_node_build1(AST_OP_ABSTRACT_DECLARATOR, $1);
	}
	| direct_abstract_declarator {
		$$ = ast_node_build1(AST_OP_ABSTRACT_DECLARATOR, $1);
	}
	| pointer direct_abstract_declarator {
		$$ = ast_node_build2(AST_OP_ABSTRACT_DECLARATOR, $1, $2);
	}
	;

direct_abstract_declarator
	: '(' abstract_declarator ')'
	| '[' ']'
	| '[' constant_expression ']'
	| direct_abstract_declarator '[' ']'
	| direct_abstract_declarator '[' constant_expression ']'
	| '(' ')'
	| '(' parameter_type_list ')'
	| direct_abstract_declarator '(' ')'
	| direct_abstract_declarator '(' parameter_type_list ')'
	;

initializer
	: assignment_expression
	| '{' initializer_list '}'
	| '{' initializer_list ',' '}'
	;

initializer_list
	: initializer
	| initializer_list ',' initializer
	;

statement
	: labeled_statement
	| compound_statement
	| expression_statement
	| selection_statement
	| iteration_statement
	| jump_statement
	;

labeled_statement
	: IDENTIFIER ':' statement
	| CASE constant_expression ':' statement
	| DEFAULT ':' statement
	;

compound_statement
	: '{' '}' {
		$$ = ast_node_build0(AST_OP_COMPOUND_STATEMENT);
	}
	| '{' statement_list '}' {
		ast_node *lst = ast_node_build1(AST_OP_STATEMENT_LIST, $2);
		$$ = ast_node_build1(AST_OP_COMPOUND_STATEMENT, lst);
	}
	| '{' declaration_list '}' {
		ast_node *lst = ast_node_build1(AST_OP_DECLARATION_LIST, $2);
		$$ = ast_node_build1(AST_OP_COMPOUND_STATEMENT, lst);
	}
	| '{' declaration_list statement_list '}' {
		ast_node *lst1 = ast_node_build1(AST_OP_DECLARATION_LIST, $2);
		ast_node *lst2 = ast_node_build1(AST_OP_STATEMENT_LIST, $3);
		$$ = ast_node_build2(AST_OP_COMPOUND_STATEMENT, lst1, lst2);
	}
	;

declaration_list
	: declaration
	| declaration_list declaration {
		$$ = ast_node_append_sibling($1, $2);
	}
	;

statement_list
	: statement
	| statement_list statement {
		$$ = ast_node_append_sibling($1, $2);
	}
	;

expression_statement
	: ';' {
		$$ = ast_node_build0(AST_OP_EXPRESSION_STATEMENT);
	}
	| expression ';' {
		$$ = ast_node_build1(AST_OP_EXPRESSION_STATEMENT, $1);
	}
	;

selection_statement
	: IF '(' expression ')' statement {$$ = ast_node_build2(AST_OP_STMT_IF, $3, $5);}
	| IF '(' expression ')' statement ELSE statement {$$ = ast_node_build3(AST_OP_STMT_IF_ELSE, $3, $5, $7);}
	| SWITCH '(' expression ')' statement
	;

iteration_statement
	: WHILE '(' expression ')' statement {$$ = ast_node_build2(AST_OP_STMT_WHILE, $3, $5);}
	| DO statement WHILE '(' expression ')' ';' {$$ = ast_node_build2(AST_OP_STMT_DO_WHILE, $2, $5);}
	| FOR '(' expression_statement expression_statement ')' statement {$$ = ast_node_build4(AST_OP_STMT_FOR, $3, $4, ast_node_build0(AST_OP_NIL), $6);}
	| FOR '(' expression_statement expression_statement expression ')' statement {$$ = ast_node_build4(AST_OP_STMT_FOR, $3, $4, $5, $7);}
	;

jump_statement
	: GOTO IDENTIFIER ';'
	| CONTINUE ';' {$$ = ast_node_build0(AST_OP_STMT_CONTINUE);}
	| BREAK ';' {$$ = ast_node_build0(AST_OP_STMT_BREAK);}
	| RETURN ';' {$$ = ast_node_build0(AST_OP_STMT_RETURN);}
	| RETURN expression ';' {$$ = ast_node_build1(AST_OP_STMT_RETURN, $2);}
	;

translation_unit
	: external_declaration
	| translation_unit external_declaration {$$ = ast_node_append_sibling($1, $2);}
	;

external_declaration
	: function_definition
	| declaration
	;

function_definition
	: declaration_specifiers declarator declaration_list compound_statement {
		$$ = ast_node_build4(AST_OP_FUNCTION_DEF, $1, $2, $3, $4);
	}
	| declaration_specifiers declarator compound_statement {
		$$ = ast_node_build3(AST_OP_FUNCTION_DEF, $1, $2, $3);
	}
	| declarator declaration_list compound_statement {
		$$ = ast_node_build3(AST_OP_FUNCTION_DEF, $1, $2, $3);
	}
	| declarator compound_statement {
		$$ = ast_node_build2(AST_OP_FUNCTION_DEF, $1, $2);
	}
	;

root
	: translation_unit {root = ast_node_build1(AST_OP_TU, $1);}
	;

%%
#include <stdio.h>

extern char yytext[];
extern int column;
extern int line;

int yyerror(const char *msg)
{
	printf("%s: line:%d,column:%d\n", msg, line, column);
	return 0;
}

