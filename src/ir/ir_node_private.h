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

#include "ir/ir_node.h"

/* TODO:FIXME: Add POISON define or something to make sure that this file is
 * never included outside the ir/ directory (i.e. it is not to be seen by the
 * front end, ir passes nor codegen) */

struct ir_node {
	graph_node graph;

	struct ir_node *bb_list_prev;
	struct ir_node *bb_list_next;
	struct ir_node *unused_list_prev;
	struct ir_node *unused_list_next;
	unsigned bb_list_depth;

	enum {IR_NODE_USED, IR_NODE_UNUSED} status;

	unsigned id;
	struct ir_bb *bb;
	ir_op op;
	ir_type type;

	union {
		struct {
			unsigned size;
			unsigned align;
		} alloca;
		struct {
			union {
				unsigned char i1;
				unsigned char i8;
				unsigned short i16;
				unsigned int i32;
				unsigned long long i64;
			} u;
		} constant;
		struct {
			ir_data *sym;
		} addr_of;
		struct {
			unsigned idx;
		} getparam;
		struct {
			ir_func *target;
		} call;
	} u;

	void *scratch;
};

