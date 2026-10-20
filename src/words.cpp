// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "autolib/words.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "detail/big_integer_access.hpp"

namespace autolib {

namespace {
std::size_t u(std::int64_t i) { return static_cast<std::size_t>(i); }
constexpr std::size_t before_first = std::numeric_limits<std::size_t>::max();
}  // namespace

struct word_range::iterator::shared {
    compiled_dfa dfa;
    int max_length;
    /// can_accept[l][s] : a word of length exactly l is accepted from s
    std::vector<std::vector<char>> can_accept;

    [[nodiscard]] bool accepts_from(int length, state_id s) const {
        return s != no_state && can_accept[u(length)][u(s)];
    }
};

word_range::word_range(const automaton& a, int max_length) {
    if (max_length < 0)
        throw std::invalid_argument("Negative length: " + std::to_string(max_length));
    auto data = std::make_shared<iterator::shared>(iterator::shared{compiled_dfa(a, false), max_length, {}});
    const auto& dfa = data->dfa;
    auto n = dfa.state_count();
    data->can_accept.assign(u(max_length) + 1, std::vector<char>(n, 0));
    for (std::size_t s = 0; s < n; ++s)
        data->can_accept[0][s] = dfa.is_accept(static_cast<state_id>(s));
    for (std::size_t l = 1; l <= u(max_length); ++l)
        for (std::size_t s = 0; s < n; ++s)
            for (const auto& e : dfa.edges(static_cast<state_id>(s)))
                if (data->can_accept[l - 1][u(e.target)]) {
                    data->can_accept[l][s] = 1;
                    break;
                }
    data_ = std::move(data);
}

word_range::iterator::iterator(std::shared_ptr<const shared> data) : data_(std::move(data)), done_(false) {
    if (next_length())
        advance(false);
    else
        done_ = true;
}

bool word_range::iterator::next_length() {
    state_id init = data_->dfa.initial();
    do {
        ++length_;
    } while (length_ <= data_->max_length && !data_->accepts_from(length_, init));
    if (length_ > data_->max_length)
        return false;
    states_.assign(u(length_) + 1, no_state);
    edges_.assign(u(length_), before_first);
    word_.assign(u(length_), 0);
    states_[0] = init;
    depth_ = 0;
    return true;
}

bool word_range::iterator::next_symbol() {
    auto edges = data_->dfa.edges(states_[u(depth_)]);
    auto& e = edges_[u(depth_)];
    auto& s = word_[u(depth_)];
    if (e != before_first && s < edges[e].max) {
        ++s;
        return true;
    }
    int remaining = length_ - depth_ - 1;
    for (auto next = e == before_first ? 0 : e + 1; next < edges.size(); ++next) {
        if (data_->accepts_from(remaining, edges[next].target)) {
            e = next;
            s = edges[next].min;
            return true;
        }
    }
    return false;
}

void word_range::iterator::advance(bool resume) {
    bool descending = !resume;
    while (true) {
        if (descending) {
            if (depth_ == length_)
                return;  // word_ holds the next word
            edges_[u(depth_)] = before_first;
        }
        // Next symbol at the current depth, or backtracking
        if (depth_ == length_ || !next_symbol()) {
            if (depth_ == 0) {
                if (!next_length()) {
                    done_ = true;
                    return;
                }
                descending = true;
                continue;
            }
            --depth_;
            descending = false;
            continue;
        }
        states_[u(depth_) + 1] = data_->dfa.edges(states_[u(depth_)])[edges_[u(depth_)]].target;
        ++depth_;
        descending = true;
    }
}

namespace {

using limb = big_integer::limb;
using access = big_integer_access;

limb width(const compiled_dfa::edge& e) {
    return static_cast<limb>(static_cast<std::int64_t>(e.max) - e.min + 1);
}

/// One row of the counting table : the number of words of a given length accepted from every state, the
/// magnitude of the count of state s being limbs[offsets[s] .. offsets[s+1]).
///
/// Appends to `to` the next row, computed from the row stored in `from` (which may be `to`) at `from_offsets`,
/// and pushes the end of each count on `to_offsets`. `from_max` is the largest number of limbs of a count of the
/// row, and the one of the new row is returned. The sums are made in place in a buffer, the products being
/// replaced by additions for the transitions reading a single symbol.
std::size_t append_row(const compiled_dfa& dfa, const std::vector<limb>& from, const std::size_t* from_offsets,
                       std::size_t from_max, std::vector<limb>& to, std::vector<std::size_t>& to_offsets,
                       std::vector<limb>& sum) {
    using detail::add_carry;
    std::size_t n = dfa.state_count();
    // A sum of fewer than 2^32 counts times widths below 2^32 has at most from_max + 2 limbs
    std::size_t bound = from_max + 3;
    sum.resize(bound);
    const limb* counts = from.data();
    limb* acc = sum.data();
    std::size_t row_max = 0;
    for (std::size_t s = 0; s < n; ++s) {
        std::size_t length = 0;
        for (const auto& e : dfa.edges(static_cast<state_id>(s))) {
            const limb* m = counts + from_offsets[u(e.target)];
            std::size_t mn = from_offsets[u(e.target) + 1] - from_offsets[u(e.target)];
            if (mn == 0)
                continue;
            while (length <= mn)
                acc[length++] = 0;
            limb b = width(e), carry = 0;
            std::size_t i;
            if (b == 1) {
                for (i = 0; i < mn; ++i)
                    acc[i] = add_carry(acc[i], m[i], carry);
            } else {
                limb mul_carry = 0;
                for (i = 0; i < mn; ++i) {
                    auto w = detail::mul_wide(m[i], b);
                    limb lo = w.lo + mul_carry;
                    mul_carry = w.hi + (lo < w.lo);
                    acc[i] = add_carry(acc[i], lo, carry);
                }
                acc[i] = add_carry(acc[i], mul_carry, carry);
                ++i;
            }
            for (; carry != 0; ++i) {
                if (i == length)
                    acc[length++] = 0;
                acc[i] = add_carry(acc[i], 0, carry);
            }
        }
        while (length > 0 && acc[length - 1] == 0)
            --length;
        if (to.size() + length > to.capacity()) {
            to.reserve(std::max(to.size() + length, 2 * to.capacity()));
            counts = from.data();  // from may be to
        }
        to.insert(to.end(), acc, acc + length);
        to_offsets.push_back(to.size());
        row_max = std::max(row_max, length);
    }
    return row_max;
}

/// The same as append_row when the counts of both rows fit in a limb : machine arithmetic. Returns false, leaving
/// `to` and `to_offsets` unchanged, when a count of the new row overflows.
bool append_small_row(const compiled_dfa& dfa, const std::vector<limb>& from, const std::size_t* from_offsets,
                      std::vector<limb>& to, std::vector<std::size_t>& to_offsets) {
    std::size_t n = dfa.state_count(), size = to.size(), offsets = to_offsets.size();
    if (to.size() + n > to.capacity())
        to.reserve(std::max(to.size() + n, 2 * to.capacity()));
    const limb* counts = from.data();  // after the reservation, from being possibly to
    for (std::size_t s = 0; s < n; ++s) {
        limb total = 0;
        for (const auto& e : dfa.edges(static_cast<state_id>(s))) {
            std::size_t first = from_offsets[u(e.target)];
            if (first == from_offsets[u(e.target) + 1])
                continue;
            auto product = detail::mul_wide(counts[first], width(e));
            total += product.lo;
            if (product.hi != 0 || total < product.lo) {
                to.resize(size);
                to_offsets.resize(offsets);
                return false;
            }
        }
        if (total != 0)
            to.push_back(total);
        to_offsets.push_back(to.size());
    }
    return true;
}

/// The first row : 1 for the accepting states.
std::size_t append_first_row(const compiled_dfa& dfa, std::vector<limb>& to, std::vector<std::size_t>& to_offsets) {
    for (std::size_t s = 0; s < dfa.state_count(); ++s) {
        if (dfa.is_accept(static_cast<state_id>(s)))
            to.push_back(1);
        to_offsets.push_back(to.size());
    }
    return 1;
}

void check_length(int length) {
    if (length < 0)
        throw std::invalid_argument("Negative length: " + std::to_string(length));
}

/// An estimate of the number of limbs of all the counts up to a length, by the same dynamic programming on
/// doubles, each row being scaled by its maximum. Counts too small to be seen beside the largest one of their
/// row are ignored : the estimate only spares the reallocations of the table.
std::size_t estimate_limbs(const compiled_dfa& dfa, int length) {
    std::size_t n = dfa.state_count();
    std::vector<double> previous(n), row(n);
    for (std::size_t s = 0; s < n; ++s)
        previous[s] = dfa.is_accept(static_cast<state_id>(s)) ? 1.0 : 0.0;
    std::size_t total = 0;
    double scale = 0;  // log2 of the factor removed from the counts
    auto add_row = [&](const std::vector<double>& counts) {
        for (double c : counts)
            if (c > 0)
                total += static_cast<std::size_t>(std::max(0.0, (std::ilogb(c) + scale) / 64 + 1e-9)) + 1;
    };
    add_row(previous);
    for (int l = 1; l <= length; ++l) {
        double largest = 0;
        for (std::size_t s = 0; s < n; ++s) {
            double total_from = 0;
            for (const auto& e : dfa.edges(static_cast<state_id>(s)))
                total_from += previous[u(e.target)] * static_cast<double>(width(e));
            row[s] = total_from;
            largest = std::max(largest, total_from);
        }
        if (largest > 0) {
            for (double& c : row)
                c /= largest;
            scale += std::log2(largest);
        }
        add_row(row);
        std::swap(previous, row);
    }
    return total;
}

/// The value of a magnitude of at most one limb.
limb small(std::span<const limb> m) noexcept {
    return m.empty() ? 0 : m[0];
}

}  // namespace

big_integer count_words(const automaton& a, int length) {
    check_length(length);
    compiled_dfa dfa(a, false);
    if (dfa.initial() == no_state)
        return {};
    // Two rows, used in turn
    std::vector<limb> limbs, next_limbs, sum;
    std::vector<std::size_t> offsets{0}, next_offsets;
    std::size_t largest = append_first_row(dfa, limbs, offsets);
    for (int l = 1; l <= length; ++l) {
        next_limbs.clear();
        next_offsets.assign(1, 0);
        if (largest > 1 || !append_small_row(dfa, limbs, offsets.data(), next_limbs, next_offsets))
            largest = append_row(dfa, limbs, offsets.data(), largest, next_limbs, next_offsets, sum);
        std::swap(limbs, next_limbs);
        std::swap(offsets, next_offsets);
    }
    auto s = u(dfa.initial());
    return big_integer::from_magnitude({limbs.data() + offsets[s], limbs.data() + offsets[s + 1]});
}

word_sampler::word_sampler(const automaton& a, int length) : dfa_(a, false), length_(length), states_(dfa_.state_count()) {
    check_length(length);
    // Every row is appended to the same table : its size is estimated first, to spare the reallocations
    offsets_.reserve(u(length + 1) * states_ + 1);
    offsets_.push_back(0);
    limbs_.reserve(estimate_limbs(dfa_, length));
    std::vector<limb> sum;
    std::size_t largest = append_first_row(dfa_, limbs_, offsets_);
    for (int l = 1; l <= length; ++l) {
        const std::size_t* previous = offsets_.data() + u(l - 1) * states_;
        if (largest > 1 || !append_small_row(dfa_, limbs_, previous, limbs_, offsets_))
            largest = append_row(dfa_, limbs_, previous, largest, limbs_, offsets_, sum);
    }
    if (dfa_.initial() != no_state)
        count_ = big_integer::from_magnitude(from(length_, dfa_.initial()));
}

word word_sampler::unrank(const big_integer& rank) const {
    if (rank.signum() < 0 || rank >= count_)
        throw std::out_of_range("Rank " + rank.to_string() + " out of [0, " + count_.to_string() + ")");
    word w(u(length_));
    state_id s = dfa_.initial();
    if (count_.magnitude().size() <= 1) {
        // Every count on the way is at most count_ : machine integers
        auto r = *rank.to<limb>();
        for (int i = 0; i < length_; ++i) {
            int remaining = length_ - i - 1;
            for (const auto& e : dfa_.edges(s)) {
                limb each = small(from(remaining, e.target));
                if (each == 0)
                    continue;
                limb weight = each * width(e);
                if (r < weight) {
                    w[u(i)] = static_cast<symbol>(e.min + static_cast<std::int64_t>(r / each));
                    r %= each;
                    s = e.target;
                    break;
                }
                r -= weight;
            }
        }
        return w;
    }
    big_integer r = rank, each, weight;
    for (int i = 0; i < length_; ++i) {
        int remaining = length_ - i - 1;
        for (const auto& e : dfa_.edges(s)) {
            auto m = from(remaining, e.target);
            if (m.empty())
                continue;
            access::assign_magnitude(each, m);
            weight = 0;
            access::add_product(weight, m.data(), m.size(), width(e));
            if (r < weight) {
                auto [q, rest] = big_integer::div_mod(r, each);
                w[u(i)] = static_cast<symbol>(e.min + *q.to<std::int64_t>());
                r = std::move(rest);
                s = e.target;
                break;
            }
            r -= weight;
        }
    }
    return w;
}

std::optional<big_integer> word_sampler::rank(std::span<const symbol> w) const {
    if (w.size() != u(length_) || !dfa_(w))
        return std::nullopt;
    big_integer r;
    state_id s = dfa_.initial();
    for (int i = 0; i < length_; ++i) {
        int remaining = length_ - i - 1;
        symbol c = w[u(i)];
        for (const auto& e : dfa_.edges(s)) {
            auto m = from(remaining, e.target);
            if (c <= e.max) {
                access::add_product(r, m.data(), m.size(), static_cast<limb>(static_cast<std::int64_t>(c) - e.min));
                s = e.target;
                break;
            }
            access::add_product(r, m.data(), m.size(), width(e));
        }
    }
    return r;
}

}  // namespace autolib
