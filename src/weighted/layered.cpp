// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "autolib/layered_automaton.hpp"

#include <limits>

#include "weighted/layered_data.hpp"
#include "weighted/paths.hpp"

namespace autolib {

using detail::data;
using detail::inf;
using detail::u;

layered_automaton layered_automaton::build(const automaton_graph& base_graph, int horizon, bool at_most, const symbol* w) {
    if (horizon < 0)
        throw std::invalid_argument("Negative horizon");
    const auto& base = data(base_graph);
    std::size_t n = base.states();
    auto count64 = (static_cast<std::uint64_t>(horizon) + 1) * n;
    if (count64 > static_cast<std::uint64_t>(std::numeric_limits<state_id>::max()))
        throw std::length_error("Unfolding too large");
    std::size_t count = count64;
    // A pair (position, state) is the node position * n + state
    auto blocked_forward = [&](edge_id e, std::size_t position) {
        return !base.epsilon[u(e)] && (position == u(horizon) || (w != nullptr && !base.accepts(e, w[position])));
    };
    std::vector<char> forward(count), backward(count);
    std::vector<std::size_t> queue;
    if (base.initial != no_state) {
        forward[u(base.initial)] = 1;
        queue.push_back(u(base.initial));
    }
    for (std::size_t head = 0; head < queue.size(); ++head) {
        auto node = queue[head], position = node / n;
        auto s = static_cast<state_id>(node % n);
        for (edge_id e : base.out(s)) {
            if (blocked_forward(e, position))
                continue;
            auto t = (position + (base.epsilon[u(e)] ? 0 : 1)) * n + u(base.target[u(e)]);
            if (!forward[t]) {
                forward[t] = 1;
                queue.push_back(t);
            }
        }
    }
    queue.clear();
    for (std::size_t position = at_most ? 0 : u(horizon); position <= u(horizon); ++position)
        for (std::size_t s = 0; s < n; ++s)
            if (base.accept[s] && forward[position * n + s]) {
                backward[position * n + s] = 1;
                queue.push_back(position * n + s);
            }
    for (std::size_t head = 0; head < queue.size(); ++head) {
        auto node = queue[head], position = node / n;
        auto s = static_cast<state_id>(node % n);
        for (edge_id e : base.in(s)) {
            if (!base.epsilon[u(e)] && (position == 0 || (w != nullptr && !base.accepts(e, w[position - 1]))))
                continue;
            auto p = (position - (base.epsilon[u(e)] ? 0 : 1)) * n + u(base.source[u(e)]);
            if (forward[p] && !backward[p]) {
                backward[p] = 1;
                queue.push_back(p);
            }
        }
    }
    auto d = std::make_shared<structure>();
    d->base = base_graph;
    d->horizon = horizon;
    automaton_graph::builder b;
    std::vector<state_id> ids(count, no_state);
    for (std::size_t node = 0; node < count; ++node) {
        if (!backward[node])
            continue;
        auto id = b.add_state();
        ids[node] = id;
        d->original_state.push_back(static_cast<state_id>(node % n));
        d->position.push_back(static_cast<int>(node / n));
        if (base.accept[node % n] && (at_most || node / n == u(horizon)))
            b.set_accept(id);
    }
    if (base.initial != no_state && ids[u(base.initial)] != no_state)
        b.set_initial(ids[u(base.initial)]);
    for (std::size_t node = 0; node < count; ++node) {
        if (ids[node] == no_state)
            continue;
        auto position = node / n;
        auto s = static_cast<state_id>(node % n);
        for (edge_id e : base.out(s)) {
            if (blocked_forward(e, position))
                continue;
            auto next = (position + (base.epsilon[u(e)] ? 0 : 1)) * n + u(base.target[u(e)]);
            if (ids[next] == no_state)
                continue;
            if (base.epsilon[u(e)])
                b.add_epsilon(ids[node], ids[next]);
            else if (w != nullptr)
                b.add_transition(ids[node], ids[next], w[position]);
            else
                b.add_transition(ids[node], ids[next], base.labels(e));
            d->original_edge.push_back(e);
            d->edge_position.push_back(static_cast<int>(position));
        }
    }
    d->graph = b.build();
    return layered_automaton(std::move(d));
}

layered_automaton layered_automaton::exactly(const automaton_graph& graph, int horizon) { return build(graph, horizon, false, nullptr); }
layered_automaton layered_automaton::at_most(const automaton_graph& graph, int horizon) { return build(graph, horizon, true, nullptr); }
layered_automaton layered_automaton::for_word(const automaton_graph& graph, std::span<const symbol> w) {
    if (w.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::length_error("Word too long");
    return build(graph, static_cast<int>(w.size()), false, w.data());
}

const automaton_graph& layered_automaton::base_graph() const noexcept { return data_->base; }
const automaton_graph& layered_automaton::graph() const noexcept { return data_->graph; }
int layered_automaton::horizon() const noexcept { return data_->horizon; }

state_id layered_automaton::original_state(state_id s) const {
    data(data_->graph).check_state(s);
    return data_->original_state[u(s)];
}

int layered_automaton::position(state_id s) const {
    data(data_->graph).check_state(s);
    return data_->position[u(s)];
}

edge_id layered_automaton::original_edge(edge_id e) const {
    data(data_->graph).check_edge(e);
    return data_->original_edge[u(e)];
}

void layered_automaton::check(const automaton_graph& g) const {
    if (!g.same_as(data_->base))
        throw std::invalid_argument("Weighting must share the base graph");
}

weighted_automaton layered_automaton::evaluate(const weighted_automaton& a) const {
    check(a.graph());
    const auto& w = detail::weights(a);
    detail::weighted_access::weights unfolded;
    unfolded.edges.reserve(data_->original_edge.size());
    for (edge_id e : data_->original_edge)
        unfolded.edges.push_back(w.edges[u(e)]);
    unfolded.initial = w.initial;
    for (state_id s : data_->original_state)
        unfolded.finals.push_back(w.finals[u(s)]);
    return detail::weighted_access::make(data_->graph, std::move(unfolded));
}

weighted_automaton layered_automaton::evaluate(const weighted_automaton& a, const position_weights& weights) const {
    check(a.graph());
    std::vector<double> edges(data_->original_edge.size()), finals;
    for (std::size_t e = 0; e < edges.size(); ++e)
        edges[e] = weights(data_->edge_position[e], data_->original_edge[e]);
    for (state_id s : data_->original_state)
        finals.push_back(a.final_weight(s));
    return weighted_automaton::of(data_->graph, std::move(edges), a.initial_weight(), std::move(finals));
}

weighted_path layered_automaton::map(const weighted_path& path) const {
    if (!path.graph().same_as(data_->graph))
        throw std::invalid_argument("Foreign unfolded path");
    std::vector<edge_id> edges;
    edges.reserve(path.edge_count());
    for (edge_id e : path.edges())
        edges.push_back(data_->original_edge[u(e)]);
    return weighted_path(data_->base, data_->original_state[u(path.start())], std::move(edges), path.symbols(), path.cost());
}

std::optional<weighted_path> layered_automaton::best_path(const weighted_automaton& a) const {
    auto path = evaluate(a).best_path();
    return path ? std::optional<weighted_path>(map(*path)) : std::nullopt;
}

layered_automaton::analysis layered_automaton::analyze(const weighted_automaton& a) const { return {*this, evaluate(a)}; }

layered_automaton::analysis layered_automaton::analyze(const weighted_automaton& a, const position_weights& weights) const {
    return {*this, evaluate(a, weights)};
}

layered_automaton::analysis::analysis(layered_automaton layers, weighted_automaton evaluated)
    : layers_(std::move(layers)), evaluated_(std::move(evaluated)), forward_(shortest_paths(evaluated_)),
      backward_(autolib::backward_distances(evaluated_)) {}

std::optional<weighted_path> layered_automaton::analysis::best_path() const {
    auto path = forward_.best_path();
    return path ? std::optional<weighted_path>(layers_.map(*path)) : std::nullopt;
}

double layered_automaton::analysis::cost_at(int position, symbol c) const {
    if (position < 0 || position >= layers_.horizon())
        throw std::out_of_range("Position " + std::to_string(position));
    const auto& g = data(evaluated_);
    const auto& w = detail::weights(evaluated_);
    const auto& d = *layers_.data_;
    double best = inf;
    for (std::size_t e = 0; e < d.original_edge.size(); ++e)
        if (d.edge_position[e] == position && !g.epsilon[e] && g.accepts(static_cast<edge_id>(e), c))
            best = std::min(best, detail::join(forward_.distance_to(g.source[e]), w.edges[e], backward_[u(g.target[e])]));
    return best;
}

}  // namespace autolib
