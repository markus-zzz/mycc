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

#include "ir/ir_dom.h"
#include "ir/ir_bb_private.h"
#include "ir/ir_func.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static ir_dom_info * intersect(ir_bb *b1, ir_bb *b2)
{
	ir_dom_info *finger1 = b1->dom_info;
	ir_dom_info *finger2 = b2->dom_info;

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

static void ir_dom_comp_idom(ir_func *func)
{
	ir_bb_iter bit;
	ir_bb *b;
	unsigned po = func->n_ir_bbs;
	int changed;

	ir_bb_iter_init(&bit, func);
	while ((b = ir_bb_iter_next(&bit)))
	{
		b->dom_info = calloc(1, sizeof(ir_dom_info));
		b->dom_info->bb = b;
		b->dom_info->po = --po;
	}

	func->entry->dom_info->idom = func->entry->dom_info;

	changed = 1;
	while (changed)
	{
		changed = 0;

		ir_bb_iter_init(&bit, func);
		(void)ir_bb_iter_next(&bit); /* skip entry node */
		while ((b = ir_bb_iter_next(&bit)))
		{
			graph_edge *edge;
			ir_dom_info *new_idom;

			for (edge = graph_pred_first((graph_node *)b);
				 edge != NULL;
				 edge = graph_pred_next(edge))
			{
				ir_bb *p = (ir_bb *)graph_edge_tail(edge);
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
				ir_bb *p = (ir_bb *)graph_edge_tail(edge);
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

static void ir_dom_comp_domtree(ir_func *func)
{
	ir_bb_iter bit;
	ir_bb *b;

	ir_bb_iter_init(&bit, func);
	(void)ir_bb_iter_next(&bit); /* skip entry block */
	while ((b = ir_bb_iter_next(&bit)))
	{
		ir_dom_info *idom = b->dom_info->idom;
		ir_dom_info_lst *lst = calloc(1, sizeof(ir_dom_info_lst));
		lst->next = idom->domtree_children;
		idom->domtree_children = lst;
		lst->info = b->dom_info;
	}
}

static void ir_dom_comp_df(ir_func *func)
{
	ir_bb_iter bit;
	ir_bb *b;

	ir_bb_iter_init(&bit, func);
	while ((b = ir_bb_iter_next(&bit)))
	{
		graph_edge *edge = graph_pred_first((graph_node *)b);
		if (edge != NULL && graph_pred_next(edge) != NULL)
		{
			for (edge = graph_pred_first((graph_node *)b);
				 edge != NULL;
				 edge = graph_pred_next(edge))
			{
				ir_bb *p = (ir_bb *)graph_edge_tail(edge);
				ir_dom_info *runner = p->dom_info;
				while (runner != b->dom_info->idom)
				{
					/* add b to runners df set */
					ir_dom_info_lst *lst = calloc(1, sizeof(ir_dom_info_lst));
					lst->next = runner->df;
					runner->df = lst;
					lst->info = b->dom_info;

					assert(runner->idom != NULL);
					runner = runner->idom;
				}
			}
		}
	}
}

void ir_dom_dump_domtree(FILE *fp, ir_func *func)
{
	ir_bb_iter bit;
	ir_bb *b;

	fprintf(fp, "digraph %s {\n", func->name);

	ir_bb_iter_init(&bit, func);
	while ((b = ir_bb_iter_next(&bit)))
	{
		ir_dom_info_lst *lst;
		for (lst = b->dom_info->domtree_children; lst != NULL; lst = lst->next)
		{
			fprintf(fp, "  bb%d -> bb%d\n", b->id, lst->info->bb->id);
		}
	}
	fprintf(fp, "}\n");
}

void ir_dom_dump_df(FILE *fp, ir_func *func)
{
	ir_bb_iter bit;
	ir_bb *b;

	ir_bb_iter_init(&bit, func);
	while ((b = ir_bb_iter_next(&bit)))
	{
		ir_dom_info_lst *lst;
		fprintf(fp, "DF(bb%d) = { ", b->id);
		for (lst = b->dom_info->df; lst != NULL; lst = lst->next)
		{
			fprintf(fp, "bb%d ", lst->info->bb->id);
		}
		fprintf(fp, "}\n");
	}
}

void ir_dom_setup_dom_info(ir_func *func)
{
	ir_dom_comp_idom(func);
	ir_dom_comp_domtree(func);
	ir_dom_comp_df(func);
}

void ir_dom_destroy_dom_info(ir_func *func)
{
	ir_bb_iter bit;
	ir_bb *b;

	ir_bb_iter_init(&bit, func);
	while ((b = ir_bb_iter_next(&bit)))
	{
		assert(b->dom_info != NULL);
		free(b->dom_info);
	}
}
