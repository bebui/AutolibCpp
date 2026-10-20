// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Compiled evaluators of deterministic weighted automata.

#pragma once

#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "autolib/export.hpp"
#include "autolib/multi_weighted_automaton.hpp"
#include "autolib/weighted_automaton.hpp"

namespace autolib {

/// The transitions of a deterministic graph sorted by symbols, independently of any weighting : the weightings
/// of the same graph share it.
class AUTOLIB_EXPORT weighted_dfa_index {
public:
    /// \throws std::invalid_argument if the graph is not deterministic
    explicit weighted_dfa_index(const automaton_graph& graph);

    [[nodiscard]] const automaton_graph& graph() const noexcept { return graph_; }
    /// The edge reading a symbol from a state, or -1; `no_state` has no edge.
    [[nodiscard]] edge_id transition(state_id s, symbol c) const noexcept;

private:
    struct data;
    automaton_graph graph_;
    std::shared_ptr<const data> data_;
};

/// The evaluator of a deterministic weighted automaton : the cost of a word in time proportional to its length
/// times the logarithm of the number of transitions per state.
class AUTOLIB_EXPORT compiled_weighted_dfa {
public:
    /// \throws std::invalid_argument if the automaton is not deterministic
    explicit compiled_weighted_dfa(const weighted_automaton& a);
    /// An evaluator reusing the index of the same graph.
    /// \throws std::invalid_argument if the graphs differ
    compiled_weighted_dfa(const weighted_automaton& a, weighted_dfa_index index);

    [[nodiscard]] const weighted_dfa_index& index() const noexcept { return index_; }
    [[nodiscard]] const weighted_automaton& automaton() const noexcept { return automaton_; }
    /// The evaluator of another weighting of the same graph, sharing the index.
    [[nodiscard]] compiled_weighted_dfa reweight(const weighted_automaton& a) const { return {a, index_}; }
    [[nodiscard]] edge_id transition(state_id s, symbol c) const noexcept { return index_.transition(s, c); }
    /// The cost of a word, initial and final costs included : +infinity if it is not accepted.
    [[nodiscard]] double cost(std::span<const symbol> w) const;
    [[nodiscard]] double cost(std::initializer_list<symbol> w) const { return cost(std::span(w.begin(), w.size())); }
    [[nodiscard]] double operator()(std::span<const symbol> w) const { return cost(w); }
    [[nodiscard]] double operator()(std::initializer_list<symbol> w) const { return cost(w); }

private:
    weighted_automaton automaton_;
    weighted_dfa_index index_;
};

/// The evaluator of a deterministic automaton weighted by vectors, writing in a buffer without allocating.
class AUTOLIB_EXPORT compiled_multi_weighted_dfa {
public:
    /// \throws std::invalid_argument if the graph is not deterministic
    explicit compiled_multi_weighted_dfa(const multi_weighted_automaton& a);
    compiled_multi_weighted_dfa(const multi_weighted_automaton& a, weighted_dfa_index index);

    [[nodiscard]] const weighted_dfa_index& index() const noexcept { return index_; }
    [[nodiscard]] const automaton_graph& graph() const noexcept { return automaton_.graph(); }
    [[nodiscard]] std::size_t dimensions() const noexcept { return automaton_.dimensions(); }
    [[nodiscard]] edge_id transition(state_id s, symbol c) const noexcept { return index_.transition(s, c); }
    /// The evaluator of another vector weighting of the same graph, possibly of another dimension.
    [[nodiscard]] compiled_multi_weighted_dfa reweight(const multi_weighted_automaton& a) const { return {a, index_}; }
    /// The evaluator of one dimension, sharing the index.
    [[nodiscard]] compiled_weighted_dfa project(std::size_t dimension) const;
    /// The evaluator of a linear combination of the dimensions, sharing the index.
    [[nodiscard]] compiled_weighted_dfa scalarize(std::span<const double> coefficients) const;
    [[nodiscard]] compiled_weighted_dfa scalarize(std::initializer_list<double> coefficients) const {
        return scalarize(std::span(coefficients.begin(), coefficients.size()));
    }

    /// The cost vector of a word, if it is accepted.
    [[nodiscard]] std::optional<std::vector<double>> costs(std::span<const symbol> w) const;
    [[nodiscard]] std::optional<std::vector<double>> costs(std::initializer_list<symbol> w) const {
        return costs(std::span(w.begin(), w.size()));
    }
    /// Writes the cost vector of a word in `output`, of `dimensions()` entries, without allocating. Returns
    /// whether the word is accepted; if not, `output` is filled with +infinity.
    /// \throws std::invalid_argument if `output` has a wrong size
    bool costs(std::span<const symbol> w, std::span<double> output) const;

private:
    multi_weighted_automaton automaton_;
    weighted_dfa_index index_;
};

}  // namespace autolib
