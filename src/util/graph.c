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

#include "graph.h"

#include <assert.h>
#include <stdlib.h>

graph_node *
graph_create_node(unsigned size)
{
	assert(size >= sizeof(graph_node));
	return calloc(1, size);
}

static void
succ_insert_before_ref(graph_edge *edge, graph_edge *ref)
{
	if (ref->succ_prev)
	{
		ref->succ_prev->succ_next = edge;
	}
	else
	{
		ref->tail->succs = edge;

	}
	edge->succ_prev = ref->succ_prev;
	edge->succ_next = ref;
	ref->succ_prev = edge;
}

static void
pred_insert_before_ref(graph_edge *edge, graph_edge *ref)
{
	if (ref->pred_prev)
	{
		ref->pred_prev->pred_next = edge;
	}
	else
	{
		ref->head->preds = edge;

	}
	edge->pred_prev = ref->pred_prev;
	edge->pred_next = ref;
	ref->pred_prev = edge;
}

graph_edge *
graph_edge_create(graph_ctx *ctx, graph_node *tail, graph_node *head, unsigned size, int (*cmp)(void *, void *))
{
	graph_edge *edge, *tmp, *prev_edge;

	assert(size >= sizeof(graph_edge));
	edge = calloc(1, size);

	ctx->version++;

	edge->tail = tail;
	edge->head = head;

	/* Insert sorted according to head.id */
	if (tail->succs)
	{
		for (tmp = tail->succs; tmp; tmp = tmp->succ_next)
		{
			if (cmp == NULL || cmp(head, tmp->head) <= 0)
			{
				succ_insert_before_ref(edge, tmp);
				break;
			}
			prev_edge = tmp;
		}
		if (!tmp)
		{
			/* Insert last */
			assert(prev_edge);
			edge->succ_prev = prev_edge;
			prev_edge->succ_next = edge;
		}
	}
	else
	{
		/* First and only edge */
		tail->succs = edge;
	}


	/* Insert sorted according to tail.id */
	if (head->preds)
	{
		for (tmp = head->preds; tmp; tmp = tmp->pred_next)
		{
			if (cmp == NULL || cmp(tail, tmp->tail) <= 0)
			{
				pred_insert_before_ref(edge, tmp);
				break;
			}
			prev_edge = tmp;
		}
		if (!tmp)
		{
			/* Insert last */
			assert(prev_edge);
			edge->pred_prev = prev_edge;
			prev_edge->pred_next = edge;
		}
	}
	else
	{
		/* First and only edge */
		head->preds = edge;
	}

	return edge;
}

void
graph_node_delete(graph_ctx *ctx, graph_node *node)
{
	ctx->version++;
	graph_succs_delete(ctx, node);
	graph_preds_delete(ctx, node);
	free(node);
}

void
graph_succs_delete(graph_ctx *ctx, graph_node *node)
{
	graph_edge *edge, *next_edge;

	ctx->version++;
	for (edge = graph_succ_first(node); edge != NULL; edge = next_edge)
	{
		next_edge = graph_succ_next(edge);
		graph_edge_delete(ctx, edge);
	}
}

void
graph_preds_delete(graph_ctx *ctx, graph_node *node)
{
	graph_edge *edge, *next_edge;

	ctx->version++;
	for (edge = graph_pred_first(node); edge != NULL; edge = next_edge)
	{
		next_edge = graph_pred_next(edge);
		graph_edge_delete(ctx, edge);
	}
}

void
graph_edge_delete(graph_ctx *ctx, graph_edge *edge)
{
	ctx->version++;
	if (edge->pred_prev == NULL)
	{
		/* First in list */
		assert(edge->head->preds == edge);
		edge->head->preds = edge->pred_next;
	}
	else
	{
		/* Not first in list */
		assert(edge->head->preds != edge);
		assert(edge->pred_prev->pred_next = edge);
		edge->pred_prev->pred_next = edge->pred_next;
	}

	if (edge->pred_next != NULL)
	{
		assert(edge->pred_next->pred_prev == edge);
		edge->pred_next->pred_prev = edge->pred_prev;
	}


	if (edge->succ_prev == NULL)
	{
		/* First in list */
		assert(edge->tail->succs == edge);
		edge->tail->succs = edge->succ_next;
	}
	else
	{
		/* Not first in list */
		assert(edge->tail->succs != edge);
		assert(edge->succ_prev->succ_next == edge);
		edge->succ_prev->succ_next = edge->succ_next;
	}

	if (edge->succ_next != NULL)
	{
		assert(edge->succ_next->succ_prev == edge);
		edge->succ_next->succ_prev = edge->succ_prev;
	}

	free(edge);
}

graph_edge *
graph_succ_first(graph_node *n)
{
	return n->succs;
}

graph_edge *
graph_succ_next(graph_edge *e)
{
	return e->succ_next;
}

graph_edge *
graph_pred_first(graph_node *n)
{
	return n->preds;
}

graph_edge *
graph_pred_next(graph_edge *e)
{
	return e->pred_next;
}

graph_node *
graph_edge_head(graph_edge *e)
{
	return e->head;
}

graph_node *
graph_edge_tail(graph_edge *e)
{
	return e->tail;
}

void
graph_marker_alloc(graph_ctx *ctx, graph_marker *marker)
{
	for (marker->idx = 0; marker->idx < GRAPH_NBR_MARKERS; marker->idx++)
	{
		if (!ctx->marker_reserv[marker->idx])
		{
			ctx->marker_reserv[marker->idx] = 1;
			marker->pattern = ++ctx->prev_marker;
			return;
		}
	}
	assert(0 && "No free node marker found!");
}

void
graph_marker_free(graph_ctx *ctx, graph_marker *marker)
{
	ctx->marker_reserv[marker->idx] = 0;
}

int
graph_marker_set(graph_node *n, graph_marker *marker)
{
	int was_set = n->markers[marker->idx] == marker->pattern;
	n->markers[marker->idx] = marker->pattern;
	return was_set;
}

int
graph_marker_is_set(graph_node *n, graph_marker *marker)
{
	return n->markers[marker->idx] == marker->pattern;
}
