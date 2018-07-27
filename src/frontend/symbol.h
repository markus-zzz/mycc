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

#ifndef SYMBOL_H
#define SYMBOL_H

typedef struct symbol {
	struct symbol *next;

	const char *name;
	struct ir_node *alloca;
	struct ast_node *type_spec;
	int is_pointer;
	int is_array;
} symbol;

symbol * symbol_insert(const char *name);
symbol * symbol_lookup(const char *name);
void symbol_scope_push(void);
void symbol_scope_pop(void);

#endif
