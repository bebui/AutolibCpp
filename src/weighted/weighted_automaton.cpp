// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <algorithm>
#include <bit>
#include <optional>
#include <unordered_map>

#include "autolib/compiled_weighted_dfa.hpp"
#include "autolib/layered_automaton.hpp"
#include "weighted/paths.hpp"

namespace autolib {

using detail::add;
using detail::data;
using detail::inf;
using detail::u;
using detail::weights;

namespace {

/// Copies the states and edges of an automaton into a builder, with the final costs or not.
std::vector<state_id> copy(const weighted_automaton& a, weighted_automaton::builder& b, bool finals) {
    const auto& g = data(a);
    const auto& w = detail::weights(a);
    std::vector<state_id> map(g.states());
    for (std::size_t s = 0; s < map.size(); ++s) {
        map[s] = b.add_state();
        if (finals && g.accept[s])
            b.set_accept(map[s], w.finals[s]);
    }
    for (std::size_t e = 0; e < g.edges(); ++e) {
        auto from = map[u(g.source[e])], to = map[u(g.target[e])];
        if (g.epsilon[e])
            b.add_epsilon(from, to, w.edges[e]);
        else
            b.add_transition(from, to, g.labels(static_cast<edge_id>(e)), w.edges[e]);
    }
    return map;
}

}  // namespace

// Construction

weighted_automaton::weighted_automaton()
    : weighted_automaton(automaton_graph(), std::make_shared<const weights_data>()) {}

weighted_automaton::weighted_automaton(automaton_graph graph, std::shared_ptr<const weights_data> weights)
    : graph_(std::move(graph)), weights_(std::move(weights)) {}

weighted_automaton weighted_automaton::of(automaton_graph graph, std::vector<double> edge_weights, double initial_weight,
                                          std::vector<double> final_weights) {
    weights_data w;
    w.edges = detail::copy_weights(std::move(edge_weights), graph.edge_count());
    w.initial = detail::finite(initial_weight);
    w.finals = final_weights.empty() ? std::vector<double>(graph.state_count(), 0.0)
                                     : detail::copy_weights(std::move(final_weights), graph.state_count());
    return {std::move(graph), std::make_shared<const weights_data>(std::move(w))};
}

weighted_automaton weighted_automaton::from(const automaton& a,
                                            const std::function<double(state_id, state_id, const interval_set&)>& weight) {
    auto graph = automaton_graph::from(a);
    const auto& g = data(graph);
    std::vector<double> costs(g.edges());
    for (std::size_t e = 0; e < costs.size(); ++e)
        costs[e] = weight(g.source[e], g.target[e], g.labels(static_cast<edge_id>(e)));
    return of(std::move(graph), std::move(costs));
}

double weighted_automaton::weight(edge_id e) const {
    data(graph_).check_edge(e);
    return weights_->edges[u(e)];
}

std::span<const double> weighted_automaton::weights() const noexcept { return weights_->edges; }
double weighted_automaton::initial_weight() const noexcept { return weights_->initial; }

double weighted_automaton::final_weight(state_id s) const {
    data(graph_).check_state(s);
    return weights_->finals[u(s)];
}

weighted_automaton weighted_automaton::with_weights(std::vector<double> edge_weights) const {
    return of(graph_, std::move(edge_weights), weights_->initial, weights_->finals);
}

// Builder

state_id weighted_automaton::builder::add_state() {
    finals_.push_back(0);
    return graph_.add_state();
}

void weighted_automaton::builder::set_initial(state_id s, double cost) {
    double v = detail::finite(cost);
    graph_.set_initial(s);
    initial_ = v;
}

void weighted_automaton::builder::set_accept(state_id s, double cost) {
    double v = detail::finite(cost);
    graph_.set_accept(s);
    finals_[u(s)] = v;
}

edge_id weighted_automaton::builder::add_transition(state_id from, state_id to, const interval_set& labels, double cost) {
    double v = detail::finite(cost);
    edge_id e = graph_.add_transition(from, to, labels);
    edges_.push_back(v);
    return e;
}

edge_id weighted_automaton::builder::add_epsilon(state_id from, state_id to, double cost) {
    double v = detail::finite(cost);
    edge_id e = graph_.add_epsilon(from, to);
    edges_.push_back(v);
    return e;
}

weighted_automaton weighted_automaton::builder::build() const {
    return of(graph_.build(), edges_, initial_, finals_);
}

// Costs of words

double weighted_automaton::deterministic_cost(std::span<const symbol> w) const {
    const auto& g = data(graph_);
    state_id s = g.initial;
    if (s == no_state)
        return inf;
    double cost = weights_->initial;
    for (symbol c : w) {
        edge_id found = -1;
        for (edge_id e : g.out(s))
            if (g.accepts(e, c)) {
                found = e;
                break;
            }
        if (found < 0)
            return inf;
        cost = add(cost, weights_->edges[u(found)]);
        s = g.target[u(found)];
    }
    return g.accept[u(s)] ? add(cost, weights_->finals[u(s)]) : inf;
}

struct weighted_automaton::cost_evaluator::state {
    weighted_automaton a;
    std::vector<double> current, next;
    std::optional<detail::epsilon_workspace> closure;

    explicit state(const weighted_automaton& automaton)
        : a(automaton), current(automaton.graph().state_count()), next(current.size()) {
        if (data(a).has_epsilon)
            closure.emplace(a);
    }

    void advance(std::span<const symbol> w) {
        const auto& g = data(a);
        const auto& wt = detail::weights(a);
        std::ranges::fill(current, inf);
        current[u(g.initial)] = wt.initial;
        if (closure)
            closure->close(current);
        for (symbol c : w) {
            std::ranges::fill(next, inf);
            for (std::size_t s = 0; s < current.size(); ++s) {
                if (current[s] == inf)
                    continue;
                for (edge_id e : g.out(static_cast<state_id>(s))) {
                    if (!g.accepts(e, c))
                        continue;
                    double candidate = current[s] == -inf ? -inf : add(current[s], wt.edges[u(e)]);
                    auto t = u(g.target[u(e)]);
                    if (candidate < next[t])
                        next[t] = candidate;
                }
            }
            std::swap(current, next);
            if (closure)
                closure->close(current);
        }
    }

    [[nodiscard]] double terminal(const std::vector<double>& distances) const {
        const auto& g = data(a);
        const auto& wt = detail::weights(a);
        double best = inf;
        for (std::size_t s = 0; s < distances.size(); ++s)
            if (g.accept[s] && distances[s] != inf)
                best = std::min(best, distances[s] == -inf ? -inf : add(distances[s], wt.finals[s]));
        return best;
    }
};

weighted_automaton::cost_evaluator::cost_evaluator(const weighted_automaton& a) : state_(std::make_unique<state>(a)) {}
weighted_automaton::cost_evaluator::cost_evaluator(cost_evaluator&&) noexcept = default;
weighted_automaton::cost_evaluator& weighted_automaton::cost_evaluator::operator=(cost_evaluator&&) noexcept = default;
weighted_automaton::cost_evaluator::~cost_evaluator() = default;

double weighted_automaton::cost_evaluator::cost(std::span<const symbol> w) {
    const auto& g = data(state_->a);
    if (g.initial == no_state)
        return inf;
    if (g.deterministic)
        return state_->a.deterministic_cost(w);
    state_->advance(w);
    return state_->terminal(state_->current);
}

double weighted_automaton::cost_evaluator::optimal_cost_with_prefix(std::span<const symbol> prefix) {
    if (data(state_->a).initial == no_state)
        return inf;
    state_->advance(prefix);
    return state_->terminal(detail::solve(state_->a, state_->current, false, false, false).dist);
}

double weighted_automaton::cost(std::span<const symbol> w) const {
    return graph_.is_deterministic() ? deterministic_cost(w) : evaluator().cost(w);
}

weighted_automaton::cost_evaluator weighted_automaton::evaluator() const { return cost_evaluator(*this); }

double weighted_automaton::optimal_cost_with_prefix(std::span<const symbol> prefix) const {
    return evaluator().optimal_cost_with_prefix(prefix);
}

// Best paths

double weighted_automaton::optimal_cost() const { return shortest_paths(*this, false).optimal_cost(); }

std::optional<weighted_path> weighted_automaton::best_path() const { return shortest_paths(*this).best_path(); }

std::optional<weighted_path> weighted_automaton::best_path(std::span<const symbol> w) const {
    if (graph_.initial() == no_state)
        return std::nullopt;
    return layered_automaton::for_word(graph_, w).best_path(*this);
}

double weighted_automaton::cost_of_length(int length) const { return detail::fixed_length_cost(*this, length, false); }
double weighted_automaton::cost_of_length_at_most(int length) const { return detail::fixed_length_cost(*this, length, true); }

std::optional<weighted_path> weighted_automaton::best_path_of_length(int length) const {
    return layered_automaton::exactly(graph_, length).best_path(*this);
}

std::optional<word> weighted_automaton::best_word_of_length(int length) const {
    auto path = best_path_of_length(length);
    return path ? std::optional<word>(path->symbols()) : std::nullopt;
}

// Operations

weighted_automaton weighted_automaton::union_with(const weighted_automaton& other) const {
    builder out;
    auto root = out.add_state();
    out.set_initial(root);
    auto left = copy(*this, out, true), right = copy(other, out, true);
    if (graph_.initial() != no_state)
        out.add_epsilon(root, left[u(graph_.initial())], initial_weight());
    if (other.graph_.initial() != no_state)
        out.add_epsilon(root, right[u(other.graph_.initial())], other.initial_weight());
    return out.build();
}

weighted_automaton weighted_automaton::concatenate(const weighted_automaton& other) const {
    builder out;
    auto left = copy(*this, out, false), right = copy(other, out, true);
    if (graph_.initial() != no_state && other.graph_.initial() != no_state) {
        out.set_initial(left[u(graph_.initial())], initial_weight());
        const auto& g = data(graph_);
        for (std::size_t s = 0; s < left.size(); ++s)
            if (g.accept[s])
                out.add_epsilon(left[s], right[u(other.graph_.initial())], add(weights_->finals[s], other.initial_weight()));
    }
    return out.build();
}

weighted_automaton weighted_automaton::intersect_with(const weighted_automaton& other) const {
    const auto& ga = data(graph_);
    const auto& gb = data(other.graph_);
    const auto& wa = *weights_;
    const auto& wb = *other.weights_;
    builder out;
    if (ga.initial == no_state || gb.initial == no_state)
        return out.build();
    // Pairs of states, with the phase of the canonical interleaving of epsilon moves : left moves, then right
    // moves, between the transitions reading symbols
    struct pair {
        state_id left, right;
        int phase;
    };
    std::vector<pair> queue;
    std::unordered_map<std::uint64_t, state_id> ids;
    auto state_of = [&](state_id l, state_id r, int phase) {
        auto key = (static_cast<std::uint64_t>(l) << 33) | (static_cast<std::uint64_t>(r) << 2) | static_cast<std::uint64_t>(phase);
        auto [it, fresh] = ids.try_emplace(key, 0);
        if (fresh) {
            it->second = out.add_state();
            queue.push_back({l, r, phase});
        }
        return it->second;
    };
    state_of(ga.initial, gb.initial, 0);
    out.set_initial(0, add(wa.initial, wb.initial));
    for (std::size_t at = 0; at < queue.size(); ++at) {
        auto p = queue[at];
        auto from = static_cast<state_id>(at);
        if (ga.accept[u(p.left)] && gb.accept[u(p.right)])
            out.set_accept(from, add(wa.finals[u(p.left)], wb.finals[u(p.right)]));
        for (edge_id e : ga.out(p.left)) {
            if (ga.epsilon[u(e)]) {
                if (p.phase != 2)
                    out.add_epsilon(from, state_of(ga.target[u(e)], p.right, 1), wa.edges[u(e)]);
                continue;
            }
            auto le = ga.labels(e);
            for (edge_id f : gb.out(p.right)) {
                if (gb.epsilon[u(f)])
                    continue;
                auto common = le & gb.labels(f);
                if (!common.empty())
                    out.add_transition(from, state_of(ga.target[u(e)], gb.target[u(f)], 0), common, add(wa.edges[u(e)], wb.edges[u(f)]));
            }
        }
        for (edge_id f : gb.out(p.right))
            if (gb.epsilon[u(f)])
                out.add_epsilon(from, state_of(p.left, gb.target[u(f)], 2), wb.edges[u(f)]);
    }
    return out.build();
}

weighted_automaton weighted_automaton::reverse() const {
    const auto& g = data(graph_);
    builder out;
    if (g.initial == no_state)
        return out.build();
    for (std::size_t s = 0; s < g.states(); ++s)
        out.add_state();
    auto root = out.add_state();
    out.set_initial(root);
    out.set_accept(g.initial, weights_->initial);
    for (std::size_t s = 0; s < g.states(); ++s)
        if (g.accept[s])
            out.add_epsilon(root, static_cast<state_id>(s), weights_->finals[s]);
    for (std::size_t e = 0; e < g.edges(); ++e) {
        if (g.epsilon[e])
            out.add_epsilon(g.target[e], g.source[e], weights_->edges[e]);
        else
            out.add_transition(g.target[e], g.source[e], g.labels(static_cast<edge_id>(e)), weights_->edges[e]);
    }
    return out.build();
}

weighted_automaton weighted_automaton::star() const {
    builder out;
    auto root = out.add_state();
    out.set_initial(root);
    out.set_accept(root);
    auto map = copy(*this, out, false);
    if (graph_.initial() != no_state) {
        out.add_epsilon(root, map[u(graph_.initial())], weights_->initial);
        const auto& g = data(graph_);
        for (std::size_t s = 0; s < map.size(); ++s)
            if (g.accept[s])
                out.add_epsilon(map[s], root, weights_->finals[s]);
    }
    return out.build();
}

weighted_automaton weighted_automaton::repeat(int min, int max) const {
    if (min < 0 || (max >= 0 && max < min))
        throw std::invalid_argument("Invalid repetition bounds");
    if (max < 0)
        return repeat(min, min).concatenate(star());
    const auto& g = data(graph_);
    if ((static_cast<std::uint64_t>(max) + 1) * g.states() > static_cast<std::uint64_t>(std::numeric_limits<state_id>::max()))
        throw std::length_error("Repetition too large");
    builder out;
    auto root = out.add_state();
    out.set_initial(root);
    if (min == 0)
        out.set_accept(root);
    std::vector<state_id> previous;
    for (int k = 1; k <= max; ++k) {
        auto current = copy(*this, out, false);
        if (g.initial != no_state) {
            if (k == 1) {
                out.add_epsilon(root, current[u(g.initial)], weights_->initial);
            } else {
                for (std::size_t s = 0; s < previous.size(); ++s)
                    if (g.accept[s])
                        out.add_epsilon(previous[s], current[u(g.initial)], add(weights_->finals[s], weights_->initial));
            }
        }
        if (k >= min)
            for (std::size_t s = 0; s < current.size(); ++s)
                if (g.accept[s])
                    out.set_accept(current[s], weights_->finals[s]);
        previous = std::move(current);
    }
    return out.build();
}

weighted_automaton weighted_automaton::trim() const {
    const auto& g = data(graph_);
    auto useful = g.useful();
    builder out;
    std::vector<state_id> map(g.states(), no_state);
    for (std::size_t s = 0; s < map.size(); ++s)
        if (useful[s]) {
            map[s] = out.add_state();
            if (g.accept[s])
                out.set_accept(map[s], weights_->finals[s]);
        }
    if (g.initial != no_state && useful[u(g.initial)])
        out.set_initial(map[u(g.initial)], weights_->initial);
    for (std::size_t e = 0; e < g.edges(); ++e) {
        auto s = u(g.source[e]), t = u(g.target[e]);
        if (!useful[s] || !useful[t])
            continue;
        if (g.epsilon[e])
            out.add_epsilon(map[s], map[t], weights_->edges[e]);
        else
            out.add_transition(map[s], map[t], g.labels(static_cast<edge_id>(e)), weights_->edges[e]);
    }
    return out.build();
}

weighted_automaton weighted_automaton::simplify() const {
    auto a = trim();
    const auto& g = data(a.graph_);
    const auto& w = *a.weights_;
    // Edges of equal endpoints, mode and weight, in their order of first appearance
    struct merge {
        state_id from, to;
        bool epsilon;
        double weight;
        interval_set labels;
    };
    std::vector<merge> groups;
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> index;
    for (std::size_t e = 0; e < g.edges(); ++e) {
        auto bits = std::bit_cast<std::uint64_t>(w.edges[e]);
        auto key = (static_cast<std::uint64_t>(g.source[e]) * std::uint64_t{0x9e3779b97f4a7c15U}) ^ (static_cast<std::uint64_t>(g.target[e]) << 1) ^
                   bits ^ static_cast<std::uint64_t>(g.epsilon[e]);
        auto& candidates = index[key];
        std::size_t found = groups.size();
        for (std::size_t i : candidates) {
            const auto& m = groups[i];
            if (m.from == g.source[e] && m.to == g.target[e] && m.epsilon == (g.epsilon[e] != 0) &&
                std::bit_cast<std::uint64_t>(m.weight) == bits) {
                found = i;
                break;
            }
        }
        if (found == groups.size()) {
            groups.push_back({g.source[e], g.target[e], g.epsilon[e] != 0, w.edges[e], {}});
            candidates.push_back(found);
        }
        if (!g.epsilon[e])
            groups[found].labels |= g.labels(static_cast<edge_id>(e));
    }
    builder out;
    for (std::size_t s = 0; s < g.states(); ++s) {
        out.add_state();
        if (g.accept[s])
            out.set_accept(static_cast<state_id>(s), w.finals[s]);
    }
    if (g.initial != no_state)
        out.set_initial(g.initial, w.initial);
    for (const auto& m : groups) {
        if (m.epsilon)
            out.add_epsilon(m.from, m.to, m.weight);
        else
            out.add_transition(m.from, m.to, m.labels, m.weight);
    }
    return out.build();
}

compiled_weighted_dfa weighted_automaton::compile() const { return compiled_weighted_dfa(*this); }

}  // namespace autolib
