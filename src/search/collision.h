/****
DIAMOND protein aligner
Copyright (C) 2013-2017 Benjamin Buchfink <buchfink@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
****/

#ifndef COLLISION_H_
#define COLLISION_H_

#include "../data/frequent_seeds.h"
#include "sse_dist.h"

bool is_primary_hit(const Letter *query,
	const Letter *subject,
	const unsigned seed_offset,
	const unsigned sid,
	const unsigned len);

#endif /* COLLISION_H_ */
