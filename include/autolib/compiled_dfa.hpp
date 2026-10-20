// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Immutable deterministic automata compiled into flat arrays, to read many words quickly.

#pragma once

#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

#include "autolib/automaton.hpp"
#include "autolib/export.hpp"

namespace autolib {

/// An immutable deterministic automaton compiled into flat arrays.
///
/// The transitions of every state are stored as sorted disjoint intervals. When the automaton is small enough,
/// a transition table indexed by state and symbol class is also built, and the class of a symbol is read in an
/// array when the symbols used span a small range : reading a symbol then costs two array accesses, without any
/// search.
///
/// ```cpp
/// compiled_dfa dfa("(1|2)*12"_re);
/// bool ok = dfa({2, 1, 2});
/// ```
///
/// Instances can be shared between threads.
class AUTOLIB_EXPORT compiled_dfa {
public:
    /// A transition reading the symbols `[min, max]`.
    struct edge {
        symbol min;
        symbol max;
        state_id target;
    };

    /// The automaton recognizing the empty language, without any state.
    compiled_dfa() = default;
    /// Compiles an automaton, determinizing it first if needed. The automaton is not minimized.
    /// \param lookup_table whether to build the transition table when it is small enough
    explicit compiled_dfa(const automaton& a, bool lookup_table = true);

    /// The initial state, or `no_state` for the empty language.
    [[nodiscard]] state_id initial() const noexcept { return initial_; }
    [[nodiscard]] std::size_t state_count() const noexcept { return accept_.size(); }
    /// Checks whether a state is accepting; `no_state` is not.
    [[nodiscard]] bool is_accept(state_id s) const noexcept { return s != no_state && accept_[static_cast<std::size_t>(s)]; }
    /// The transitions of a state, sorted by symbols.
    [[nodiscard]] std::span<const edge> edges(state_id s) const noexcept {
        if (s == no_state)
            return {};
        auto i = static_cast<std::size_t>(s);
        return {edges_.data() + start_[i], edges_.data() + start_[i + 1]};
    }
    /// Whether the transition table was built.
    [[nodiscard]] bool has_lookup_table() const noexcept { return !table_.empty(); }

    /// The state reached by reading a symbol, or `no_state`.
    [[nodiscard]] state_id step(state_id s, symbol c) const noexcept {
        if (s == no_state)
            return no_state;
        if (!table_.empty()) {
            state_id t = table_[static_cast<std::size_t>(s) * classes_ + class_of(c)];
            return static_cast<std::size_t>(t) == accept_.size() ? no_state : t;
        }
        return search(s, c);
    }
    /// Checks whether a word is accepted.
    [[nodiscard]] bool run(std::span<const symbol> w) const noexcept { return is_accept(read(initial_, w)); }
    [[nodiscard]] bool run(std::initializer_list<symbol> w) const noexcept { return run(std::span(w.begin(), w.size())); }
    [[nodiscard]] bool operator()(std::span<const symbol> w) const noexcept { return run(w); }
    [[nodiscard]] bool operator()(std::initializer_list<symbol> w) const noexcept { return run(w); }
    /// The state reached by reading a word from a state, or `no_state`.
    [[nodiscard]] state_id read(state_id s, std::span<const symbol> w) const noexcept;

private:
    [[nodiscard]] state_id search(state_id s, symbol c) const noexcept;
    [[nodiscard]] std::size_t class_of(symbol c) const noexcept;

    state_id initial_ = no_state;
    std::vector<char> accept_;
    std::vector<std::uint32_t> start_{0};
    std::vector<edge> edges_;

    // Symbol classes : class i holds the symbols of [cuts_[i-1], cuts_[i]), class 0 those below cuts_[0]
    std::size_t classes_ = 0;
    std::vector<std::int64_t> cuts_;
    std::vector<std::uint16_t> direct_;  ///< class of the symbols direct_lo_ + i, when their range is small
    symbol direct_lo_ = 0;
    std::size_t below_ = 0;  ///< class of the symbols below direct_lo_
    std::size_t above_ = 0;  ///< class of the symbols beyond the direct range
    std::vector<state_id> table_;        ///< target of every (state, class), the dead state being state_count()
};

}  // namespace autolib
