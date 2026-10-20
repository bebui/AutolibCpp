// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "autolib/compiled_dfa.hpp"

#include <algorithm>

namespace autolib {

namespace {
std::size_t u(std::int64_t i) { return static_cast<std::size_t>(i); }

/// Largest transition table built, in entries
constexpr std::size_t max_table = std::size_t{1} << 22;
/// Largest range of symbols whose classes are stored in an array
constexpr std::int64_t max_direct = std::int64_t{1} << 16;
}  // namespace

compiled_dfa::compiled_dfa(const automaton& a, bool lookup_table) {
    const automaton dfa = a.is_deterministic() ? automaton{} : a.determinize();
    const automaton& d = a.is_deterministic() ? a : dfa;
    if (d.initial() == no_state)
        return;
    std::size_t n = d.state_count();
    initial_ = d.initial();
    accept_.resize(n);
    start_.assign(n + 1, 0);
    for (std::size_t s = 0; s < n; ++s) {
        auto id = static_cast<state_id>(s);
        accept_[s] = d.is_accept(id);
        std::size_t first = edges_.size();
        for (const auto& t : d.transitions(id))
            for (interval i : t.labels)
                edges_.push_back({i.min, i.max, t.target});
        std::sort(edges_.begin() + static_cast<std::ptrdiff_t>(first), edges_.end(),
                  [](const edge& x, const edge& y) { return x.min < y.min; });
        start_[s + 1] = static_cast<std::uint32_t>(edges_.size());
    }
    if (!lookup_table)
        return;

    for (const auto& e : edges_) {
        cuts_.push_back(e.min);
        cuts_.push_back(static_cast<std::int64_t>(e.max) + 1);
    }
    std::ranges::sort(cuts_);
    cuts_.erase(std::unique(cuts_.begin(), cuts_.end()), cuts_.end());
    classes_ = cuts_.size() + 1;
    if ((n + 1) * classes_ > max_table) {
        cuts_.clear();
        classes_ = 0;
        return;
    }
    // Classes of the symbols between the smallest and the largest cut read in an array, ignoring the cuts
    // at the ends of the integers that the intervals reaching them create
    auto lo = std::ranges::upper_bound(cuts_, std::int64_t{min_symbol});
    auto hi = std::ranges::lower_bound(cuts_, std::int64_t{max_symbol} + 1);
    if (lo < hi && *(hi - 1) - *lo <= max_direct && classes_ <= 0xffff) {
        direct_lo_ = static_cast<symbol>(*lo);
        direct_.resize(u(*(hi - 1) - *lo));
        std::size_t c = u(lo - cuts_.begin()) + 1;
        for (std::size_t i = 0; i < direct_.size(); ++i) {
            while (cuts_[c] <= *lo + static_cast<std::int64_t>(i))
                ++c;
            direct_[i] = static_cast<std::uint16_t>(c);
        }
        below_ = u(lo - cuts_.begin());
        above_ = u(hi - cuts_.begin());
    }
    // Row n is the dead state, absorbing, so that reading needs no test
    table_.assign((n + 1) * classes_, static_cast<state_id>(n));
    for (std::size_t s = 0; s < n; ++s) {
        for (auto e = start_[s]; e < start_[s + 1]; ++e) {
            const edge& x = edges_[e];
            auto first = u(std::ranges::upper_bound(cuts_, std::int64_t{x.min}) - cuts_.begin());
            auto last = u(std::ranges::upper_bound(cuts_, std::int64_t{x.max}) - cuts_.begin());
            for (auto c = first; c <= last; ++c)
                table_[s * classes_ + c] = x.target;
        }
    }
}

std::size_t compiled_dfa::class_of(symbol c) const noexcept {
    if (!direct_.empty()) {
        // Unsigned comparison : symbols below direct_lo_ wrap around to large offsets
        auto offset = static_cast<std::uint32_t>(c) - static_cast<std::uint32_t>(direct_lo_);
        if (offset < direct_.size())
            return direct_[offset];
        return c < direct_lo_ ? below_ : above_;
    }
    return u(std::ranges::upper_bound(cuts_, std::int64_t{c}) - cuts_.begin());
}

state_id compiled_dfa::search(state_id s, symbol c) const noexcept {
    auto lo = start_[u(s)], hi = start_[u(s) + 1];
    while (lo < hi) {
        auto mid = lo + (hi - lo) / 2;
        const edge& e = edges_[mid];
        if (e.max < c)
            lo = mid + 1;
        else if (e.min > c)
            hi = mid;
        else
            return e.target;
    }
    return no_state;
}

state_id compiled_dfa::read(state_id s, std::span<const symbol> w) const noexcept {
    if (table_.empty()) {
        for (symbol c : w) {
            if (s == no_state)
                return no_state;
            s = search(s, c);
        }
        return s;
    }
    const state_id* table = table_.data();
    const std::size_t classes = classes_;
    const auto dead = static_cast<state_id>(accept_.size());
    auto current = s == no_state ? dead : s;
    // The dead state is absorbing : it is only looked for between blocks of symbols
    constexpr std::size_t block = 64;
    if (!direct_.empty()) {
        const std::uint16_t* direct = direct_.data();
        const auto size = static_cast<std::uint32_t>(direct_.size());
        const auto lo = static_cast<std::uint32_t>(direct_lo_);
        for (std::size_t i = 0; i < w.size(); i += block) {
            auto end = std::min(w.size(), i + block);
            for (std::size_t j = i; j < end; ++j) {
                auto offset = static_cast<std::uint32_t>(w[j]) - lo;
                std::size_t c = offset < size ? direct[offset] : (w[j] < direct_lo_ ? below_ : above_);
                current = table[u(current) * classes + c];
            }
            if (current == dead)
                return no_state;
        }
    } else {
        for (std::size_t i = 0; i < w.size(); i += block) {
            auto end = std::min(w.size(), i + block);
            for (std::size_t j = i; j < end; ++j)
                current = table[u(current) * classes + class_of(w[j])];
            if (current == dead)
                return no_state;
        }
    }
    return current == dead ? no_state : current;
}

}  // namespace autolib
