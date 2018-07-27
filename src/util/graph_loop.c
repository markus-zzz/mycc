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

#include "util/graph_loop.h"
#include "util/graph.h"
#include "util/dset.h"
#include <assert.h>
#include <stdlib.h>

static void
dfs(graph_ctx *gctx,
    graph_node *start,
    graph_node **preorder,
    const unsigned n_nodes,
    graph_loop_get_info get_info,
    dset_ctx *dset)
{
	graph_marker marker;
	graph_node *stack[n_nodes*2]; /* each node has at most two successors */
	unsigned stackidx = 0;
	unsigned pre_counter = 0;
	unsigned rpost_counter = n_nodes;

	graph_marker_alloc(gctx, &marker);

	stack[stackidx++] = start;
	while (stackidx > 0)
	{
		if (!graph_marker_set(stack[stackidx-1], &marker))
		{
			/* pre order action */
			graph_edge *edge;

			dset_makeset(dset, pre_counter);
			get_info(stack[stackidx-1])->header = NULL;
			get_info(stack[stackidx-1])->type = GRAPH_LOOP_NONHEADER;
			get_info(stack[stackidx-1])->pre = pre_counter;
			get_info(stack[stackidx-1])->rpost = -1;
			preorder[pre_counter] = stack[stackidx-1];
			pre_counter++;

			for (edge = graph_succ_first(stack[stackidx-1]); edge; edge = graph_succ_next(edge))
			{
				graph_node *succ = graph_edge_head(edge);
				if (!graph_marker_is_set(succ, &marker))
				{
					assert(stackidx < sizeof(stack)/sizeof(stack[0]));
					stack[stackidx++] = succ;
				}
			}
		}

		if (graph_marker_is_set(stack[stackidx-1], &marker))
		{
			if (get_info(stack[stackidx-1])->rpost == -1)
			{
				/* post order action */
				get_info(stack[stackidx-1])->rpost = --rpost_counter;
			}
			stackidx--;
		}
	}

	graph_marker_free(gctx, &marker);

	assert(pre_counter == n_nodes);
	assert(rpost_counter == 0);
}

static int
is_ancestor(graph_node *x,
            graph_node *y,
            graph_loop_get_info get_info)
{
	graph_loop_bb_info *x_info = get_info(x);
	graph_loop_bb_info *y_info = get_info(y);

	return x_info->pre < y_info->pre && x_info->rpost < y_info->rpost;
}

#define PRE(x) (get_info(x)->pre)

void
graph_loop_analyze(graph_ctx *gctx,
                   graph_node *start,
                   unsigned n_nodes,
                   graph_loop_get_info get_info)
{
	graph_node *preorder[n_nodes];
	dset_ctx *dset = dset_create_universe(n_nodes);
	unsigned i;

	dfs(gctx, start, preorder, n_nodes, get_info, dset);

	/* For all nodes in reverse pre-order */
	for (i = 0; i < n_nodes; i++)
	{
		graph_node *w = preorder[n_nodes-i-1];
		graph_node *P[n_nodes];
		graph_node *worklist[n_nodes];
		graph_edge *edge;
		graph_marker pmarker;
		unsigned pidx = 0;
		unsigned workidx = 0;

		graph_marker_alloc(gctx, &pmarker);

		for (edge = graph_pred_first(w); edge; edge = graph_pred_next(edge))
		{
			graph_node *v = graph_edge_tail(edge);
			if (v != w && is_ancestor(w, v, get_info))
			{
				/* v is a back-edge predecessor for w */
				if (v != w)
				{
					P[pidx++] = preorder[dset_find(dset, PRE(v))];
					graph_marker_set(P[pidx-1], &pmarker);
				}
				else
				{
					get_info(w)->type = GRAPH_LOOP_SELF;
				}
			}
		}

		/* worklist := P */
		for (workidx = 0; workidx < pidx; workidx++)
		{
			worklist[workidx] = P[workidx];
		}

		if (pidx > 0)
		{
			get_info(w)->type = GRAPH_LOOP_HEADER;
		}

		/* select a node x from worklist and delete it from worklist */
		while (workidx > 0)
		{
			unsigned j;
			graph_node *x = worklist[--workidx];

			for (edge = graph_pred_first(x); edge; edge = graph_pred_next(edge))
			{
				graph_node *y = graph_edge_tail(edge);
				if (!is_ancestor(x, y, get_info))
				{
					/* y is a non-back-edge predecessor of x */
					graph_node *yprime = preorder[dset_find(dset, PRE(y))];
					if (yprime != w && !is_ancestor(w, yprime, get_info))
					{
						assert(0 && "irreducible");
					}
					else if (yprime != w && !graph_marker_set(yprime, &pmarker))
					{
						P[pidx++] = yprime;
						worklist[workidx++] = yprime;
					}
				}
			}

			for (j = 0; j < pidx; j++)
			{
				get_info(P[j])->header = w;
				dset_union(dset, PRE(P[j]), PRE(w));
			}
		}

		graph_marker_free(gctx, &pmarker);
	}
}
