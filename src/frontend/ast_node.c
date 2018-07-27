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

#include "ast_node.h"

#include <stdio.h>
#include <stdlib.h>

ast_node *root;

const char *ast_op_to_str[] = {
#define DEF_AST_OP(x) #x,
#include "ast_op.def"
#undef DEF_AST_OP
};

static void
dump_tree_worker(FILE *fp, ast_node *n, char *indent_buf, int indent_pos)
{
	ast_node *child;
	fprintf(fp, "%s ", ast_op_to_str[n->op]);
	switch (n->op)
	{
	case AST_OP_CONSTANT:
		fprintf(fp, "(0x%x)", n->u.ivalue);
		break;
	case AST_OP_IDENTIFIER:
	case AST_OP_TYPE_NAME:
		fprintf(fp, "(%s)", n->u.strvalue);
		break;
	default:
		break;
	}
	fprintf(fp, "[%p]\n", n);

	for (child = n->child; child != NULL; child = child->sibling)
	{
		int indent_pos_ = indent_pos;
		int i;
		for (i = 0; i < indent_pos; i++)
		{
			fprintf(fp, "%c", indent_buf[i]);
		}
		fprintf(fp, "%c", child->sibling == NULL ? '\\' : '+');
		fprintf(fp, "-");
		if (child->sibling == NULL)
		{
			indent_buf[indent_pos_++] = ' ';
		}
		else
		{
			indent_buf[indent_pos_++] = '|';
		}
		indent_buf[indent_pos_++] = ' ';
		dump_tree_worker(fp, child, indent_buf, indent_pos_);
    }
}

void
ast_node_dump_tree(FILE *fp, ast_node *n)
{
	char buf[128];
	dump_tree_worker(fp, root, buf, 0);
}

ast_node *
ast_node_build0(ast_op op)
{
	ast_node *n;
	n = calloc(1, sizeof(ast_node));
	n->op = op;
	return n;
}

ast_node *
ast_node_build1(ast_op op, ast_node *n1)
{
	ast_node *n;
	n = calloc(1, sizeof(ast_node));
	n->op = op;
	n->child = n1;
	return n;
}

ast_node *
ast_node_build2(ast_op op, ast_node *n1, ast_node *n2)
{
	ast_node *n;
	n = calloc(1, sizeof(ast_node));
	n->op = op;
	n->child = n1;
	n->child->sibling = n2;
	return n;
}

ast_node *
ast_node_build3(ast_op op, ast_node *n1, ast_node *n2, ast_node *n3)
{
	ast_node *n;
	n = calloc(1, sizeof(ast_node));
	n->op = op;
	n->child = n1;
	n->child->sibling = n2;
	n->child->sibling->sibling = n3;

	return n;
}

ast_node *
ast_node_build4(ast_op op, ast_node *n1, ast_node *n2, ast_node *n3, ast_node *n4)
{
	ast_node *n;
	n = calloc(1, sizeof(ast_node));
	n->op = op;
	n->child = n1;
	n->child->sibling = n2;
	n->child->sibling->sibling = n3;
	n->child->sibling->sibling->sibling = n4;

	return n;
}

ast_node *
ast_node_append_sibling(ast_node *n, ast_node *n1)
{
	ast_node **pp = &n->sibling;
	while (*pp != NULL)
	{
		pp = &(*pp)->sibling;
	}

	*pp = n1;

	return n;
}

ast_node *
ast_node_prepend_child(ast_node *n, ast_node *child)
{
	child->sibling = n->child;
	n->child = child;
	return n;
}
