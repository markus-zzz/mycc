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

#ifndef BSET_H
#define BSET_H

#include <stdio.h>

typedef struct bset_set bset_set;

void
bset_print(FILE *fp, bset_set *set);

bset_set *
bset_create_set(unsigned size);

void
bset_clear(bset_set *set);

void
bset_add(bset_set *set, int x);

void
bset_remove(bset_set *set, int x);

int
bset_has(bset_set *set, int x);

int
bset_equal(bset_set *src0, bset_set *src1);

void
bset_copy(bset_set *dst, bset_set *src);

void
bset_intersect(bset_set *dst, bset_set *src);

void
bset_union(bset_set *dst, bset_set *src);

void
bset_not(bset_set *dst, bset_set *src);

unsigned
bset_count(bset_set *set);

#endif
