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

#ifndef GRAPH_H
#define GRAPH_H

#define GRAPH_NBR_MARKERS 2
#define GRAPH_MARKER_MAX (1 << 15)

typedef struct graph_ctx {
	unsigned marker_reserv[GRAPH_NBR_MARKERS];
	unsigned prev_marker;
	unsigned version; /* bumped each time the graph is modifed */
} graph_ctx;

typedef struct graph_node {
	struct graph_edge *preds;
	struct graph_edge *succs;
	unsigned short markers[GRAPH_NBR_MARKERS];
} graph_node;

typedef struct graph_edge {
	struct graph_node *tail;
	struct graph_node *head;
	struct graph_edge *pred_prev;
	struct graph_edge *pred_next;
	struct graph_edge *succ_prev;
	struct graph_edge *succ_next;
} graph_edge;

typedef struct graph_marker {
	unsigned char idx;
	unsigned short pattern;
} graph_marker;

graph_node *
graph_create_node(unsigned size);

graph_edge *
graph_edge_create(graph_ctx *ctx, graph_node *tail, graph_node *head, unsigned size, int (*cmp)(void *, void *));

void
graph_edge_delete(graph_ctx *ctx, graph_edge *edge);

void
graph_node_delete(graph_ctx *ctx, graph_node *node);

void
graph_preds_delete(graph_ctx *ctx, graph_node *node);

void
graph_succs_delete(graph_ctx *ctx, graph_node *node);

graph_edge *
graph_succ_first(graph_node *n);

graph_edge *
graph_succ_next(graph_edge *e);

graph_edge *
graph_pred_first(graph_node *n);

graph_edge *
graph_pred_next(graph_edge *e);

graph_node *
graph_edge_head(graph_edge *e);

graph_node *
graph_edge_tail(graph_edge *e);

void
graph_marker_alloc(graph_ctx *ctx, graph_marker *marker);

void
graph_marker_free(graph_ctx *ctx, graph_marker *marker);

int
graph_marker_set(graph_node *n, graph_marker *marker);

int
graph_marker_is_set(graph_node *n, graph_marker *marker);

#endif
