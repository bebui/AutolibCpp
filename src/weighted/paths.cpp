// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "weighted/paths.hpp"

#include "weighted/exact_costs.hpp"

#include <algorithm>
#include <optional>

namespace autolib {

namespace detail {

std::vector<double> seeds(std::size_t n, state_id s, double weight) {
    std::vector<double> seed(n, inf);
    if (s != no_state)
        seed[u(s)] = weight;
    return seed;
}

bool has_negative(const weighted_automaton& a, bool epsilon_only) {
    const auto& g = data(a);
    const auto& w = detail::weights(a).edges;
    for (std::size_t e = 0; e < g.edges(); ++e)
        if ((!epsilon_only || g.epsilon[e]) && w[e] < 0)
            return true;
    return false;
}

namespace {

std::vector<edge_id> parents(std::size_t n, bool track) {
    return track ? std::vector<edge_id>(n, -1) : std::vector<edge_id>{};
}

/// The edges followed from a state, and the state they lead to, forward or backward.
struct direction {
    const graph_data& g;
    bool reverse;

    [[nodiscard]] std::span<const edge_id> edges(state_id s) const noexcept { return reverse ? g.in(s) : g.out(s); }
    [[nodiscard]] state_id from(edge_id e) const noexcept { return reverse ? g.target[u(e)] : g.source[u(e)]; }
    [[nodiscard]] state_id to(edge_id e) const noexcept { return reverse ? g.source[u(e)] : g.target[u(e)]; }
};

/// Marks the states reachable from the marked ones, setting their distances to -infinity.
void mark_negative(direction dir, std::vector<double>* d, std::vector<char>& negative, bool epsilon_only) {
    std::vector<state_id> queue;
    for (std::size_t s = 0; s < negative.size(); ++s)
        if (negative[s])
            queue.push_back(static_cast<state_id>(s));
    for (std::size_t head = 0; head < queue.size(); ++head) {
        state_id s = queue[head];
        if (d != nullptr)
            (*d)[u(s)] = -inf;
        for (edge_id e : dir.edges(s)) {
            if (epsilon_only && !dir.g.epsilon[u(e)])
                continue;
            auto t = dir.to(e);
            if (!negative[u(t)]) {
                negative[u(t)] = 1;
                queue.push_back(t);
            }
        }
    }
}

std::vector<char> propagate(direction dir, const std::vector<double>& seed, bool epsilon_only) {
    std::vector<char> negative(seed.size());
    for (std::size_t s = 0; s < seed.size(); ++s)
        negative[s] = seed[s] == -inf;
    mark_negative(dir, nullptr, negative, epsilon_only);
    return negative;
}

relaxed dag(const weighted_automaton& a, const std::vector<double>& seed, bool reverse, bool longest, bool track) {
    const auto& g = data(a);
    const auto& w = detail::weights(a).edges;
    direction dir{g, reverse};
    relaxed r{seed, parents(seed.size(), track)};
    auto& d = r.dist;
    if (longest)
        std::ranges::replace(d, inf, -inf);
    double none = longest ? -inf : inf;
    for (std::size_t k = 0; k < g.order.size(); ++k) {
        state_id s = g.order[reverse ? g.order.size() - 1 - k : k];
        if (d[u(s)] == none)
            continue;
        for (edge_id e : dir.edges(s)) {
            auto t = dir.to(e);
            double candidate = d[u(s)] == -inf ? -inf : add(d[u(s)], w[u(e)]);
            if (longest ? candidate > d[u(t)] : candidate < d[u(t)]) {
                d[u(t)] = candidate;
                if (track)
                    r.parent[u(t)] = e;
            }
        }
    }
    return r;
}

relaxed dijkstra(const weighted_automaton& a, const std::vector<double>& seed, bool reverse, bool epsilon_only, bool track) {
    const auto& g = data(a);
    const auto& w = detail::weights(a).edges;
    direction dir{g, reverse};
    relaxed r{seed, parents(seed.size(), track)};
    auto& d = r.dist;
    auto negative = propagate(dir, seed, epsilon_only);
    state_heap heap(d);
    for (std::size_t s = 0; s < d.size(); ++s)
        if (!negative[s] && std::isfinite(d[s]))
            heap.offer(static_cast<state_id>(s));
    while (!heap.empty()) {
        state_id s = heap.poll();
        for (edge_id e : dir.edges(s)) {
            if (epsilon_only && !g.epsilon[u(e)])
                continue;
            auto t = dir.to(e);
            if (negative[u(t)])
                continue;
            double candidate = add(d[u(s)], w[u(e)]);
            if (candidate < d[u(t)]) {
                d[u(t)] = candidate;
                if (track)
                    r.parent[u(t)] = e;
                heap.offer(t);
            }
        }
    }
    for (std::size_t s = 0; s < d.size(); ++s)
        if (negative[s])
            d[s] = -inf;
    return r;
}

relaxed bellman(const weighted_automaton& a, const std::vector<double>& seed, bool reverse, bool epsilon_only, bool track) {
    const auto& g = data(a);
    const auto& w = detail::weights(a).edges;
    direction dir{g, reverse};
    std::size_t n = seed.size();
    auto negative = propagate(dir, seed, epsilon_only);
    // Synchronous rounds bound every finite candidate to at most n edges : bounded integers stay exact
    double bound = 0x1.0p53 / std::max(2.0, static_cast<double>(n) + 1.0);
    bool integral = true;
    for (double v : seed)
        if (std::isfinite(v) && (v != std::rint(v) || std::fabs(v) > bound))
            integral = false;
    for (std::size_t e = 0; e < g.edges(); ++e)
        if (!epsilon_only || g.epsilon[e])
            if (w[e] != std::rint(w[e]) || std::fabs(w[e]) > bound)
                integral = false;
    auto p = parents(n, track);
    if (integral) {
        std::vector<double> d = seed, next(n);
        for (std::size_t s = 0; s < n; ++s)
            if (negative[s])
                d[s] = inf;
        for (std::size_t pass = 0; pass < n; ++pass) {
            next = d;
            bool changed = false;
            for (std::size_t e = 0; e < g.edges(); ++e) {
                if (epsilon_only && !g.epsilon[e])
                    continue;
                auto s = u(dir.from(static_cast<edge_id>(e))), t = u(dir.to(static_cast<edge_id>(e)));
                if (d[s] == inf || negative[t])
                    continue;
                double candidate = add(d[s], w[e]);
                if (candidate < next[t]) {
                    changed = true;
                    next[t] = candidate;
                    if (track)
                        p[t] = static_cast<edge_id>(e);
                    if (pass == n - 1)
                        negative[t] = 1;
                }
            }
            std::swap(d, next);
            if (!changed)
                break;
        }
        mark_negative(dir, &d, negative, epsilon_only);
        return {std::move(d), std::move(p)};
    }
    // Exact distances, in units of the finest quantum of the values
    exact_scale scale;
    for (std::size_t s = 0; s < n; ++s)
        if (!negative[s] && std::isfinite(seed[s]))
            scale.include(seed[s]);
    for (std::size_t e = 0; e < g.edges(); ++e)
        if (!epsilon_only || g.epsilon[e])
            scale.include(w[e]);
    std::vector<exact> d(n), next(n), weight(g.edges());
    for (std::size_t s = 0; s < n; ++s)
        if (!negative[s] && std::isfinite(seed[s]))
            d[s] = scale.of(seed[s]);
    for (std::size_t e = 0; e < g.edges(); ++e)
        if (!epsilon_only || g.epsilon[e])
            weight[e] = scale.of(w[e]);
    big_integer candidate;
    for (std::size_t pass = 0; pass < n; ++pass) {
        next = d;
        bool changed = false;
        for (std::size_t e = 0; e < g.edges(); ++e) {
            if (!weight[e])
                continue;
            auto s = u(dir.from(static_cast<edge_id>(e))), t = u(dir.to(static_cast<edge_id>(e)));
            if (!d[s] || negative[t])
                continue;
            candidate = *d[s];
            candidate += *weight[e];
            if (!next[t] || candidate < *next[t]) {
                changed = true;
                next[t] = candidate;
                if (track)
                    p[t] = static_cast<edge_id>(e);
                if (pass == n - 1)
                    negative[t] = 1;
            }
        }
        std::swap(d, next);
        if (!changed)
            break;
    }
    std::vector<double> result(n, inf);
    mark_negative(dir, &result, negative, epsilon_only);
    for (std::size_t s = 0; s < n; ++s)
        if (!negative[s] && d[s]) {
            result[s] = scale.value(*d[s]).to_double();
            if (!std::isfinite(result[s]))
                throw std::overflow_error("Cost overflow");
        }
    return {std::move(result), std::move(p)};
}

void close_epsilon(const weighted_automaton& a, std::vector<double>& distances) {
    // No DAG shortcut : the order of a DAG covers the edges reading symbols too
    auto r = has_negative(a, true) ? bellman(a, distances, false, true, false) : dijkstra(a, distances, false, true, false);
    distances = std::move(r.dist);
}

}  // namespace

relaxed solve(const weighted_automaton& a, const std::vector<double>& seed, bool reverse, bool epsilon_only, bool track) {
    if (data(a).acyclic)
        return dag(a, seed, reverse, false, track);
    return has_negative(a, epsilon_only) ? bellman(a, seed, reverse, epsilon_only, track)
                                         : dijkstra(a, seed, reverse, epsilon_only, track);
}

shortest_path_result shortest_from(const weighted_automaton& a, state_id start, double value, bool track, bool longest,
                                   bool force_dijkstra) {
    auto seed = seeds(a.graph().state_count(), start, value);
    relaxed r = force_dijkstra                ? dijkstra(a, seed, false, false, track)
                : data(a).acyclic             ? dag(a, seed, false, longest, track)
                : has_negative(a, false)      ? bellman(a, seed, false, false, track)
                                              : dijkstra(a, seed, false, false, track);
    return weighted_access::result(a, std::move(r.dist), std::move(r.parent), start, longest);
}

epsilon_workspace::epsilon_workspace(const weighted_automaton& a)
    : a_(&a), signed_(has_negative(a, true)), work_(a.graph().state_count()), heap_(work_) {
    const auto& g = data(a);
    const auto& w = detail::weights(a).edges;
    start_.reserve(g.states() + 1);
    start_.push_back(0);
    for (std::size_t s = 0; s < g.states(); ++s) {
        for (edge_id e : g.out(static_cast<state_id>(s)))
            if (g.epsilon[u(e)]) {
                target_.push_back(g.target[u(e)]);
                weight_.push_back(w[u(e)]);
            }
        start_.push_back(static_cast<std::uint32_t>(target_.size()));
    }
}

void epsilon_workspace::close(std::vector<double>& distances) {
    bool fallback = signed_ || std::ranges::find(distances, -inf) != distances.end();
    if (fallback) {
        close_epsilon(*a_, distances);
        return;
    }
    // Dijkstra on the epsilon edges; only the states with epsilon edges can improve others
    work_ = distances;
    heap_.clear();
    for (std::size_t s = 0; s < work_.size(); ++s)
        if (start_[s] != start_[s + 1] && std::isfinite(work_[s]))
            heap_.offer(static_cast<state_id>(s));
    while (!heap_.empty()) {
        auto s = u(heap_.poll());
        for (auto i = start_[s]; i < start_[s + 1]; ++i) {
            double candidate = add(work_[s], weight_[i]);
            auto t = target_[i];
            if (candidate < work_[u(t)]) {
                work_[u(t)] = candidate;
                if (start_[u(t)] != start_[u(t) + 1])
                    heap_.offer(t);
            }
        }
    }
    distances = work_;
}

double fixed_length_cost(const weighted_automaton& a, int length, bool at_most) {
    if (length < 0)
        throw std::invalid_argument("Negative length");
    const auto& g = data(a);
    const auto& w = detail::weights(a);
    if (g.initial == no_state)
        return inf;
    auto current = seeds(g.states(), g.initial, w.initial);
    std::vector<double> next(current.size());
    std::optional<epsilon_workspace> closure;
    if (g.has_epsilon)
        closure.emplace(a);
    double best = inf;
    for (int position = 0;; ++position) {
        if (closure)
            closure->close(current);
        if (at_most || position == length)
            for (std::size_t s = 0; s < current.size(); ++s)
                if (g.accept[s])
                    best = std::min(best, join(current[s], 0, w.finals[s]));
        if (position == length)
            return best;
        std::ranges::fill(next, inf);
        for (std::size_t e = 0; e < g.edges(); ++e)
            if (!g.epsilon[e] && current[u(g.source[e])] != inf) {
                double candidate = join(current[u(g.source[e])], w.edges[e], 0);
                if (candidate < next[u(g.target[e])])
                    next[u(g.target[e])] = candidate;
            }
        std::swap(current, next);
    }
}

}  // namespace detail

using detail::inf;
using detail::u;

// shortest_path_result

shortest_path_result::shortest_path_result(const weighted_automaton& a, std::vector<double> distances,
                                           std::vector<edge_id> parent, state_id start, bool longest)
    : automaton_(std::make_shared<const weighted_automaton>(a)), distance_(std::move(distances)), parent_(std::move(parent)),
      start_(start), longest_(longest) {}

double shortest_path_result::distance_to(state_id s) const {
    detail::data(*automaton_).check_state(s);
    double d = distance_[u(s)];
    return longest_ && d == -inf ? inf : d;
}

std::vector<double> shortest_path_result::distances() const {
    auto result = distance_;
    if (longest_)
        std::ranges::replace(result, -inf, inf);
    return result;
}

shortest_path_result::status shortest_path_result::status_to(state_id s) const {
    double d = distance_to(s);
    return d == inf ? status::unreachable : d == -inf ? status::unbounded : status::finite;
}

std::optional<weighted_path> shortest_path_result::path_to(state_id s) const {
    auto st = status_to(s);
    if (st == status::unreachable)
        return std::nullopt;
    if (st == status::unbounded)
        throw std::logic_error("Unbounded optimum has no best path");
    if (parent_.empty())
        throw std::logic_error("Predecessors were not requested");
    const auto& g = detail::data(*automaton_);
    std::vector<edge_id> edges;
    for (state_id at = s; at != start_;) {
        if (edges.size() == g.states() || parent_[u(at)] < 0)
            throw std::logic_error("Invalid predecessor chain");
        edges.push_back(parent_[u(at)]);
        at = g.source[u(edges.back())];
    }
    std::ranges::reverse(edges);
    word w;
    for (edge_id e : edges)
        if (!g.epsilon[u(e)])
            w.push_back(g.representative(e));
    return weighted_path(automaton_->graph(), start_, std::move(edges), std::move(w), distance_[u(s)]);
}

state_id shortest_path_result::best_final() const {
    const auto& g = detail::data(*automaton_);
    const auto& w = detail::weights(*automaton_);
    state_id best = no_state;
    double value = longest_ ? -inf : inf;
    for (std::size_t s = 0; s < distance_.size(); ++s) {
        if (!g.accept[s] || status_to(static_cast<state_id>(s)) == status::unreachable)
            continue;
        double candidate = distance_[s] == -inf ? -inf : detail::add(distance_[s], w.finals[s]);
        if (best == no_state || (longest_ ? candidate > value : candidate < value)) {
            best = static_cast<state_id>(s);
            value = candidate;
        }
    }
    return best;
}

double shortest_path_result::optimal_cost() const {
    state_id best = best_final();
    if (best == no_state)
        return inf;
    if (distance_[u(best)] == -inf && !longest_)
        return -inf;
    return detail::add(distance_[u(best)], detail::weights(*automaton_).finals[u(best)]);
}

std::optional<weighted_path> shortest_path_result::best_path() const {
    state_id best = best_final();
    if (best == no_state)
        return std::nullopt;
    auto path = path_to(best);
    return weighted_path(path->graph(), start_, {path->edges().begin(), path->edges().end()}, path->symbols(),
                         detail::add(path->cost(), detail::weights(*automaton_).finals[u(best)]));
}

// weighted_path

weighted_path::weighted_path(automaton_graph graph, state_id start, std::vector<edge_id> edges, word w, double cost)
    : graph_(std::move(graph)), start_(start), edges_(std::move(edges)), word_(std::move(w)), cost_(cost) {
    const auto& g = detail::data(graph_);
    g.check_state(start);
    state_id s = start;
    std::size_t position = 0;
    for (edge_id e : edges_) {
        g.check_edge(e);
        if (g.source[u(e)] != s)
            throw std::invalid_argument("Disconnected path");
        s = g.target[u(e)];
        if (!g.epsilon[u(e)] && (position == word_.size() || !g.accepts(e, word_[position++])))
            throw std::invalid_argument("Path does not spell word");
    }
    if (position != word_.size())
        throw std::invalid_argument("Wrong word length");
}

state_id weighted_path::end() const noexcept {
    return edges_.empty() ? start_ : detail::data(graph_).target[u(edges_.back())];
}

// path_analysis

path_analysis::path_analysis(const weighted_automaton& a)
    : automaton_(std::make_shared<const weighted_automaton>(a)), forward_(shortest_paths(a)), backward_(backward_distances(a)) {}

double path_analysis::cost_through_state(state_id s) const {
    detail::data(*automaton_).check_state(s);
    return detail::join(forward_.distance_to(s), 0, backward_[u(s)]);
}

double path_analysis::cost_through_edge(edge_id e) const {
    const auto& g = detail::data(*automaton_);
    g.check_edge(e);
    return detail::join(forward_.distance_to(g.source[u(e)]), detail::weights(*automaton_).edges[u(e)], backward_[u(g.target[u(e)])]);
}

// Free functions

shortest_path_result shortest_paths(const weighted_automaton& a, bool reconstruct) {
    return detail::shortest_from(a, a.graph().initial(), a.initial_weight(), reconstruct, false, false);
}

shortest_path_result shortest_paths_from(const weighted_automaton& a, state_id source) {
    detail::data(a).check_state(source);
    return detail::shortest_from(a, source, 0, true, false, false);
}

shortest_path_result dag_shortest_paths(const weighted_automaton& a) {
    detail::data(a).require_dag();
    return detail::shortest_from(a, a.graph().initial(), a.initial_weight(), true, false, false);
}

shortest_path_result dag_longest_paths(const weighted_automaton& a) {
    detail::data(a).require_dag();
    return detail::shortest_from(a, a.graph().initial(), a.initial_weight(), true, true, false);
}

shortest_path_result dijkstra(const weighted_automaton& a) {
    for (double w : a.weights())
        if (w < 0)
            throw std::invalid_argument("Dijkstra requires nonnegative edges");
    return detail::shortest_from(a, a.graph().initial(), a.initial_weight(), true, false, true);
}

shortest_path_result bellman_ford(const weighted_automaton& a) {
    auto seed = detail::seeds(a.graph().state_count(), a.graph().initial(), a.initial_weight());
    auto r = detail::bellman(a, seed, false, false, true);
    return detail::weighted_access::result(a, std::move(r.dist), std::move(r.parent), a.graph().initial(), false);
}

std::vector<double> backward_distances(const weighted_automaton& a) {
    const auto& g = detail::data(a);
    const auto& w = detail::weights(a);
    std::vector<double> seed(g.states());
    for (std::size_t s = 0; s < seed.size(); ++s)
        seed[s] = g.accept[s] ? w.finals[s] : inf;
    return detail::solve(a, seed, true, false, false).dist;
}

std::vector<double> distances_to(const weighted_automaton& a, state_id destination) {
    detail::data(a).check_state(destination);
    return detail::solve(a, detail::seeds(a.graph().state_count(), destination, 0), true, false, false).dist;
}

double cost_through_state(const weighted_automaton& a, state_id s) {
    detail::data(a).check_state(s);
    return detail::join(shortest_paths(a, false).distance_to(s), 0, backward_distances(a)[u(s)]);
}

double cost_through_edge(const weighted_automaton& a, edge_id e) {
    const auto& g = detail::data(a);
    g.check_edge(e);
    return detail::join(shortest_paths(a, false).distance_to(g.source[u(e)]), a.weight(e), backward_distances(a)[u(g.target[u(e)])]);
}

}  // namespace autolib
