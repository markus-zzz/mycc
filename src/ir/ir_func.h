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

#ifndef IR_FUNC_H
#define IR_FUNC_H

#include "ir/ir.h"
#include "util/graph.h"

struct ir_func {
	const char *name;
	graph_ctx cfg_graph_ctx;
	graph_ctx ssa_graph_ctx;
	ir_func *tu_list_prev;
	ir_func *tu_list_next;
	ir_bb *entry;
	ir_bb *exit;
	ir_node *first_unused_ir_node;
	ir_node *last_unused_ir_node;
	unsigned n_ir_bbs;
	unsigned n_ir_nodes;
	unsigned n_params;
	ir_type *param_types;
	ir_type ret_type;
	int is_variadic;
	void *scratch;
};

ir_func *
ir_func_build(ir_tu *tu,
              const char *name,
              ir_type ret_type,
              unsigned n_params,
              ir_type *param_types);

ir_func *
ir_func_variadic_build(ir_tu *tu,
                       const char *name,
                       ir_type ret_type,
                       unsigned n_params,
                       ir_type *param_types);

int
ir_func_is_declaration(ir_func *func);

int
ir_func_is_definition(ir_func *func);

void ir_func_free_unused_nodes(ir_func *func);

#endif
