// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "autolib/automaton_snapshot.hpp"

#include "weighted/common.hpp"

namespace autolib {

using detail::u;

automaton_snapshot::automaton_snapshot(const automaton& a) {
    automaton_graph::builder b;
    for (std::size_t s = 0; s < a.state_count(); ++s)
        b.add_state(a.is_accept(static_cast<state_id>(s)));
    if (a.initial() != no_state)
        b.set_initial(a.initial());
    for (std::size_t s = 0; s < a.state_count(); ++s) {
        auto from = static_cast<state_id>(s);
        auto transitions = a.transitions(from);
        for (std::size_t i = 0; i < transitions.size(); ++i) {
            if (transitions[i].labels.empty())
                continue;  // reads nothing : not an edge
            b.add_transition(from, transitions[i].target, transitions[i].labels);
            origins_.push_back({static_cast<std::uint32_t>(i), false});
        }
        auto epsilons = a.epsilons(from);
        for (std::size_t i = 0; i < epsilons.size(); ++i) {
            b.add_epsilon(from, epsilons[i]);
            origins_.push_back({static_cast<std::uint32_t>(i), true});
        }
    }
    graph_ = b.build();
}

automaton_edge automaton_snapshot::origin(edge_id e) const {
    const auto& g = detail::data(graph_);
    g.check_edge(e);
    const auto& o = origins_[u(e)];
    return {g.source[u(e)], g.target[u(e)], o.epsilon, g.labels(e), o.index};
}

weighted_automaton automaton_snapshot::weighted(const edge_cost& cost) const {
    return weighted(cost, 0, nullptr);
}

weighted_automaton automaton_snapshot::weighted(const edge_cost& cost, double initial_cost,
                                                const std::function<double(state_id)>& final_cost) const {
    if (!cost)
        throw std::invalid_argument("Missing cost function");
    const auto& g = detail::data(graph_);
    std::vector<double> edges(g.edges()), finals(g.states(), 0.0);
    for (std::size_t e = 0; e < edges.size(); ++e)
        edges[e] = cost(origin(static_cast<edge_id>(e)));
    if (final_cost)
        for (std::size_t s = 0; s < finals.size(); ++s)
            if (g.accept[s])
                finals[s] = final_cost(static_cast<state_id>(s));
    return weighted_automaton::of(graph_, std::move(edges), initial_cost, std::move(finals));
}

multi_weighted_automaton automaton_snapshot::multi_weighted(std::span<const edge_cost> dimensions) const {
    if (dimensions.empty())
        throw std::invalid_argument("At least one dimension required");
    for (const auto& f : dimensions)
        if (!f)
            throw std::invalid_argument("Missing cost function");
    const auto& g = detail::data(graph_);
    std::size_t d = dimensions.size();
    std::vector<double> costs(g.edges() * d);
    for (std::size_t e = 0; e < g.edges(); ++e) {
        auto edge = origin(static_cast<edge_id>(e));
        for (std::size_t k = 0; k < d; ++k)
            costs[e * d + k] = dimensions[k](edge);
    }
    return multi_weighted_automaton::of(graph_, d, std::move(costs));
}

// The weightings of automata

automaton_snapshot automaton::snapshot() const { return automaton_snapshot(*this); }

weighted_automaton automaton::weighted(const edge_cost& cost) const { return snapshot().weighted(cost); }

multi_weighted_automaton automaton::multi_weighted(std::span<const edge_cost> dimensions) const {
    return snapshot().multi_weighted(dimensions);
}

multi_weighted_automaton automaton::multi_weighted(std::initializer_list<edge_cost> dimensions) const {
    return snapshot().multi_weighted(dimensions);
}

weighted_automaton weighted_automaton::from(const automaton& a, const edge_cost& cost) { return a.weighted(cost); }

multi_weighted_automaton multi_weighted_automaton::from(const automaton& a, std::span<const edge_cost> dimensions) {
    return a.multi_weighted(dimensions);
}

}  // namespace autolib
