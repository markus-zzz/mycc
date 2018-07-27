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

#include "ir_func.h"
#include "ir_tu.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

ir_func *
ir_func_build(ir_tu *tu,
              const char *name,
              ir_type ret_type,
              unsigned n_params,
              ir_type *param_types)
{
	unsigned i;
	ir_func *func = calloc(1, sizeof(ir_func));
	func->name = strdup(name);
	func->ret_type = ret_type;
	func->param_types = calloc(n_params, sizeof(ir_type));
	func->n_params = n_params;
	for (i = 0; i < n_params; i++)
	{
		func->param_types[i] = param_types[i];
	}

	if (tu->first_ir_func == NULL)
	{
		assert(tu->last_ir_func == NULL);
		tu->first_ir_func = func;
		tu->last_ir_func = func;
	}
	else
	{
		assert(tu->last_ir_func != NULL);
		func->tu_list_prev = tu->last_ir_func;
		tu->last_ir_func->tu_list_next = func;
		tu->last_ir_func = func;
	}

	return func;
}

ir_func *
ir_func_variadic_build(ir_tu *tu,
                       const char *name,
                       ir_type ret_type,
                       unsigned n_params,
                       ir_type *param_types)
{
	ir_func *f;
	f = ir_func_build(tu, name, ret_type, n_params, param_types);
	f->is_variadic = 1;
	return f;
}

int
ir_func_is_declaration(ir_func *func)
{
	return func->entry == NULL;
}

int
ir_func_is_definition(ir_func *func)
{
	return func->entry != NULL;
}
