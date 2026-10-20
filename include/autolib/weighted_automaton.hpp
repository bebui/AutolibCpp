// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Weighted automata : additive costs on the edges of a graph, the cost of a word being the least cost of its
/// accepting paths.

#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "autolib/dyadic.hpp"
#include "autolib/export.hpp"
#include "autolib/weighted_graph.hpp"

namespace autolib {

class compiled_weighted_dfa;
class weighted_automaton;

/// An accepting path, or a path between two states : its edges, the symbols it reads and its cost.
class AUTOLIB_EXPORT weighted_path {
public:
    /// A path of `graph` from `start` following `edges` and reading `w`.
    /// \throws std::invalid_argument if the edges are not consecutive or do not read the word
    weighted_path(automaton_graph graph, state_id start, std::vector<edge_id> edges, word w, double cost);

    [[nodiscard]] const automaton_graph& graph() const noexcept { return graph_; }
    /// The first state, not necessarily the initial state of the graph.
    [[nodiscard]] state_id start() const noexcept { return start_; }
    /// The last state.
    [[nodiscard]] state_id end() const noexcept;
    /// The edges, epsilon edges included.
    [[nodiscard]] std::span<const edge_id> edges() const noexcept { return edges_; }
    [[nodiscard]] std::size_t edge_count() const noexcept { return edges_.size(); }
    /// The symbols read. An edge reading several symbols contributes the one chosen by the query, by default
    /// its least symbol.
    [[nodiscard]] const word& symbols() const noexcept { return word_; }
    /// The number of symbols read.
    [[nodiscard]] std::size_t length() const noexcept { return word_.size(); }
    /// The cost, initial and final costs included when the query includes them.
    [[nodiscard]] double cost() const noexcept { return cost_; }

private:
    automaton_graph graph_;
    state_id start_;
    std::vector<edge_id> edges_;
    word word_;
    double cost_;
};

/// A word and its cost : the least cost of its accepting paths.
struct weighted_word {
    word symbols;
    double cost;

    [[nodiscard]] std::size_t length() const noexcept { return symbols.size(); }
};

/// Shortest distances from a state, with the predecessors to rebuild the paths when they were requested.
class AUTOLIB_EXPORT shortest_path_result {
public:
    /// Whether an optimum exists.
    enum class status {
        finite,       ///< a finite optimum exists
        unreachable,  ///< no path exists
        unbounded     ///< the infimum is minus infinity : negative cycles, and no best path
    };

    /// The distance to a state : +infinity if unreachable, -infinity if unbounded.
    [[nodiscard]] double distance_to(state_id s) const;
    /// The distances to every state.
    [[nodiscard]] std::vector<double> distances() const;
    [[nodiscard]] status status_to(state_id s) const;
    /// A best path to a state, whose symbols are the least of each edge.
    /// \throws std::logic_error if the optimum is unbounded or the predecessors were not requested
    [[nodiscard]] std::optional<weighted_path> path_to(state_id s) const;
    /// The best cost of the accepting paths, final costs included : +infinity if none, -infinity if unbounded.
    [[nodiscard]] double optimal_cost() const;
    /// A best accepting path, final cost included.
    /// \throws std::logic_error if the optimum is unbounded or the predecessors were not requested
    [[nodiscard]] std::optional<weighted_path> best_path() const;

private:
    friend struct detail::weighted_access;
    shortest_path_result(const weighted_automaton& a, std::vector<double> distances, std::vector<edge_id> parent,
                         state_id start, bool longest);
    [[nodiscard]] state_id best_final() const;

    std::shared_ptr<const weighted_automaton> automaton_;
    std::vector<double> distance_;
    std::vector<edge_id> parent_;  ///< empty when the predecessors were not requested
    state_id start_;
    bool longest_;
};

/// A lazy sequence of results computed one at a time : an input range, which can be abandoned at any time.
template <class T>
class lazy_sequence {
public:
    /// The next result, if any.
    using generator = std::function<std::optional<T>()>;

    class iterator {
    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::input_iterator_tag;

        iterator() = default;
        [[nodiscard]] const T& operator*() const noexcept { return *owner_->current_; }
        [[nodiscard]] const T* operator->() const noexcept { return &*owner_->current_; }
        iterator& operator++() {
            owner_->advance();
            return *this;
        }
        void operator++(int) { owner_->advance(); }
        [[nodiscard]] friend bool operator==(const iterator& it, std::default_sentinel_t) noexcept {
            return it.owner_ == nullptr || !it.owner_->current_;
        }

    private:
        friend class lazy_sequence;
        explicit iterator(lazy_sequence* owner) : owner_(owner) {}
        lazy_sequence* owner_ = nullptr;
    };

    explicit lazy_sequence(generator next) : next_(std::move(next)) {}

    /// The next result, if any.
    [[nodiscard]] std::optional<T> next() {
        if (started_ && current_) {
            started_ = false;
            return std::exchange(current_, std::nullopt);
        }
        started_ = false;
        return next_();
    }
    /// Starts reading the results from the next one.
    [[nodiscard]] iterator begin() {
        if (!started_) {
            current_ = next_();
            started_ = true;
        }
        return iterator(this);
    }
    [[nodiscard]] std::default_sentinel_t end() const noexcept { return {}; }

private:
    void advance() { current_ = next_(); }

    generator next_;
    std::optional<T> current_;
    bool started_ = false;
};

/// Distances in both directions of one weighting, for the costs of the accepting paths through a state or an
/// edge in constant time.
class AUTOLIB_EXPORT path_analysis {
public:
    explicit path_analysis(const weighted_automaton& a);
    [[nodiscard]] const shortest_path_result& forward() const noexcept { return forward_; }
    /// The distances from every state to acceptance, final costs included.
    [[nodiscard]] std::span<const double> backward() const noexcept { return backward_; }
    /// The best cost of the accepting paths through a state.
    [[nodiscard]] double cost_through_state(state_id s) const;
    /// The best cost of the accepting paths through an edge.
    [[nodiscard]] double cost_through_edge(edge_id e) const;

private:
    std::shared_ptr<const weighted_automaton> automaton_;
    shortest_path_result forward_;
    std::vector<double> backward_;
};

/// A weighted automaton : additive costs on the edges of an immutable graph, plus an initial cost and final
/// costs. The cost of a word is the least cost of its accepting paths, +infinity when there is none, and
/// -infinity when a negative cycle of epsilon edges makes it unbounded.
///
/// ```cpp
/// weighted_automaton::builder b;
/// auto s = b.add_state(), t = b.add_state();
/// b.set_initial(s);
/// b.set_accept(t, 0.5);                      // final cost
/// b.add_transition(s, t, 1, 2.0);            // reads 1 for 2.0
/// b.add_transition(s, t, interval_set::range(0, 9), 3.0);
/// auto a = b.build();
/// double c = a({1});                          // 2.5 : the best of the two paths
/// auto best = a.k_best_words(3);              // the 3 cheapest words
/// ```
///
/// Weights are finite doubles, and sums overflowing the doubles throw `std::overflow_error`. Costs are summed
/// in doubles, except by the algorithms comparing sums exactly (Bellman-Ford beyond the integers, the
/// transformations, the enumerations by cost), which use dyadics and round each result once.
///
/// Weighted automata are immutable values sharing their graph and weights : copies are cheap.
class AUTOLIB_EXPORT weighted_automaton {
public:
    class builder;
    class cost_evaluator;

    /// Default bound on the number of states built by determinize().
    static constexpr std::size_t default_max_states = 10'000;

    /// The automaton without any state, recognizing nothing.
    weighted_automaton();
    /// A weighting of a graph, the weights of the edges being given in the order of their numbers. The final
    /// costs, one per state, default to 0 and are ignored for the non-accepting states.
    /// \throws std::invalid_argument if a weight is not finite or the numbers of weights are wrong
    [[nodiscard]] static weighted_automaton of(automaton_graph graph, std::vector<double> edge_weights,
                                               double initial_weight = 0, std::vector<double> final_weights = {});
    /// The weighting of the graph of an automaton, `weight(from, to, labels)` giving the cost of each edge; the
    /// labels of an epsilon edge are empty.
    [[nodiscard]] static weighted_automaton from(const automaton& a,
                                                 const std::function<double(state_id, state_id, const interval_set&)>& weight);

    [[nodiscard]] const automaton_graph& graph() const noexcept { return graph_; }
    [[nodiscard]] double weight(edge_id e) const;
    [[nodiscard]] std::span<const double> weights() const noexcept;
    [[nodiscard]] double initial_weight() const noexcept;
    /// The final cost of a state, used only when it is accepting.
    [[nodiscard]] double final_weight(state_id s) const;
    /// Another weighting of the same graph, keeping the initial and final costs.
    [[nodiscard]] weighted_automaton with_weights(std::vector<double> edge_weights) const;

    /// The cost of a word : +infinity if it is not accepted, -infinity if unbounded.
    [[nodiscard]] double cost(std::span<const symbol> w) const;
    [[nodiscard]] double cost(std::initializer_list<symbol> w) const { return cost(std::span(w.begin(), w.size())); }
    [[nodiscard]] double operator()(std::span<const symbol> w) const { return cost(w); }
    [[nodiscard]] double operator()(std::initializer_list<symbol> w) const { return cost(w); }
    /// An evaluator of costs reusing its buffers : for many words, from one thread.
    [[nodiscard]] cost_evaluator evaluator() const;
    /// The best cost of the paths starting by reading a prefix.
    [[nodiscard]] double optimal_cost_with_prefix(std::span<const symbol> prefix) const;

    /// The best cost of the accepting paths : +infinity if none, -infinity if unbounded.
    [[nodiscard]] double optimal_cost() const;
    /// A best accepting path, if any.
    /// \throws std::logic_error if the optimum is unbounded
    [[nodiscard]] std::optional<weighted_path> best_path() const;
    /// A best accepting path reading a word, if any, found in the product with the word.
    /// \throws std::logic_error if the cost of the word is unbounded
    [[nodiscard]] std::optional<weighted_path> best_path(std::span<const symbol> w) const;
    [[nodiscard]] std::optional<weighted_path> best_path(std::initializer_list<symbol> w) const {
        return best_path(std::span(w.begin(), w.size()));
    }
    /// The best cost of the accepting words of a length.
    [[nodiscard]] double cost_of_length(int length) const;
    /// The best cost of the accepting words of at most a length.
    [[nodiscard]] double cost_of_length_at_most(int length) const;
    /// A best accepting path reading a word of a length, if any.
    [[nodiscard]] std::optional<weighted_path> best_path_of_length(int length) const;
    /// The word of a best accepting path of a length, if any.
    [[nodiscard]] std::optional<word> best_word_of_length(int length) const;

    // Operations preserving the costs of the words, without changing the operands
    /// The least of the costs of both automata.
    [[nodiscard]] weighted_automaton union_with(const weighted_automaton& other) const;
    /// The least cost over the splits of a word.
    [[nodiscard]] weighted_automaton concatenate(const weighted_automaton& other) const;
    /// The words of both automata, at the sum of their costs.
    [[nodiscard]] weighted_automaton intersect_with(const weighted_automaton& other) const;
    /// The costs of the mirror words; the sums may round differently.
    [[nodiscard]] weighted_automaton reverse() const;
    /// Kleene star; a negative cost of the empty word makes the costs unbounded.
    [[nodiscard]] weighted_automaton star() const;
    /// One or more repetitions.
    [[nodiscard]] weighted_automaton plus() const { return repeat(1, -1); }
    /// From `min` to `max` repetitions, `max < 0` meaning unbounded.
    [[nodiscard]] weighted_automaton repeat(int min, int max) const;
    /// The automaton restricted to its useful states, with all its accepting paths.
    [[nodiscard]] weighted_automaton trim() const;
    /// The trimmed automaton whose parallel edges of equal weight are merged : same costs, fewer paths.
    [[nodiscard]] weighted_automaton simplify() const;

    [[nodiscard]] friend weighted_automaton operator|(const weighted_automaton& a, const weighted_automaton& b) { return a.union_with(b); }
    [[nodiscard]] friend weighted_automaton operator&(const weighted_automaton& a, const weighted_automaton& b) { return a.intersect_with(b); }
    [[nodiscard]] friend weighted_automaton operator+(const weighted_automaton& a, const weighted_automaton& b) { return a.concatenate(b); }
    [[nodiscard]] friend weighted_automaton operator*(const weighted_automaton& a) { return a.star(); }
    [[nodiscard]] friend weighted_automaton operator+(const weighted_automaton& a) { return a.plus(); }

    // Transformations
    /// An equivalent automaton without epsilon edges.
    /// \throws std::invalid_argument if a negative cycle of epsilon edges is reachable
    [[nodiscard]] weighted_automaton remove_epsilon() const;
    /// An equivalent deterministic automaton, by residual subsets, computed exactly. A deterministic automaton
    /// is returned unchanged.
    /// \throws std::length_error if more than `max_states` states are needed (which does not prove that no
    ///         deterministic equivalent exists)
    /// \throws std::range_error if a weight of the result is not exactly a double
    [[nodiscard]] weighted_automaton determinize(std::size_t max_states = default_max_states) const;
    /// The minimal deterministic automaton with the same costs, the costs being pushed toward the initial state.
    /// \throws std::invalid_argument if the automaton is not deterministic
    /// \throws std::range_error if a weight of the result is not exactly a double
    [[nodiscard]] weighted_automaton minimize() const;
    /// A compiled evaluator of a deterministic automaton.
    /// \throws std::invalid_argument if the automaton is not deterministic
    [[nodiscard]] compiled_weighted_dfa compile() const;

    // Enumeration by cost
    /// The k cheapest accepting paths, by increasing cost; distinct paths may read the same word.
    /// \throws std::invalid_argument if a useful negative cycle exists
    [[nodiscard]] std::vector<weighted_path> k_best_paths(std::size_t k) const;
    /// The accepting paths by increasing cost, lazily. Cycles of cost zero give infinitely many paths of the
    /// same cost, by increasing number of edges.
    /// \throws std::invalid_argument if a useful negative cycle exists
    [[nodiscard]] lazy_sequence<weighted_path> paths_by_cost() const;
    /// The k cheapest distinct words, by increasing cost.
    /// \throws std::invalid_argument if a useful negative cycle exists
    [[nodiscard]] std::vector<weighted_word> k_best_words(std::size_t k) const;
    /// The distinct words by increasing cost, lazily, without determinizing the automaton.
    /// \throws std::invalid_argument if a useful negative cycle exists
    [[nodiscard]] lazy_sequence<weighted_word> words_by_cost() const;

private:
    friend struct detail::weighted_access;
    struct weights_data;
    weighted_automaton(automaton_graph graph, std::shared_ptr<const weights_data> weights);
    [[nodiscard]] double deterministic_cost(std::span<const symbol> w) const;

    automaton_graph graph_;
    std::shared_ptr<const weights_data> weights_;
};

/// Builds weighted automata : the graph and its costs together.
class AUTOLIB_EXPORT weighted_automaton::builder {
public:
    builder() = default;

    state_id add_state();
    /// Sets the initial state and the initial cost.
    void set_initial(state_id s, double cost = 0);
    /// Makes a state accepting, with a final cost.
    void set_accept(state_id s, double cost = 0);
    edge_id add_transition(state_id from, state_id to, const interval_set& labels, double cost);
    edge_id add_transition(state_id from, state_id to, symbol c, double cost) { return add_transition(from, to, interval_set(c), cost); }
    edge_id add_epsilon(state_id from, state_id to, double cost);

    [[nodiscard]] weighted_automaton build() const;

private:
    automaton_graph::builder graph_;
    std::vector<double> edges_, finals_;
    double initial_ = 0;
};

/// Evaluates the costs of words, reusing its buffers. Not to be shared between threads.
class AUTOLIB_EXPORT weighted_automaton::cost_evaluator {
public:
    explicit cost_evaluator(const weighted_automaton& a);
    cost_evaluator(cost_evaluator&&) noexcept;
    cost_evaluator& operator=(cost_evaluator&&) noexcept;
    ~cost_evaluator();

    [[nodiscard]] double cost(std::span<const symbol> w);
    [[nodiscard]] double cost(std::initializer_list<symbol> w) { return cost(std::span(w.begin(), w.size())); }
    [[nodiscard]] double optimal_cost_with_prefix(std::span<const symbol> prefix);

private:
    struct state;
    std::unique_ptr<state> state_;
};

// Shortest paths : without negative edge, Dijkstra; on a DAG, one pass in topological order; otherwise
// Bellman-Ford, in doubles on bounded integers and exactly otherwise.

/// Distances from the initial state, initial cost included, by the best suited algorithm.
[[nodiscard]] AUTOLIB_EXPORT shortest_path_result shortest_paths(const weighted_automaton& a, bool reconstruct = true);
/// Distances from a state, from 0.
[[nodiscard]] AUTOLIB_EXPORT shortest_path_result shortest_paths_from(const weighted_automaton& a, state_id source);
/// Shortest distances on a DAG, in O(V + E).
/// \throws std::invalid_argument if the graph has a cycle
[[nodiscard]] AUTOLIB_EXPORT shortest_path_result dag_shortest_paths(const weighted_automaton& a);
/// Longest distances on a DAG, in O(V + E).
/// \throws std::invalid_argument if the graph has a cycle
[[nodiscard]] AUTOLIB_EXPORT shortest_path_result dag_longest_paths(const weighted_automaton& a);
/// Dijkstra's algorithm, in O((V + E) log V).
/// \throws std::invalid_argument if an edge is negative
[[nodiscard]] AUTOLIB_EXPORT shortest_path_result dijkstra(const weighted_automaton& a);
/// Bellman-Ford's algorithm, the states reachable from a negative cycle being at -infinity.
[[nodiscard]] AUTOLIB_EXPORT shortest_path_result bellman_ford(const weighted_automaton& a);
/// The distances from every state to acceptance, final costs included, initial cost excluded.
[[nodiscard]] AUTOLIB_EXPORT std::vector<double> backward_distances(const weighted_automaton& a);
/// The distances from every state to a state.
[[nodiscard]] AUTOLIB_EXPORT std::vector<double> distances_to(const weighted_automaton& a, state_id destination);
/// The best cost of the accepting paths through a state.
[[nodiscard]] AUTOLIB_EXPORT double cost_through_state(const weighted_automaton& a, state_id s);
/// The best cost of the accepting paths through an edge.
[[nodiscard]] AUTOLIB_EXPORT double cost_through_edge(const weighted_automaton& a, edge_id e);

/// Determinization restricted to DAGs with bounded integer weights, where doubles are exact.
/// \throws std::invalid_argument outside that family
[[nodiscard]] AUTOLIB_EXPORT weighted_automaton determinize_dag(const weighted_automaton& a,
                                                                std::size_t max_states = static_cast<std::size_t>(-1));
/// Minimization restricted to deterministic DAGs with bounded integer weights.
/// \throws std::invalid_argument outside that family
[[nodiscard]] AUTOLIB_EXPORT weighted_automaton minimize_dag(const weighted_automaton& a);
/// The k cheapest accepting paths reading words of a length.
[[nodiscard]] AUTOLIB_EXPORT std::vector<weighted_path> k_best_paths_of_length(const weighted_automaton& a, int length, std::size_t k);
/// The k cheapest distinct words of a length.
[[nodiscard]] AUTOLIB_EXPORT std::vector<weighted_word> k_best_words_of_length(const weighted_automaton& a, int length, std::size_t k);

}  // namespace autolib
