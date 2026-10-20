// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Immutable labelled multigraphs, shared by the weighted automata.
///
/// The weighted part of the library follows these error conventions :
/// - `std::invalid_argument` : an input outside the supported family (a cycle where a DAG is required, a
///   negative cycle that cannot be encoded, an infinite or NaN weight, a wrong number of weights);
/// - `std::out_of_range` : a state, edge, dimension or position that does not exist;
/// - `std::logic_error` : a question without an answer, such as the best path of an unbounded optimum;
/// - `std::length_error` : a construction exceeding its explicit budget of states;
/// - `std::overflow_error` : a finite sum of costs exceeding the doubles;
/// - `std::range_error` : an exact result that is not representable as a double.

#pragma once

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <vector>

#include "autolib/automaton.hpp"
#include "autolib/export.hpp"
#include "autolib/interval_set.hpp"

namespace autolib {

/// Identifier of an edge of a graph : its rank of insertion.
using edge_id = std::int32_t;

namespace detail {
struct graph_data;
struct weighted_access;
}  // namespace detail

/// An immutable multigraph whose edges read a set of symbols, or nothing (epsilon edges). It is the structure of
/// weighted automata : scalar and vector weightings of the same graph share it without copying.
///
/// States and edges are numbered from 0 in their order of creation. Parallel edges are never merged : each one
/// keeps its own cost in a weighting. Labels, incoming and outgoing edges and a topological order are computed
/// once, when the graph is built.
///
/// Copying a graph copies a handle : copies are the same graph, as `same_as` tells.
class AUTOLIB_EXPORT automaton_graph {
public:
    class builder;

    /// The graph without any state.
    automaton_graph();

    /// The graph of an automaton, without determinizing it : every transition becomes an edge reading its
    /// symbols, then every epsilon transition an epsilon edge, state after state.
    [[nodiscard]] static automaton_graph from(const automaton& a);

    [[nodiscard]] std::size_t state_count() const noexcept;
    [[nodiscard]] std::size_t edge_count() const noexcept;
    /// The initial state, or `no_state`.
    [[nodiscard]] state_id initial() const noexcept;
    [[nodiscard]] bool is_accept(state_id s) const;
    [[nodiscard]] state_id source(edge_id e) const;
    [[nodiscard]] state_id target(edge_id e) const;
    [[nodiscard]] bool is_epsilon(edge_id e) const;
    /// The symbols read by an edge; empty for an epsilon edge.
    [[nodiscard]] interval_set labels(edge_id e) const;
    /// Whether an edge reads a symbol.
    [[nodiscard]] bool accepts(edge_id e, symbol c) const;
    /// The edges leaving a state, by increasing number.
    [[nodiscard]] std::span<const edge_id> outgoing(state_id s) const;
    /// The edges entering a state, by increasing number.
    [[nodiscard]] std::span<const edge_id> incoming(state_id s) const;

    /// Whether the graph, epsilon edges included, has no cycle.
    [[nodiscard]] bool is_acyclic() const noexcept;
    /// The states in a topological order.
    /// \throws std::invalid_argument if the graph has a cycle
    [[nodiscard]] std::span<const state_id> topological_order() const;
    /// Whether no state has an epsilon edge or two outgoing edges reading a common symbol.
    [[nodiscard]] bool is_deterministic() const noexcept;
    /// Whether each state is both reachable from the initial state and able to reach an accepting state.
    [[nodiscard]] std::vector<bool> useful_states() const;
    /// Whether each state can reach an accepting state, reachable from the initial state or not.
    [[nodiscard]] std::vector<bool> coaccessible_states() const;

    /// Whether an accepting path reads a word : the language of the graph, independent of any weighting.
    [[nodiscard]] bool run(std::span<const symbol> w) const;
    [[nodiscard]] bool run(std::initializer_list<symbol> w) const { return run(std::span(w.begin(), w.size())); }
    /// A new automaton with the same states and language. Parallel edges merge : the distinct paths, and the
    /// costs of any weighting, are forgotten.
    [[nodiscard]] automaton to_automaton() const;

    /// Whether two handles designate the same graph : weightings require it to be combined.
    [[nodiscard]] bool same_as(const automaton_graph& other) const noexcept { return data_ == other.data_; }

private:
    friend struct detail::weighted_access;
    explicit automaton_graph(std::shared_ptr<const detail::graph_data> data) : data_(std::move(data)) {}

    std::shared_ptr<const detail::graph_data> data_;
};

/// Builds graphs. Each addition of a transition creates a distinct edge, and building takes a snapshot : the
/// builder can go on afterwards without changing the graphs already built.
class AUTOLIB_EXPORT automaton_graph::builder {
public:
    builder() = default;

    /// Adds a state and returns its number.
    state_id add_state(bool accept = false);
    void set_initial(state_id s);
    void set_accept(state_id s, bool accept = true);
    /// Adds an edge reading a nonempty set of symbols and returns its number.
    /// \throws std::invalid_argument if the set is empty
    edge_id add_transition(state_id from, state_id to, const interval_set& labels);
    edge_id add_transition(state_id from, state_id to, symbol c) { return add_transition(from, to, interval_set(c)); }
    /// Adds an epsilon edge and returns its number.
    edge_id add_epsilon(state_id from, state_id to);

    [[nodiscard]] std::size_t state_count() const noexcept { return accept_.size(); }
    [[nodiscard]] std::size_t edge_count() const noexcept { return source_.size(); }
    /// The graph built so far.
    [[nodiscard]] automaton_graph build() const;

private:
    void check(state_id s) const;

    state_id initial_ = no_state;
    std::vector<char> accept_;
    std::vector<state_id> source_, target_;
    std::vector<interval_set> labels_;
    std::vector<char> epsilon_;
};

}  // namespace autolib
