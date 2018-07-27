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

#include "cg_data.h"
#include "cg_tu.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

cg_data *
cg_data_build(cg_tu *tu, const char *name, unsigned size, unsigned align, const unsigned char *init)
{
	cg_data *sym = calloc(1, sizeof(cg_data));
	sym->name = strdup(name);
	sym->size = size;
	sym->align = align;
	if (init != NULL)
	{
		sym->init = malloc(size);
		memcpy(sym->init, init, size);
	}
	if (tu->data_first == NULL)
	{
		assert(tu->data_last == NULL);
		tu->data_first = sym;
	}
	else
	{
		assert(tu->data_last != NULL);
		tu->data_last->data_next = sym;
	}
	tu->data_last = sym;

	return sym;
}
