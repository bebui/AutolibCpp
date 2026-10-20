// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Unfoldings of a graph by the number of symbols read, for the questions about a length or a word.

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "autolib/export.hpp"
#include "autolib/weighted_automaton.hpp"

namespace autolib {

class multi_weighted_automaton;
class multi_weighted_path;

/// The unfolding of a graph by positions : its states are the pairs (state, number of symbols read), up to a
/// horizon. Only the pairs reachable from the initial one and reaching an accepting one are built. Epsilon
/// edges stay within a layer, so an unfolding is not necessarily acyclic.
///
/// The structure is built once and evaluated with any weighting of the base graph, or with costs depending on
/// the position : `cost_of_length`, `best_path_of_length` and the best path of a word rely on it.
///
/// ```cpp
/// auto layers = layered_automaton::exactly(a.graph(), 10);
/// auto best = layers.best_path(a);                       // a best accepting path reading 10 symbols
/// auto later = layers.evaluate(a, [](int position, edge_id e) { return position * 0.5 + e; });
/// ```
class AUTOLIB_EXPORT layered_automaton {
public:
    /// A cost depending on the position (the number of symbols read before the edge) and the edge of the base.
    using position_weights = std::function<double(int position, edge_id e)>;
    class analysis;

    /// The unfolding accepting at the horizon only.
    /// \throws std::invalid_argument if the horizon is negative
    /// \throws std::length_error if the unfolding has more than 2^31 - 1 pairs
    [[nodiscard]] static layered_automaton exactly(const automaton_graph& graph, int horizon);
    /// The unfolding accepting at every layer.
    [[nodiscard]] static layered_automaton at_most(const automaton_graph& graph, int horizon);
    /// The unfolding reading exactly a word.
    [[nodiscard]] static layered_automaton for_word(const automaton_graph& graph, std::span<const symbol> w);

    [[nodiscard]] const automaton_graph& base_graph() const noexcept;
    /// The unfolded graph.
    [[nodiscard]] const automaton_graph& graph() const noexcept;
    [[nodiscard]] int horizon() const noexcept;
    /// The state of the base of an unfolded state.
    [[nodiscard]] state_id original_state(state_id s) const;
    /// The number of symbols read to reach an unfolded state.
    [[nodiscard]] int position(state_id s) const;
    /// The edge of the base of an unfolded edge.
    [[nodiscard]] edge_id original_edge(edge_id e) const;

    /// The weighting of the unfolding given by a weighting of the base graph.
    /// \throws std::invalid_argument if the weighting is not one of the base graph
    [[nodiscard]] weighted_automaton evaluate(const weighted_automaton& a) const;
    /// The weighting of the unfolding given by costs depending on the position, with the initial and final
    /// costs of `a`.
    [[nodiscard]] weighted_automaton evaluate(const weighted_automaton& a, const position_weights& weights) const;
    /// The vector weighting of the unfolding given by a vector weighting of the base graph.
    [[nodiscard]] multi_weighted_automaton evaluate(const multi_weighted_automaton& a) const;

    /// A best accepting path at the horizon, as a path of the base graph.
    /// \throws std::logic_error if the optimum is unbounded
    [[nodiscard]] std::optional<weighted_path> best_path(const weighted_automaton& a) const;
    /// The path of the base graph of a path of the unfolding.
    [[nodiscard]] weighted_path map(const weighted_path& path) const;
    [[nodiscard]] multi_weighted_path map(const multi_weighted_path& path) const;

    /// Distances of the unfolding for a weighting of the base graph.
    [[nodiscard]] analysis analyze(const weighted_automaton& a) const;
    /// Distances of the unfolding for costs depending on the position.
    [[nodiscard]] analysis analyze(const weighted_automaton& a, const position_weights& weights) const;

private:
    struct structure;
    explicit layered_automaton(std::shared_ptr<const structure> d) : data_(std::move(d)) {}
    static layered_automaton build(const automaton_graph& base, int horizon, bool at_most, const symbol* w);
    void check(const automaton_graph& g) const;

    std::shared_ptr<const structure> data_;
};

/// The distances of one weighting of an unfolding : best cost at the horizon, best path, and best costs
/// conditioned on the symbol read at a position.
class AUTOLIB_EXPORT layered_automaton::analysis {
public:
    [[nodiscard]] std::vector<double> forward_distances() const { return forward_.distances(); }
    /// The distances to acceptance, final costs included.
    [[nodiscard]] std::span<const double> backward_distances() const noexcept { return backward_; }
    [[nodiscard]] double optimal_cost() const { return forward_.optimal_cost(); }
    /// A best path, as a path of the base graph.
    [[nodiscard]] std::optional<weighted_path> best_path() const;
    /// The best cost of the accepting paths reading a symbol at a position.
    /// \throws std::out_of_range unless 0 <= position < horizon
    [[nodiscard]] double cost_at(int position, symbol c) const;

private:
    friend class layered_automaton;
    analysis(layered_automaton layers, weighted_automaton evaluated);

    layered_automaton layers_;
    weighted_automaton evaluated_;
    shortest_path_result forward_;
    std::vector<double> backward_;
};

}  // namespace autolib
