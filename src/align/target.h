/****
DIAMOND protein aligner
Copyright (C) 2020 Max Planck Society for the Advancement of Science e.V.

Code developed by Benjamin Buchfink <benjamin.buchfink@tue.mpg.de>

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

#pragma once
#include <array>
#include <set>
#include <vector>
#include <stdint.h>
#include <list>
#include "../search/trace_pt_buffer.h"
#include "../basic/diagonal_segment.h"
#include "../basic/const.h"
#include "../dp/hsp_traits.h"
#include "../dp/comp_based_stats.h"
#include "extend.h"
#include "../util/data_structures/flat_array.h"
#include "../basic/parameters.h"

namespace Extension {

struct SeedHit {
	int diag() const {
		return i - j;
	}
	bool operator<(const SeedHit& x) const {
		const int d1 = diag(), d2 = x.diag();
		return d1 < d2 || (d1 == d2 && j < x.j);
	}
	int i, j;
	unsigned frame;
};

struct WorkTarget {
	WorkTarget(size_t block_id, const sequence &seq) :
		block_id(block_id),
		seq(seq),
		filter_score(0),
		ungapped_score(0),
		outranked(false)
	{}
	bool operator<(const WorkTarget &t) const {
		return filter_score > t.filter_score || (filter_score == t.filter_score && block_id < t.block_id);
	}
	size_t block_id;
	sequence seq;
	int filter_score, ungapped_score;
	bool outranked;
	std::array<std::list<Hsp_traits>, MAX_CONTEXT> hsp;
};

std::vector<WorkTarget> ungapped_stage(const sequence* query_seq, const Bias_correction* query_cb, FlatArray<SeedHit>& seed_hits, const uint32_t* target_block_ids, int flags);
void rank_targets(std::vector<WorkTarget> &targets, double ratio, double factor);

struct Target {

	Target(size_t block_id, const sequence &seq, bool outranked, int ungapped_score):
		block_id(block_id),
		seq(seq),
		filter_score(0),
		ungapped_score(ungapped_score),
		outranked(outranked)
	{}

	void add_hit(std::list<Hsp> &list, std::list<Hsp>::iterator it) {
		std::list<Hsp> &l = hsp[it->frame];
		l.splice(l.end(), list, it);
		filter_score = std::max(filter_score, (int)l.back().score);
	}

	bool operator<(const Target &t) const {
		return filter_score > t.filter_score || (filter_score == t.filter_score && block_id < t.block_id);
	}

	size_t block_id;
	sequence seq;
	int filter_score, ungapped_score;
	bool outranked;
	std::array<std::list<Hsp>, MAX_CONTEXT> hsp;
};

void score_only_culling(std::vector<Target> &targets);
std::vector<WorkTarget> gapped_filter(const sequence *query, const Bias_correction* query_cbs, std::vector<WorkTarget>& targets, Statistics &stat);
void gapped_filter(const sequence* query, const Bias_correction* query_cbs, FlatArray<SeedHit> &seed_hits, std::vector<uint32_t> &target_block_ids, Statistics& stat, int flags, const Parameters &params);
std::vector<Target> align(const std::vector<WorkTarget> &targets, const sequence *query_seq, const Bias_correction *query_cb, int flags, Statistics &stat);
std::vector<Match> align(std::vector<Target> &targets, const sequence *query_seq, const Bias_correction *query_cb, int source_query_len, int flags, Statistics &stat);
void culling(std::vector<Match> &targets, int source_query_len, const char *query_title);

}
