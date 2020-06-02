/****
DIAMOND protein aligner
Copyright (C) 2013-2019 Benjamin Buchfink <buchfink@gmail.com>

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

#include <vector>
#include "../score_vector.h"
#include "../score_vector_int8.h"
#include "../score_vector_int16.h"
#include "swipe.h"
#include "../../basic/sequence.h"
#include "target_iterator.h"
#include "../../util/data_structures/mem_buffer.h"

using std::vector;
using std::list;
using namespace DISPATCH_ARCH;

namespace DP { namespace Swipe {
namespace DISPATCH_ARCH {

template<typename _sv>
struct Matrix
{
	struct ColumnIterator
	{
		ColumnIterator(_sv* hgap_front, _sv* score_front) :
			hgap_ptr_(hgap_front),
			score_ptr_(score_front)
		{ }
		inline void operator++()
		{
			++hgap_ptr_; ++score_ptr_;
		}
		inline _sv hgap() const
		{
			return *hgap_ptr_;
		}
		inline _sv diag() const
		{
			return *score_ptr_;
		}
		inline void set_hgap(const _sv& x)
		{
			*hgap_ptr_ = x;
		}
		inline void set_score(const _sv& x)
		{
			*score_ptr_ = x;
		}
		_sv *hgap_ptr_, *score_ptr_;
	};
	Matrix(int rows)
	{
		hgap_.resize(rows);
		score_.resize(rows + 1);
		std::fill(hgap_.begin(), hgap_.end(), ScoreTraits<_sv>::zero());
		std::fill(score_.begin(), score_.end(), ScoreTraits<_sv>::zero());
	}
	inline ColumnIterator begin()
	{
		return ColumnIterator(&hgap_[0], &score_[0]);
	}
	void set_zero(int c)
	{
		const int l = (int)hgap_.size();
		for (int i = 0; i < l; ++i) {
			set_channel(hgap_[i], c, ScoreTraits<_sv>::zero_score());
			set_channel(score_[i], c, ScoreTraits<_sv>::zero_score());
		}
		set_channel(score_[l], c, ScoreTraits<_sv>::zero_score());
	}
private:
	static thread_local MemBuffer<_sv> hgap_, score_;
};

template<typename _sv> thread_local MemBuffer<_sv> Matrix<_sv>::hgap_;
template<typename _sv> thread_local MemBuffer<_sv> Matrix<_sv>::score_;

#ifdef __SSE2__

template<typename _sv>
list<Hsp> swipe(const sequence &query, const sequence *subject_begin, const sequence *subject_end, int score_cutoff, vector<int> &overflow)
{
	typedef typename ScoreTraits<_sv>::Score Score;

	const int qlen = (int)query.length();
	Matrix<_sv> dp(qlen);

	const _sv open_penalty(static_cast<Score>(score_matrix.gap_open() + score_matrix.gap_extend())),
		extend_penalty(static_cast<Score>(score_matrix.gap_extend()));
	_sv best = _sv();
	SwipeProfile<_sv> profile;
	TargetBuffer<Score> targets(subject_begin, subject_end);
	list<Hsp> out;

	while (targets.active.size() > 0) {
		typename Matrix<_sv>::ColumnIterator it(dp.begin());
		_sv vgap, hgap, last;
		vgap = hgap = last = _sv();
		profile.set(targets.seq_vector());
		for (int i = 0; i < qlen; ++i) {
			hgap = it.hgap();
			const _sv next = swipe_cell_update<_sv>(it.diag(), profile.get(query[i]), nullptr, extend_penalty, open_penalty, hgap, vgap, best);
			it.set_hgap(hgap);
			it.set_score(last);
			last = next;
			++it;
		}
		it.set_score(last);
		
		for (int i = 0; i < targets.active.size();) {
			int j = targets.active[i];
			if (extract_channel(best, j) == ScoreTraits<_sv>::max_score()) {
				overflow.push_back(targets.target[j]);
				if (targets.init_target(i, j)) {
					dp.set_zero(j);
					set_channel(best, j, ScoreTraits<_sv>::zero_score());
				}
				else
					continue;
			}
			if (!targets.inc(j)) {
				const int s = ScoreTraits<_sv>::int_score(extract_channel(best, j));
				if (s >= score_cutoff)
					out.emplace_back(s, targets.target[j]);
				if (targets.init_target(i, j)) {
					dp.set_zero(j);
					set_channel(best, j, ScoreTraits<_sv>::zero_score());
				}
				else
					continue;
			}
			++i;
		}
	}

	return out;
}

#endif

list<Hsp> swipe(const sequence &query, const sequence *subject_begin, const sequence *subject_end, int score_cutoff)
{
	vector<int> overflow8, overflow16, overflow32;
#ifdef __SSE4_1__
	list<Hsp> out = swipe<score_vector<int8_t>>(query, subject_begin, subject_end, score_cutoff, overflow8);

	if (overflow8.empty())
		return out;

	vector<sequence> overflow_seq;
	overflow_seq.reserve(overflow8.size());
	for (int i : overflow8)
		overflow_seq.push_back(subject_begin[i]);
	list<Hsp> out16 = swipe<score_vector<int16_t>>(query, overflow_seq.data(), overflow_seq.data() + overflow_seq.size(), score_cutoff, overflow16);
	for (Hsp &hsp : out16)
		hsp.swipe_target = overflow8[hsp.swipe_target];
	out.splice(out.end(), out16);

	if (overflow16.empty())
		return out;

	overflow_seq.clear();
	for (int i : overflow16)
		overflow_seq.push_back(subject_begin[overflow8[i]]);
	list<Hsp> out32 = swipe<int32_t>(query, overflow_seq.data(), overflow_seq.data() + overflow_seq.size(), score_cutoff, overflow32);
	for (Hsp &hsp : out32)
		hsp.swipe_target = overflow8[overflow16[hsp.swipe_target]];
	out.splice(out.end(), out32);

	return out;
#else
	return {};
#endif
}

}}}