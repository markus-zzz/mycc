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

#ifndef IR_PRINT_H
#define IR_PRINT_H

#include "ir_bb.h"
#include "ir_node.h"

#include <stdio.h>

void
ir_print_tu(FILE *fp, ir_tu *tu);

void
ir_print_func(FILE *fp, ir_func *func);

void
ir_print_bb(FILE *fp, ir_bb *bb);

void
ir_print_node(FILE *fp, ir_node *n);

#endif
