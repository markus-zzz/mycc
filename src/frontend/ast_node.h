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

#ifndef AST_NODE_H
#define AST_NODE_H

#include <stdio.h>

typedef enum ast_op {
#define DEF_AST_OP(x) AST_OP_##x,
#include "ast_op.def"
#undef DEF_AST_OP
} ast_op;

typedef struct type_info {
	enum {
		T_INVALID,
		T_FLOAT,
		T_DOUBLE,
		T_SIGNED_CHAR,
		T_SIGNED_SHORT,
		T_SIGNED_INT,
		T_SIGNED_LONG,
		T_UNSIGNED_CHAR,
		T_UNSIGNED_SHORT,
		T_UNSIGNED_INT,
		T_UNSIGNED_LONG,
		T_POINTER,
		T_ARRAY,
		T_STRUCT
	} basic_type;
	struct ast_node *pointed_to_type;
} type_info;

typedef struct ast_node {
	ast_op op;
	struct ast_node *child;
	struct ast_node *sibling;
	struct ast_node *type;
	int is_pointer;
	struct type_info ti;
	union {
		int ivalue;
		char *strvalue;
	} u;
} ast_node;

ast_node *
ast_node_build0(ast_op op);

ast_node *
ast_node_build1(ast_op op, ast_node *n1);

ast_node *
ast_node_build2(ast_op op, ast_node *n1, ast_node *n2);

ast_node *
ast_node_build3(ast_op op, ast_node *n1, ast_node *n2, ast_node *n3);

ast_node *
ast_node_build4(ast_op op, ast_node *n1, ast_node *n2, ast_node *n3, ast_node *n4);

ast_node *
ast_node_append_sibling(ast_node *n, ast_node *n1);

ast_node *
ast_node_prepend_child(ast_node *n, ast_node *child);

void
ast_node_dump_tree(FILE *fp, ast_node *n);

#endif
