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

#include "frontend/symbol.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static symbol *symbol_first[32] = {0};
static int scope_idx = 0;

symbol * symbol_insert(const char *name)
{
	symbol *sym = calloc(1, sizeof(symbol));
	sym->name = strdup(name);
	sym->next = symbol_first[scope_idx];
	symbol_first[scope_idx] = sym;

	return sym;
}

symbol * symbol_lookup(const char *name)
{
	int i;
	for (i = scope_idx; i >= 0; i--)
	{
		symbol *curr;
		for (curr = symbol_first[i]; curr != NULL; curr = curr->next)
		{
			if (strcmp(curr->name, name) == 0)
			{
				return curr;
			}
		}
	}

	return NULL;
}

void symbol_scope_push(void)
{
	symbol_first[++scope_idx] = NULL;
	assert(scope_idx < 32);
}

void symbol_scope_pop(void)
{
	symbol_first[scope_idx--] = NULL;
	assert(scope_idx >= 0);
}

