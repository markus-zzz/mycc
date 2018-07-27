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

#ifndef MEM_H
#define MEM_H

#include <stdint.h>

typedef struct memctx memctx;

memctx * mem_new(void);

void mem_free(memctx *ctx);

memctx * mem_copy(memctx *ctx);

void mem_write(memctx *ctx, uint64_t addr, uint8_t value, uint8_t valid);

void mem_read(memctx *ctx, uint64_t addr, uint8_t *value, uint8_t *valid);

#endif
