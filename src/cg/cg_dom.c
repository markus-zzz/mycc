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

#include "cg/cg_dom.h"
#include "cg/cg_bb.h"
#include "cg/cg_func.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define D(x)

static cg_dom_info * intersect(cg_bb *b1, cg_bb *b2)
{
	cg_dom_info *finger1 = b1->dom_info;
	cg_dom_info *finger2 = b2->dom_info;

	while (finger1 != finger2)
	{
		while (finger1->po < finger2->po)
		{
			finger1 = finger1->idom;
		}

		while (finger2->po < finger1->po)
		{
			finger2 = finger2->idom;
		}
	}

	return finger1;
}

static void cg_dom_comp_idom(cg_func *func)
{
	cg_bb *b;
	unsigned po = func->n_bbs;
	int changed;

	for (b = func->bb_first; b != NULL; b = b->bb_next)
	{
		b->dom_info = calloc(1, sizeof(cg_dom_info));
		b->dom_info->bb = b;
		b->dom_info->po = --po;
	}

	func->bb_first->dom_info->idom = func->bb_first->dom_info;

	changed = 1;
	while (changed)
	{
		changed = 0;

		for (b = func->bb_first->bb_next; b != NULL; b = b->bb_next) /* skip entry block */
		{
			graph_edge *edge;
			cg_dom_info *new_idom;

			for (edge = graph_pred_first((graph_node *)b);
				 edge != NULL;
				 edge = graph_pred_next(edge))
			{
				cg_bb *p = (cg_bb *)graph_edge_tail(edge);
				if (p->dom_info->idom != NULL)
				{
					new_idom = p->dom_info;
					break;
				}
			}

			assert(new_idom->idom != NULL);

			for (edge = graph_pred_first((graph_node *)b);
				 edge != NULL;
				 edge = graph_pred_next(edge))
			{
				cg_bb *p = (cg_bb *)graph_edge_tail(edge);
				if (p->dom_info != new_idom && p->dom_info->idom != NULL)
				{
					new_idom = intersect(p, new_idom->bb);
				}
			}

			if (b->dom_info->idom != new_idom)
			{
				b->dom_info->idom = new_idom;
				changed = 1;
			}
		}
	}
}

static void cg_dom_comp_domtree(cg_func *func)
{
	cg_bb *b;

	for (b = func->bb_first->bb_next; b != NULL; b = b->bb_next) /* skip entry block */
	{
		cg_dom_info *idom = b->dom_info->idom;
		cg_dom_info_lst *lst = calloc(1, sizeof(cg_dom_info_lst));
		lst->next = idom->domtree_children;
		idom->domtree_children = lst;
		lst->info = b->dom_info;
	}
}

void cg_dom_print_cfg(FILE *fp, cg_func *func);
void cg_dom_print_domtree(FILE *fp, cg_func *func);

void cg_dom_setup_dom_info(cg_func *func)
{
	cg_dom_comp_idom(func);
	cg_dom_comp_domtree(func);

	cg_dom_print_cfg(stdout, func);
	cg_dom_print_domtree(stdout, func);
}

void cg_dom_destroy_dom_info(cg_func *func)
{
	cg_bb *b;

	for (b = func->bb_first; b != NULL; b = b->bb_next)
	{
		assert(b->dom_info != NULL);
		free(b->dom_info);
	}
}

void cg_dom_print_cfg(FILE *fp, cg_func *func)
{
	cg_bb *b;

	D(printf("digraph \"cfg(%s)\" {\n", func->name));

	for (b = func->bb_first; b != NULL; b = b->bb_next)
	{
		graph_edge *edge;
		for (edge = graph_succ_first((graph_node *)b);
			 edge != NULL;
			 edge = graph_succ_next(edge))
		{
			D(cg_bb *s = (cg_bb *)graph_edge_head(edge));
			D(printf("%d -> %d;\n", b->id, s->id));
		}
	}

	D(printf("}\n"));
}

void cg_dom_print_domtree(FILE *fp, cg_func *func)
{
	cg_bb *b;

	D(printf("digraph \"domtree(%s)\" {\n", func->name));

	for (b = func->bb_first; b != NULL; b = b->bb_next)
	{
		cg_dom_info *di = b->dom_info;
		struct cg_dom_info_lst *lst;

		for (lst = di->domtree_children; lst; lst = lst->next)
		{
			D(cg_bb *c = lst->info->bb);
			D(printf("%d -> %d;\n", b->id, c->id));
		}
	}

	D(printf("}\n"));
}
