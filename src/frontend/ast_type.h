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

#ifndef AST_TYPE_H
#define AST_TYPE_H

struct ast_node;

unsigned
ast_type_get_size(struct ast_node *n);


unsigned
ast_type_get_member_offset(struct ast_node *n,
                           const char *member_name,
                           struct ast_node **member_type,
                           int *member_is_pointer);
void
ast_type_handle_typedef(struct ast_node *n);


void
ast_type_insert(const char *name, struct ast_node *ts);

struct ast_node *
ast_type_lookup(const char *name);

#endif
