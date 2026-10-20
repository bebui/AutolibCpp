// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <algorithm>
#include <utility>
#include <vector>

#include "autolib/automaton.hpp"

namespace autolib::detail {

/// Unchecked access to the representation of automata, for the algorithms of the library.
struct access {
    /// Adds a transition without updating the determinism flag : the caller knows the result.
    static void add(automaton& a, state_id from, state_id to, interval_set labels) {
        a.states_[static_cast<std::size_t>(from)].out.push_back({to, std::move(labels)});
    }
    static void add_epsilon(automaton& a, state_id from, state_id to) {
        a.states_[static_cast<std::size_t>(from)].eps.push_back(to);
    }
    static void set_deterministic(automaton& a, bool deterministic) { a.deterministic_ = deterministic; }
    /// Recomputes the determinism flag in O(m log m) : no epsilon transition, and the labels leaving each state are disjoint.
    static void update_determinism(automaton& a) {
        std::vector<interval> all;
        for (const auto& s : a.states_) {
            if (!s.eps.empty()) {
                a.deterministic_ = false;
                return;
            }
            all.clear();
            for (const auto& t : s.out)
                all.insert(all.end(), t.labels.begin(), t.labels.end());
            std::ranges::sort(all);
            for (std::size_t i = 1; i < all.size(); ++i) {
                if (all[i].min <= all[i - 1].max) {
                    a.deterministic_ = false;
                    return;
                }
            }
        }
        a.deterministic_ = true;
    }
    static void reserve(automaton& a, std::size_t states) { a.states_.reserve(states); }
    static state_id embed(automaton& a, const automaton& other) { return a.embed(other); }
};

}  // namespace autolib::detail
