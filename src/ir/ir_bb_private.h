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

#pragma once

#include "util/graph.h"
#include "ir/ir_bb.h"

struct ir_bb {
	graph_node graph;
	unsigned id;
	ir_func *func;
	ir_node *first_ir_node;
	ir_node *last_ir_node;
	ir_node *first_ir_phi_node;
	ir_node *last_ir_phi_node;
	unsigned n_ir_nodes;
	enum {IR_BB_TERM_BR, IR_BB_TERM_RET} term_kind;
	ir_node *term_node;

	struct ir_dom_info *dom_info;
	void *scratch;
};

