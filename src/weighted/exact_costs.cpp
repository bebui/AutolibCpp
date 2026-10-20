// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "weighted/exact_costs.hpp"

#include <deque>
#include <limits>
#include <queue>

namespace autolib::detail {

exact_costs::exact_costs(const weighted_automaton& a) : a_(a) {
    const auto& g = data(a);
    const auto& w = detail::weights(a);
    scale.include(w.initial);
    for (double x : w.edges)
        scale.include(x);
    for (std::size_t s = 0; s < g.states(); ++s)
        if (g.accept[s])
            scale.include(w.finals[s]);
    initial = scale.of(w.initial);
    edges.reserve(g.edges());
    for (double x : w.edges)
        edges.push_back(scale.of(x));
    finals.resize(g.states());
    for (std::size_t s = 0; s < g.states(); ++s)
        if (g.accept[s])
            finals[s] = scale.of(w.finals[s]);
}

std::vector<exact> exact_costs::suffixes(const std::vector<bool>& useful) const {
    const auto& g = graph();
    std::size_t n = g.states();
    std::vector<exact> distances(n);
    bool nonnegative = true;
    for (std::size_t e = 0; e < edges.size(); ++e)
        if (useful[u(g.source[e])] && useful[u(g.target[e])] && edges[e].signum() < 0)
            nonnegative = false;
    for (std::size_t s = 0; s < n; ++s)
        if (useful[s])
            distances[s] = finals[s];
    if (g.acyclic) {
        for (std::size_t i = n; i-- > 0;) {
            auto s = g.order[i];
            if (!useful[u(s)])
                continue;
            for (edge_id e : g.out(s)) {
                auto t = u(g.target[u(e)]);
                if (distances[t]) {
                    big_integer value = edges[u(e)] + *distances[t];
                    if (improves(value, distances[u(s)]))
                        distances[u(s)] = std::move(value);
                }
            }
        }
    } else if (nonnegative) {
        // Dijkstra backward, with lazy deletion
        using entry = std::pair<big_integer, state_id>;
        auto later = [](const entry& x, const entry& y) { return x.first > y.first; };
        std::priority_queue<entry, std::vector<entry>, decltype(later)> queue(later);
        for (std::size_t s = 0; s < n; ++s)
            if (distances[s])
                queue.emplace(*distances[s], static_cast<state_id>(s));
        while (!queue.empty()) {
            auto [cost, t] = queue.top();
            queue.pop();
            if (cost != *distances[u(t)])
                continue;
            for (edge_id e : g.in(t)) {
                auto s = g.source[u(e)];
                if (!useful[u(s)])
                    continue;
                big_integer value = edges[u(e)] + cost;
                if (improves(value, distances[u(s)])) {
                    distances[u(s)] = value;
                    queue.emplace(std::move(value), s);
                }
            }
        }
    } else {
        for (std::size_t pass = 0; pass < n; ++pass) {
            bool changed = false;
            for (std::size_t e = 0; e < edges.size(); ++e) {
                auto s = u(g.source[e]), t = u(g.target[e]);
                if (!useful[s] || !distances[t])
                    continue;
                big_integer value = edges[e] + *distances[t];
                if (improves(value, distances[s])) {
                    distances[s] = std::move(value);
                    changed = true;
                }
            }
            if (!changed)
                break;
            if (pass == n - 1)
                throw std::invalid_argument("Cost-ordered enumeration requires no useful negative cycle");
        }
    }
    return distances;
}

std::vector<int> exact_costs::symbol_steps(const std::vector<exact>& suffix) const {
    const auto& g = graph();
    constexpr int unknown = std::numeric_limits<int>::max();
    std::vector<int> steps(g.states(), unknown);
    std::deque<state_id> queue;
    for (std::size_t s = 0; s < steps.size(); ++s)
        if (suffix[s] && finals[s] && *suffix[s] == *finals[s]) {
            steps[s] = 0;
            queue.push_back(static_cast<state_id>(s));
        }
    while (!queue.empty()) {
        auto t = queue.front();
        queue.pop_front();
        for (edge_id e : g.in(t)) {
            auto s = u(g.source[u(e)]);
            int value = steps[u(t)] + (g.epsilon[u(e)] ? 0 : 1);
            if (value < steps[s] && suffix[s] && edges[u(e)] + *suffix[u(t)] == *suffix[s]) {
                steps[s] = value;
                if (g.epsilon[u(e)])
                    queue.push_front(static_cast<state_id>(s));
                else
                    queue.push_back(static_cast<state_id>(s));
            }
        }
    }
    return steps;
}

void exact_costs::close_epsilon(std::vector<exact>& distances, const std::vector<bool>* allowed) const {
    const auto& g = graph();
    std::size_t n = distances.size();
    for (std::size_t pass = 0; pass < n; ++pass) {
        bool changed = false;
        for (std::size_t e = 0; e < edges.size(); ++e) {
            if (!g.epsilon[e] || !distances[u(g.source[e])] || (allowed != nullptr && !(*allowed)[u(g.target[e])]))
                continue;
            big_integer value = *distances[u(g.source[e])] + edges[e];
            auto t = u(g.target[e]);
            if (improves(value, distances[t])) {
                distances[t] = std::move(value);
                changed = true;
            }
        }
        if (!changed)
            return;
        if (pass == n - 1)
            throw std::invalid_argument("Negative epsilon cycle cannot have finite transformed weights");
    }
}

}  // namespace autolib::detail
