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

#ifndef CG_COND_H
#define CG_COND_H

typedef enum cg_cond {
#define DEF_CG_COND(x) CG_COND_##x,
#include "cg/cg_cond.def"
#undef DEF_CG_COND
} cg_cond;

cg_cond
cg_cond_inv(cg_cond cond);

#endif
