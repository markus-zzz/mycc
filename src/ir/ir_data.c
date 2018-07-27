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

#include "ir_data.h"
#include "ir_tu.h"

#include <stdlib.h>
#include <string.h>

ir_data *
ir_data_build(ir_tu *tu, const char *name, unsigned size, unsigned align, const unsigned char *init)
{
	ir_data *sym = calloc(1, sizeof(ir_data));
	sym->name = strdup(name);
	sym->size = size;
	sym->align = align;
	if (init != NULL)
	{
		sym->init = malloc(size);
		memcpy(sym->init, init, size);
	}
	sym->tu_next = tu->first_ir_data;
	tu->first_ir_data = sym;

	return sym;
}
