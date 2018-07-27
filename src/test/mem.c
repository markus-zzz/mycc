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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"

#define D(x)

/* Implements a sparse memory model with 64 bit addressing using a
   fake MMU with an eight level page table. */

#define PT_BITS 8

struct memctx {
	void **pt0;
};

memctx * mem_new(void)
{
	memctx *ctx = calloc(1, sizeof(memctx));
	ctx->pt0 = calloc(1 << PT_BITS, sizeof(void *));

	return ctx;
}

static void mem_free_worker_1(void **p, int level)
{
	unsigned size = 1 << PT_BITS;

	if (p == NULL)
	{
		return;
	}

	if (level < 8)
	{
		unsigned i;
		for (i = 0; i < size; i++)
		{
			mem_free_worker_1(p[i], level + 1);
		}
	}

	free(p);
}

void mem_free(memctx *ctx)
{
	/* Walk the tree and free all tables and pages */
	mem_free_worker_1(ctx->pt0, 0);
	free(ctx);
}

static void * mem_copy_worker_1(void **p, int level)
{
	unsigned size = 1 << PT_BITS;

	if (p == NULL)
	{
		return NULL;
	}

	if (level < 8)
	{
		void **new_pt;
		unsigned i;
		new_pt = calloc(size, sizeof(void *));
		for (i = 0; i < size; i++)
		{
			new_pt[i] = mem_copy_worker_1(p[i], level + 1);
		}
		return new_pt;
	}
	else
	{
		uint8_t *new_page = calloc(2*size, sizeof(uint8_t));
		memcpy(new_page, p, 2*size);
		return new_page;
	}
}

memctx * mem_copy(memctx *ctx)
{
	memctx *new_ctx = calloc(1, sizeof(memctx));
	/* Walk the tree and copy all tables and pages */
	new_ctx->pt0 = mem_copy_worker_1(ctx->pt0, 0);
	return new_ctx;
}

void mem_write(memctx *ctx, uint64_t addr, uint8_t value, uint8_t valid)
{
	void **pt = ctx->pt0;
	unsigned idx;
	uint8_t *page;
	int i;

	for (i = 7; i >= 0; i--)
	{
		idx = (addr >> (PT_BITS * i)) & ((1 << PT_BITS) - 1);

		if (pt[idx] == NULL)
		{
			unsigned size = 1 << PT_BITS;
			if (i == 0)
			{
				/* This is the actual page so need to make room for
				   the valid bits */
				pt[idx] = calloc(2*size, sizeof(uint8_t));
			}
			else
			{
				pt[idx] = calloc(size, sizeof(void *));
			}
		}

		pt = pt[idx];
	}

	page = (uint8_t *)pt;
	idx = addr & 0xff;
	page[idx] = value;
	page[idx + (1 << PT_BITS)] = valid;
	D(printf("mem_write(addr=0x%llx, value=0x%x)\n", addr, value));
}

void mem_read(memctx *ctx, uint64_t addr, uint8_t *value, uint8_t *valid)
{
	void **pt = ctx->pt0;
	unsigned idx;
	uint8_t *page;
	int i;

	for (i = 7; i >= 0; i--)
	{
		idx = (addr >> (PT_BITS * i)) & ((1 << PT_BITS) - 1);

		if (pt[idx] == NULL)
		{
			*value = 0x00;
			*valid = 0x00;
			return;
		}

		pt = pt[idx];
	}

	page = (uint8_t *)pt;
	idx = addr & 0xff;
	*value = page[idx];
	*valid = page[idx + (1 << PT_BITS)];
	D(printf("mem_read(addr=0x%llx, value=0x%x)\n", addr, *value));
}

#if MEM_TEST
void try_read(memctx *ctx, uint64_t addr)
{
	uint8_t value, valid;
	mem_read(ctx, addr, &value, &valid);
	printf("addr: 0x%x, value=0x%x, valid=0x%x\n", addr, value, valid);
}

int main(int argc, char **argv)
{
	memctx *ctx;
	memctx *ctx2;
	int i;

	ctx = mem_new();
#if 0
	mem_write(ctx, 0x324, 0x23);
	mem_write(ctx, 0x325, 0x27);

	mem_write(ctx, 0x320086, 0xFE);

	try_read(ctx, 0x325);
#endif

	for (i = 0; i < 1024; i++)
	{
		mem_write(ctx, i, i);
	}

	ctx2 = mem_copy(ctx);
	mem_free(ctx);

	for (i = 0; i < 1024; i++)
	{
		try_read(ctx2, i);
	}

	mem_free(ctx2);

	return 0;
}
#endif
