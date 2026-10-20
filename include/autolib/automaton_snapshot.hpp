// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// The bridge between automata and their weightings.

#pragma once

#include <functional>
#include <initializer_list>
#include <span>
#include <vector>

#include "autolib/automaton.hpp"
#include "autolib/export.hpp"
#include "autolib/multi_weighted_automaton.hpp"
#include "autolib/weighted_automaton.hpp"
#include "autolib/weighted_graph.hpp"

namespace autolib {

/// The structure of an automaton captured once, to weight it with several costs sharing the same graph : the
/// same edge numbers for all the weightings, their scalarizations, compiled indexes and unfoldings.
///
/// ```cpp
/// auto source = "12+"_re;
/// auto direct = source.weighted([](const automaton_edge&) { return 2.0; });   // a fresh snapshot
/// double c = direct({1, 2, 2});                                                  // 6
///
/// auto snapshot = source.snapshot();
/// auto cost = snapshot.weighted([](const automaton_edge&) { return 2.0; });
/// auto duration = snapshot.weighted([](const automaton_edge& e) { return e.epsilon ? 0.0 : 5.0; });
/// auto both = snapshot.multi_weighted({[](const automaton_edge&) { return 2.0; },
///                                      [](const automaton_edge&) { return 5.0; }});
/// bool shared = cost.graph().same_as(both.graph());                             // true
/// automaton language = cost.to_automaton();                                     // back, without the costs
/// ```
///
/// The states keep their numbers, and every edge knows its transition. Each transition of the automaton
/// becomes an edge, then each epsilon transition, state after state. The cost functions are called when a
/// weighting is created, once per edge and dimension.
class AUTOLIB_EXPORT automaton_snapshot {
public:
    /// Captures the structure of an automaton, without determinizing nor minimizing it.
    explicit automaton_snapshot(const automaton& a);

    /// The graph shared by every weighting of this snapshot.
    [[nodiscard]] const automaton_graph& graph() const noexcept { return graph_; }
    /// The transition of the automaton that an edge comes from.
    [[nodiscard]] automaton_edge origin(edge_id e) const;

    /// A weighting with a cost per transition, its initial and final costs being 0.
    /// \throws std::invalid_argument if a cost is not finite
    [[nodiscard]] weighted_automaton weighted(const edge_cost& cost) const;
    /// A weighting with an initial cost and a final cost per accepting state.
    [[nodiscard]] weighted_automaton weighted(const edge_cost& cost, double initial_cost,
                                              const std::function<double(state_id)>& final_cost) const;
    /// A weighting by vectors, one cost function per dimension, its initial and final vectors being 0.
    /// \throws std::invalid_argument without dimension, or if a cost is not finite
    [[nodiscard]] multi_weighted_automaton multi_weighted(std::span<const edge_cost> dimensions) const;
    [[nodiscard]] multi_weighted_automaton multi_weighted(std::initializer_list<edge_cost> dimensions) const {
        return multi_weighted(std::span(dimensions.begin(), dimensions.size()));
    }

    /// Whether the captured automaton accepts a word.
    [[nodiscard]] bool run(std::span<const symbol> w) const { return graph_.run(w); }
    [[nodiscard]] bool run(std::initializer_list<symbol> w) const { return graph_.run(w); }
    /// A new automaton recognizing the captured language.
    [[nodiscard]] automaton to_automaton() const { return graph_.to_automaton(); }

private:
    struct origin_data {
        std::uint32_t index;
        bool epsilon;
    };
    automaton_graph graph_;
    std::vector<origin_data> origins_;
};

}  // namespace autolib
