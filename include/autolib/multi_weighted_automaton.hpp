// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Automata weighted by vectors : several additive costs at once, optimized lexicographically or as a Pareto
/// front.

#pragma once

#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "autolib/dyadic.hpp"
#include "autolib/export.hpp"
#include "autolib/weighted_automaton.hpp"

namespace autolib {

class compiled_multi_weighted_dfa;

/// An accepting path and the vector of its costs.
class AUTOLIB_EXPORT multi_weighted_path {
public:
    multi_weighted_path(weighted_path path, std::vector<double> costs, std::vector<dyadic> exact = {});

    [[nodiscard]] const automaton_graph& graph() const noexcept { return path_.graph(); }
    [[nodiscard]] state_id start() const noexcept { return path_.start(); }
    [[nodiscard]] state_id end() const noexcept { return path_.end(); }
    [[nodiscard]] std::span<const edge_id> edges() const noexcept { return path_.edges(); }
    [[nodiscard]] const word& symbols() const noexcept { return path_.symbols(); }
    [[nodiscard]] std::size_t length() const noexcept { return path_.length(); }
    /// The costs, initial and final costs included.
    [[nodiscard]] std::span<const double> costs() const noexcept { return costs_; }
    [[nodiscard]] double cost(std::size_t dimension) const { return costs_.at(dimension); }
    /// The exact sum of the costs of a dimension, which the double cost rounds.
    [[nodiscard]] dyadic exact_cost(std::size_t dimension) const;
    /// The exact sums, when the solver needed them; empty otherwise.
    [[nodiscard]] std::span<const dyadic> exact_costs() const noexcept { return exact_; }

private:
    weighted_path path_;
    std::vector<double> costs_;
    std::vector<dyadic> exact_;
};

/// The non-dominated accepting cost vectors, each with a path; their number may be exponential.
class AUTOLIB_EXPORT pareto_result {
public:
    pareto_result(std::vector<multi_weighted_path> paths, bool all_paths) : paths_(std::move(paths)), all_paths_(all_paths) {}

    /// One path per non-dominated vector, or every path when all were requested. On a temporary result, the
    /// paths are moved out, so that `for (auto& p : a.pareto_front().paths())` is safe.
    [[nodiscard]] std::span<const multi_weighted_path> paths() const& noexcept { return paths_; }
    [[nodiscard]] std::vector<multi_weighted_path> paths() && noexcept { return std::move(paths_); }
    [[nodiscard]] std::size_t size() const noexcept { return paths_.size(); }
    /// Whether the paths of equal vectors were all kept.
    [[nodiscard]] bool includes_all_paths() const noexcept { return all_paths_; }
    /// Always true : the front is never truncated nor approximated.
    [[nodiscard]] bool is_complete() const noexcept { return true; }

private:
    std::vector<multi_weighted_path> paths_;
    bool all_paths_;
};

/// Whether a vector dominates another for minimization : smaller or equal everywhere, and smaller somewhere.
/// \throws std::invalid_argument if the dimensions differ or are zero, or a value is not finite
[[nodiscard]] AUTOLIB_EXPORT bool dominates(std::span<const double> x, std::span<const double> y);

/// An automaton weighted by vectors of a fixed dimension. An ambiguous word has a set of cost vectors, one per
/// accepting path, and not their componentwise minimum : the operations keep every vector.
///
/// ```cpp
/// multi_weighted_automaton::builder b(2);               // time and money
/// auto s = b.add_state(), t = b.add_state();
/// b.set_initial(s);
/// b.set_accept(t);
/// b.add_transition(s, t, 7, {10, 1});                   // slow and cheap
/// b.add_transition(s, t, 7, {2, 5});                    // fast and expensive
/// auto a = b.build();
/// auto front = a.pareto_front();                        // both paths
/// auto fast = a.lexicographic_best_path({0, 1});        // time first
/// auto mix = a.scalarize({1, 2});                       // time + 2 money, a scalar automaton
/// ```
class AUTOLIB_EXPORT multi_weighted_automaton {
public:
    class builder;

    /// A weighting of a graph : the vectors of the edges one after the other, then the initial vector and the
    /// final vectors of the states one after the other, both zero by default.
    /// \throws std::invalid_argument if the dimension is zero, a weight is not finite or the counts are wrong
    [[nodiscard]] static multi_weighted_automaton of(automaton_graph graph, std::size_t dimensions, std::vector<double> edge_weights,
                                                     std::vector<double> initial = {}, std::vector<double> finals = {});
    /// The weighting of an automaton by vectors, one cost function per dimension : `a.multi_weighted(dimensions)`.
    [[nodiscard]] static multi_weighted_automaton from(const automaton& a, std::span<const edge_cost> dimensions);
    [[nodiscard]] static multi_weighted_automaton from(const automaton& a, std::initializer_list<edge_cost> dimensions) {
        return from(a, std::span(dimensions.begin(), dimensions.size()));
    }
    /// The scalar weightings of one graph as the dimensions of a vector : the costs of a path stay correlated.
    /// \throws std::invalid_argument if there is no weighting or their graphs differ
    [[nodiscard]] static multi_weighted_automaton from_weightings(std::span<const weighted_automaton> weightings);
    [[nodiscard]] static multi_weighted_automaton from_weightings(std::initializer_list<weighted_automaton> weightings) {
        return from_weightings(std::span(weightings.begin(), weightings.size()));
    }
    /// The synchronized product of scalar automata on their common words, one dimension per operand : every
    /// combination of their accepting paths is kept.
    [[nodiscard]] static multi_weighted_automaton synchronized_product(std::span<const weighted_automaton> automata);
    [[nodiscard]] static multi_weighted_automaton synchronized_product(std::initializer_list<weighted_automaton> automata) {
        return synchronized_product(std::span(automata.begin(), automata.size()));
    }

    [[nodiscard]] const automaton_graph& graph() const noexcept { return graph_; }
    [[nodiscard]] std::size_t dimensions() const noexcept;
    /// Whether an accepting path reads a word, whatever its costs.
    [[nodiscard]] bool run(std::span<const symbol> w) const { return graph_.run(w); }
    [[nodiscard]] bool run(std::initializer_list<symbol> w) const { return graph_.run(w); }
    /// A new automaton recognizing the same words, without the costs nor the distinct parallel paths.
    [[nodiscard]] automaton to_automaton() const { return graph_.to_automaton(); }
    [[nodiscard]] double weight(edge_id e, std::size_t dimension) const;
    /// The vector of an edge.
    [[nodiscard]] std::span<const double> weights(edge_id e) const;
    [[nodiscard]] double initial_weight(std::size_t dimension) const;
    [[nodiscard]] std::span<const double> initial_weights() const noexcept;
    [[nodiscard]] double final_weight(state_id s, std::size_t dimension) const;
    [[nodiscard]] std::span<const double> final_weights(state_id s) const;

    /// The scalar automaton of one dimension.
    [[nodiscard]] weighted_automaton project(std::size_t dimension) const;
    /// The dimensions selected, in the given order.
    [[nodiscard]] multi_weighted_automaton select_dimensions(std::span<const std::size_t> selected) const;
    [[nodiscard]] multi_weighted_automaton select_dimensions(std::initializer_list<std::size_t> selected) const {
        return select_dimensions(std::span(selected.begin(), selected.size()));
    }
    /// The scalar automaton of a linear combination of the dimensions. No path is pruned; negative
    /// coefficients may make Bellman-Ford necessary.
    [[nodiscard]] weighted_automaton scalarize(std::span<const double> coefficients) const;
    [[nodiscard]] weighted_automaton scalarize(std::initializer_list<double> coefficients) const {
        return scalarize(std::span(coefficients.begin(), coefficients.size()));
    }
    /// The cost vector of an accepting path of this graph, summed exactly when doubles could round.
    [[nodiscard]] std::vector<double> path_costs(const weighted_path& path) const;

    /// The best accepting path for the lexicographic order of the dimensions in a priority order, or natural
    /// order if empty. The graph must be acyclic.
    [[nodiscard]] std::optional<multi_weighted_path> lexicographic_best_path(std::span<const std::size_t> priority = {}) const;
    [[nodiscard]] std::optional<multi_weighted_path> lexicographic_best_path(std::initializer_list<std::size_t> priority) const {
        return lexicographic_best_path(std::span(priority.begin(), priority.size()));
    }
    /// The exact Pareto front of the accepting paths, one path per vector unless all are requested. The graph
    /// must be acyclic.
    [[nodiscard]] pareto_result pareto_front(bool all_paths = false) const;
    /// The Pareto front of the paths reading a word.
    [[nodiscard]] pareto_result pareto_for_word(std::span<const symbol> w) const;
    [[nodiscard]] pareto_result pareto_for_word(std::initializer_list<symbol> w) const {
        return pareto_for_word(std::span(w.begin(), w.size()));
    }

    // Operations keeping every word and vector, for operands of the same dimension
    [[nodiscard]] multi_weighted_automaton union_with(const multi_weighted_automaton& other) const;
    [[nodiscard]] multi_weighted_automaton concatenate(const multi_weighted_automaton& other) const;
    [[nodiscard]] multi_weighted_automaton intersect_with(const multi_weighted_automaton& other) const;
    [[nodiscard]] multi_weighted_automaton reverse() const;
    [[nodiscard]] multi_weighted_automaton trim() const;
    /// The trimmed automaton whose parallel edges of equal vectors are merged.
    [[nodiscard]] multi_weighted_automaton simplify() const;
    /// The product on common words, the dimensions of `other` following those of this automaton.
    [[nodiscard]] multi_weighted_automaton product_independent(const multi_weighted_automaton& other) const;
    /// The concatenation, the dimensions of `other` following those of this automaton.
    [[nodiscard]] multi_weighted_automaton concatenate_independent(const multi_weighted_automaton& other) const;

    [[nodiscard]] friend multi_weighted_automaton operator|(const multi_weighted_automaton& a, const multi_weighted_automaton& b) { return a.union_with(b); }
    [[nodiscard]] friend multi_weighted_automaton operator&(const multi_weighted_automaton& a, const multi_weighted_automaton& b) { return a.intersect_with(b); }
    [[nodiscard]] friend multi_weighted_automaton operator+(const multi_weighted_automaton& a, const multi_weighted_automaton& b) { return a.concatenate(b); }

    /// A compiled evaluator of a deterministic automaton.
    /// \throws std::invalid_argument if the graph is not deterministic
    [[nodiscard]] compiled_multi_weighted_dfa compile() const;
    /// The Graphviz description, with the vectors of costs; the edges of `highlight` are drawn in colour.
    [[nodiscard]] std::string to_dot(std::span<const edge_id> highlight = {}) const;
    [[nodiscard]] std::string to_dot(const multi_weighted_path& highlight) const { return to_dot(highlight.edges()); }

private:
    friend struct detail::weighted_access;
    struct vectors;
    multi_weighted_automaton(automaton_graph graph, std::shared_ptr<const vectors> v) : graph_(std::move(graph)), vectors_(std::move(v)) {}

    automaton_graph graph_;
    std::shared_ptr<const vectors> vectors_;
};

/// Builds automata weighted by vectors.
class AUTOLIB_EXPORT multi_weighted_automaton::builder {
public:
    /// \throws std::invalid_argument if the dimension is zero
    explicit builder(std::size_t dimensions);

    state_id add_state();
    /// Sets the initial state and its vector, zero if empty.
    void set_initial(state_id s, std::span<const double> costs = {});
    void set_initial(state_id s, std::initializer_list<double> costs) { set_initial(s, std::span(costs.begin(), costs.size())); }
    /// Makes a state accepting, with a final vector, zero if empty.
    void set_accept(state_id s, std::span<const double> costs = {});
    void set_accept(state_id s, std::initializer_list<double> costs) { set_accept(s, std::span(costs.begin(), costs.size())); }
    edge_id add_transition(state_id from, state_id to, const interval_set& labels, std::span<const double> costs);
    edge_id add_transition(state_id from, state_id to, const interval_set& labels, std::initializer_list<double> costs) {
        return add_transition(from, to, labels, std::span(costs.begin(), costs.size()));
    }
    edge_id add_epsilon(state_id from, state_id to, std::span<const double> costs);
    edge_id add_epsilon(state_id from, state_id to, std::initializer_list<double> costs) {
        return add_epsilon(from, to, std::span(costs.begin(), costs.size()));
    }

    [[nodiscard]] multi_weighted_automaton build() const;

private:
    std::vector<double> checked(std::span<const double> costs) const;

    std::size_t dimensions_;
    automaton_graph::builder graph_;
    std::vector<double> edges_, initial_, finals_;
};

}  // namespace autolib
