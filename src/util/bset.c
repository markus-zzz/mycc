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

#include "util/bset.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct bset_set {
	unsigned *bitwords;
	unsigned n_bitwords;
	unsigned size;
};

bset_set *
bset_create_set(unsigned size)
{
	bset_set *set = calloc(1, sizeof(bset_set));
	set->n_bitwords = size/(sizeof(set->bitwords[0])*8)+1;
	set->bitwords= calloc(set->n_bitwords, sizeof(set->bitwords[0]));
	set->size = size;
	return set;
}

void
bset_clear(bset_set *set)
{
	unsigned i;

	for (i = 0; i < set->n_bitwords; i++)
	{
		set->bitwords[i] = 0;
	}
}

void
bset_add(bset_set *set, int x)
{
	unsigned word_idx;
	unsigned bit_idx;

	assert(x < set->size);
	
	word_idx = x / (sizeof(set->bitwords[0])*8);
	bit_idx = x % (sizeof(set->bitwords[0])*8);
	
	set->bitwords[word_idx] |= (1 << bit_idx);
}

void
bset_remove(bset_set *set, int x)
{
	unsigned word_idx;
	unsigned bit_idx;

	assert(x < set->size);
	
	word_idx = x / (sizeof(set->bitwords[0])*8);
	bit_idx = x % (sizeof(set->bitwords[0])*8);
	
	set->bitwords[word_idx] &= ~(1 << bit_idx);
}

int
bset_has(bset_set *set, int x)
{
	unsigned word_idx;
	unsigned bit_idx;

	assert(x < set->size);
	
	word_idx = x / (sizeof(set->bitwords[0])*8);
	bit_idx = x % (sizeof(set->bitwords[0])*8);
	
	return set->bitwords[word_idx] & (1 << bit_idx);
}

int
bset_equal(bset_set *src0, bset_set *src1)
{
	unsigned i;
	assert(src0->size == src1->size);
	for (i = 0; i < src0->n_bitwords; i++)
	{
		if (src0->bitwords[i] != src1->bitwords[i])
		{
			return 0;
		}
	}

	return 1;
}

void
bset_copy(bset_set *dst, bset_set *src)
{
	unsigned i;
	assert(dst->size == src->size);
	for (i = 0; i < dst->n_bitwords; i++)
	{
		dst->bitwords[i] = src->bitwords[i];
	}
}

void
bset_intersect(bset_set *dst, bset_set *src)
{
	unsigned i;
	assert(dst->size == src->size);
	for (i = 0; i < dst->n_bitwords; i++)
	{
		dst->bitwords[i] &= src->bitwords[i];
	}
}

void
bset_union(bset_set *dst, bset_set *src)
{
	unsigned i;
	assert(dst->size == src->size);
	for (i = 0; i < dst->n_bitwords; i++)
	{
		dst->bitwords[i] |= src->bitwords[i];
	}
}

void
bset_not(bset_set *dst, bset_set *src)
{
	unsigned i;
	assert(dst->size == src->size);
	for (i = 0; i < dst->n_bitwords; i++)
	{
		dst->bitwords[i] = ~src->bitwords[i];
	}
}

unsigned
bset_count(bset_set *set)
{
	unsigned cnt = 0;
	unsigned i;
	for (i = 0; i < set->size; i++)
	{
		if (bset_has(set, i))
		{
			cnt++;
		}
	}
	return cnt;
}
	



void
bset_print(FILE *fp, bset_set *set)
{
	unsigned i;
	for (i = 0; i < set->n_bitwords; i++)
	{
		fprintf(fp, "%08x ", set->bitwords[i]);
	}
}
