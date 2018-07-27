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

#ifndef GRAPH_LOOP_H
#define GRAPH_LOOP_H

#include "util/graph.h"

typedef struct graph_loop_bb_info {
	unsigned pre;
	unsigned rpost;
	graph_node *header;
	enum {GRAPH_LOOP_NONHEADER, GRAPH_LOOP_HEADER, GRAPH_LOOP_SELF} type;
} graph_loop_bb_info;

typedef graph_loop_bb_info *(*graph_loop_get_info)(graph_node *);

void
graph_loop_analyze(graph_ctx *gctx, graph_node *start, unsigned n_nodes, graph_loop_get_info get_info);

#endif
