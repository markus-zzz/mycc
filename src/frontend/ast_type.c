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

#include "ast_type.h"
#include "ast_node.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

struct type {
	struct type *next;

	const char *name;
	struct ast_node *type_spec;
};

static struct type *type_first = NULL;

void
ast_type_insert(const char *name, ast_node *ts)
{
	struct type *tmp = calloc(1, sizeof(struct type));
	assert(ts->op == AST_OP_TYPE_SPECIFIER);
	tmp->name = strdup(name);
	tmp->type_spec = ts;
	tmp->next = type_first;
	type_first = tmp;
}

ast_node *
ast_type_lookup(const char *name)
{
	struct type *curr;
	for (curr = type_first; curr != NULL; curr = curr->next)
	{
		if (strcmp(curr->name, name) == 0)
		{
			return curr->type_spec;
		}
	}

	return NULL;
}

unsigned
ast_type_get_size(ast_node *n)
{
	unsigned size = 0;
	assert(n->op == AST_OP_TYPE_SPECIFIER);

	if (n->child->op == AST_OP_STRUCT_OR_UNION_SPECIFIER &&
	    n->child->child->op == AST_OP_STRUCT)
	{
		ast_node *lst = n->child->child->sibling;
		ast_node *sd;

		if (lst->op == AST_OP_IDENTIFIER)
		{
			if (lst->sibling == NULL)
			{
				n = ast_type_lookup(lst->u.strvalue);
				lst = n->child->child->sibling;
			}
			/* skip tag name */
			lst = lst->sibling;
		}
		assert(lst->op == AST_OP_STRUCT_DECLARATION_LIST);
		for (sd = lst->child; sd != NULL; sd = sd->sibling)
		{
			ast_node *sd2;
			assert(sd->child->op == AST_OP_SPECIFIER_QUALIFIER_LIST);
			assert(sd->child->sibling->op == AST_OP_STRUCT_DECLARATOR_LIST);
			for (sd2 = sd->child->sibling->child; sd2 != NULL; sd2 = sd2->sibling)
			{
				ast_node *dd;
				unsigned type_size;

				assert(sd2->op == AST_OP_STRUCT_DECLARATOR);
				assert(sd2->child->op == AST_OP_DECLARATOR);
				if (sd2->child->child->op == AST_OP_POINTER)
				{
					dd = sd2->child->child->sibling;
					type_size = 4;
				}
				else
				{
					dd = sd2->child->child;
					type_size = ast_type_get_size(sd->child->child);
				}
				assert(dd->op == AST_OP_DIRECT_DECLARATOR);
				if (dd->child->op == AST_OP_ARRAY)
				{
					ast_node *array = dd->child;
					unsigned array_size;
					if (array->child->sibling)
					{
						array_size = array->child->sibling->u.ivalue;
					}
					else
					{
						array_size = 0;
					}
					size += type_size*array_size;
				}
				else
				{
					size += type_size;
				}
			}
		}
	}
	else if (n->child->op == AST_OP_TYPE_NAME)
	{
		size = ast_type_get_size(ast_type_lookup(n->child->u.strvalue));
	}
	else
	{
		switch (n->child->op)
		{
		case AST_OP_INT:
			size = 4;
			break;
		case AST_OP_SHORT:
			size = 2;
			break;
		case AST_OP_CHAR:
			size = 1;
			break;
		default:
			assert(0);
			break;
		}
	}

	return size;
}

unsigned
ast_type_get_member_offset(ast_node *n,
                           const char *member_name,
                           ast_node **member_type,
                           int *member_is_pointer)
{
	unsigned size = 0;
	assert(n->op == AST_OP_TYPE_SPECIFIER);
	assert(n->child->op == AST_OP_STRUCT_OR_UNION_SPECIFIER);
	assert(n->child->child->op == AST_OP_STRUCT);

	ast_node *lst = n->child->child->sibling;
	ast_node *sd;

	if (lst->op == AST_OP_IDENTIFIER)
	{
		if (lst->sibling == NULL)
		{
			n = ast_type_lookup(lst->u.strvalue);
			lst = n->child->child->sibling;
		}
		lst = lst->sibling;
	}
	assert(lst->op == AST_OP_STRUCT_DECLARATION_LIST);
	for (sd = lst->child; sd != NULL; sd = sd->sibling)
	{
		ast_node *sd2;
		unsigned type_size;
		assert(sd->child->op == AST_OP_SPECIFIER_QUALIFIER_LIST);
		type_size = ast_type_get_size(sd->child->child);
		assert(sd->child->sibling->op == AST_OP_STRUCT_DECLARATOR_LIST);
		for (sd2 = sd->child->sibling->child; sd2 != NULL; sd2 = sd2->sibling)
		{
			ast_node *dd;
			ast_node *id;
			unsigned array_size;
			int is_pointer = 0;
			assert(sd2->op == AST_OP_STRUCT_DECLARATOR);
			assert(sd2->child->op == AST_OP_DECLARATOR);
			if (sd2->child->child->op == AST_OP_POINTER)
			{
				dd = sd2->child->child->sibling;
				is_pointer = 1;
			}
			else
			{
				dd = sd2->child->child;
			}
			assert(dd->op == AST_OP_DIRECT_DECLARATOR);
			if (dd->child->op == AST_OP_ARRAY)
			{
				ast_node *array = dd->child;
				assert(array->child->op == AST_OP_DIRECT_DECLARATOR);
				id = array->child->child;
				if (array->child->sibling)
				{
					assert(array->child->sibling->op == AST_OP_CONSTANT);
					array_size = array->child->sibling->u.ivalue;
				}
				else
				{
					array_size = 0;
				}
			}
			else
			{
				id = dd->child;
				array_size = 1;
			}
			assert(id->op == AST_OP_IDENTIFIER);
			if (!strcmp(member_name, id->u.strvalue))
			{
				*member_type = sd->child->child;
				*member_is_pointer = is_pointer;
				return size;
			}
			size += (is_pointer ? 4 : type_size)*array_size;
		}
	}
	assert(0 && "Member not found in struct");
	return 0;
}

void
ast_type_handle_typedef(ast_node *n)
{
/*
  DECLARATION [0x41318]
  +-DECLARATION_SPECIFIERS [0x41258]
  | +-STORAGE_CLASS_SPECIFIER [0x411e8]
  | | \-TYPEDEF [0x411c8]
  | \-TYPE_SPECIFIER [0x41228]
  |   \-INT [0x41208]
  \-INIT_DECLARATOR_LIST [0x412f8]
    \-INIT_DECLARATOR [0x412d8]
      \-DECLARATOR [0x412b8]
        \-DIRECT_DECLARATOR [0x41298]
          \-IDENTIFIER (foot)[0x41278]
*/
	ast_node *a, *b, *ts, *id;
	assert(n->op == AST_OP_DECLARATION);
	a = n->child;
	b = a->sibling;
	assert(a->op == AST_OP_DECLARATION_SPECIFIERS);
	assert(b->op == AST_OP_INIT_DECLARATOR_LIST);

	ts = NULL;
	id = NULL;

	if (a->child->op == AST_OP_STORAGE_CLASS_SPECIFIER &&
	    a->child->child->op == AST_OP_TYPEDEF)
	{
		ts = a->child->sibling;
	}

	if (b->child->op == AST_OP_INIT_DECLARATOR &&
	    b->child->child->op == AST_OP_DECLARATOR &&
	    b->child->child->child->op == AST_OP_DIRECT_DECLARATOR)
	{
		id = b->child->child->child->child;
	}

	if (ts && id)
	{
		ast_type_insert(id->u.strvalue, ts);
	}
}

