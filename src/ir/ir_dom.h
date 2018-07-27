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

#ifndef IR_DOM_H
#define IR_DOM_H

#include "ir/ir.h"

typedef struct ir_dom_info_lst
{
	struct ir_dom_info_lst *next;
	struct ir_dom_info *info;
} ir_dom_info_lst;

typedef struct ir_dom_info
{
	ir_bb *bb;
	struct ir_dom_info *idom;
	struct ir_dom_info_lst *df;
	struct ir_dom_info_lst *domtree_children;
	unsigned po;

} ir_dom_info;

void ir_dom_setup_dom_info(ir_func *func);
void ir_dom_destroy_dom_info(ir_func *func);

#endif
