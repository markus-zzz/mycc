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

#include "util/dset.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* TODO:FIXME: Add union by rank and path compression */

struct dset_ctx {
	int *parent;
	int *rank;
	unsigned size;
};

dset_ctx *
dset_create_universe(unsigned size)
{
	dset_ctx *d = calloc(1, sizeof(dset_ctx));
	d->parent = calloc(size, sizeof(d->parent[0]));
	d->rank = calloc(size, sizeof(d->rank[0]));
	memset(d->parent, -1, size*sizeof(d->parent[0]));
	d->size = size;
	return d;
}

void
dset_makeset(dset_ctx *ctx, int x)
{
	assert(x < ctx->size);
	ctx->parent[x] = x;
}

int
dset_find(dset_ctx *ctx, int x)
{
	assert(x < ctx->size);
	if (ctx->parent[x] == x)
	{
		return x;
	}
	else
	{
		return dset_find(ctx, ctx->parent[x]);
	}
}

void
dset_union(dset_ctx *ctx, int x, int y)
{
	int xroot = dset_find(ctx, x);
	int yroot = dset_find(ctx, y);

	ctx->parent[xroot] = yroot;
}

