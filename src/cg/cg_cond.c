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

#include "cg/cg_cond.h"

cg_cond
cg_cond_inv(cg_cond cond)
{
	switch (cond)
	{
		case CG_COND_eq: return CG_COND_ne;
		case CG_COND_ne: return CG_COND_eq;
		case CG_COND_le: return CG_COND_gt;
		case CG_COND_lt: return CG_COND_ge;
		case CG_COND_ge: return CG_COND_lt;
		case CG_COND_gt: return CG_COND_le;
		default: return cond;
	}
}
