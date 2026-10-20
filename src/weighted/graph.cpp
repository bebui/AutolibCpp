// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <algorithm>
#include <limits>

#include "weighted/graph_data.hpp"

namespace autolib {

using detail::graph_data;
using detail::u;

namespace {

std::shared_ptr<const graph_data> empty_graph() {
    static const auto empty = [] {
        auto d = std::make_shared<graph_data>();
        d->out_start = {0};
        d->in_start = {0};
        return d;
    }();
    return empty;
}

/// Compressed rows of the edges grouped by an endpoint : start has n + 1 entries.
void group(const std::vector<state_id>& endpoint, std::size_t n, std::vector<std::uint32_t>& start, std::vector<edge_id>& edges) {
    start.assign(n + 1, 0);
    for (state_id s : endpoint)
        ++start[u(s) + 1];
    for (std::size_t s = 0; s < n; ++s)
        start[s + 1] += start[s];
    edges.resize(endpoint.size());
    std::vector<std::uint32_t> fill(start.begin(), start.end() - 1);
    for (std::size_t e = 0; e < endpoint.size(); ++e)
        edges[fill[u(endpoint[e])]++] = static_cast<edge_id>(e);
}

bool check_determinism(const graph_data& g) {
    if (g.has_epsilon)
        return false;
    std::vector<interval> labels;
    for (std::size_t s = 0; s < g.states(); ++s) {
        labels.clear();
        for (edge_id e : g.out(static_cast<state_id>(s)))
            for (auto i = g.label_start[u(e)]; i < g.label_start[u(e) + 1]; ++i)
                labels.push_back({g.mins[i], g.maxs[i]});
        std::ranges::sort(labels, {}, &interval::min);
        for (std::size_t i = 1; i < labels.size(); ++i)
            if (labels[i].min <= labels[i - 1].max)
                return false;
    }
    return true;
}

}  // namespace

std::vector<bool> graph_data::useful() const {
    std::size_t n = states();
    std::vector<bool> reached(n), result(n);
    std::vector<state_id> queue;
    queue.reserve(n);
    if (initial != no_state) {
        reached[u(initial)] = true;
        queue.push_back(initial);
    }
    for (std::size_t head = 0; head < queue.size(); ++head)
        for (edge_id e : out(queue[head]))
            if (!reached[u(target[u(e)])]) {
                reached[u(target[u(e)])] = true;
                queue.push_back(target[u(e)]);
            }
    queue.clear();
    for (std::size_t s = 0; s < n; ++s)
        if (accept[s] && reached[s]) {
            result[s] = true;
            queue.push_back(static_cast<state_id>(s));
        }
    for (std::size_t head = 0; head < queue.size(); ++head)
        for (edge_id e : in(queue[head])) {
            auto p = u(source[u(e)]);
            if (reached[p] && !result[p]) {
                result[p] = true;
                queue.push_back(static_cast<state_id>(p));
            }
        }
    return result;
}

automaton_graph::automaton_graph() : data_(empty_graph()) {}

automaton_graph automaton_graph::from(const automaton& a) {
    builder b;
    for (std::size_t s = 0; s < a.state_count(); ++s)
        b.add_state(a.is_accept(static_cast<state_id>(s)));
    if (a.initial() != no_state)
        b.set_initial(a.initial());
    for (std::size_t s = 0; s < a.state_count(); ++s) {
        auto from = static_cast<state_id>(s);
        for (const auto& t : a.transitions(from))
            b.add_transition(from, t.target, t.labels);
        for (state_id t : a.epsilons(from))
            b.add_epsilon(from, t);
    }
    return b.build();
}

std::size_t automaton_graph::state_count() const noexcept { return data_->states(); }
std::size_t automaton_graph::edge_count() const noexcept { return data_->edges(); }
state_id automaton_graph::initial() const noexcept { return data_->initial; }

bool automaton_graph::is_accept(state_id s) const {
    data_->check_state(s);
    return data_->accept[u(s)] != 0;
}
state_id automaton_graph::source(edge_id e) const {
    data_->check_edge(e);
    return data_->source[u(e)];
}
state_id automaton_graph::target(edge_id e) const {
    data_->check_edge(e);
    return data_->target[u(e)];
}
bool automaton_graph::is_epsilon(edge_id e) const {
    data_->check_edge(e);
    return data_->epsilon[u(e)] != 0;
}
interval_set automaton_graph::labels(edge_id e) const {
    data_->check_edge(e);
    return data_->labels(e);
}
bool automaton_graph::accepts(edge_id e, symbol c) const {
    data_->check_edge(e);
    return data_->accepts(e, c);
}
std::span<const edge_id> automaton_graph::outgoing(state_id s) const {
    data_->check_state(s);
    return data_->out(s);
}
std::span<const edge_id> automaton_graph::incoming(state_id s) const {
    data_->check_state(s);
    return data_->in(s);
}
bool automaton_graph::is_acyclic() const noexcept { return data_->acyclic; }
std::span<const state_id> automaton_graph::topological_order() const {
    data_->require_dag();
    return data_->order;
}
bool automaton_graph::is_deterministic() const noexcept { return data_->deterministic; }
std::vector<bool> automaton_graph::useful_states() const { return data_->useful(); }

void automaton_graph::builder::check(state_id s) const {
    if (s < 0 || u(s) >= accept_.size())
        throw std::out_of_range("State " + std::to_string(s));
}

state_id automaton_graph::builder::add_state(bool accept) {
    if (accept_.size() >= static_cast<std::size_t>(std::numeric_limits<state_id>::max()))
        throw std::length_error("Too many states");
    accept_.push_back(accept ? 1 : 0);
    return static_cast<state_id>(accept_.size() - 1);
}

void automaton_graph::builder::set_initial(state_id s) {
    check(s);
    initial_ = s;
}

void automaton_graph::builder::set_accept(state_id s, bool accept) {
    check(s);
    accept_[u(s)] = accept ? 1 : 0;
}

edge_id automaton_graph::builder::add_transition(state_id from, state_id to, const interval_set& labels) {
    if (labels.empty())
        throw std::invalid_argument("Empty label");
    check(from);
    check(to);
    source_.push_back(from);
    target_.push_back(to);
    labels_.push_back(labels);
    epsilon_.push_back(0);
    return static_cast<edge_id>(source_.size() - 1);
}

edge_id automaton_graph::builder::add_epsilon(state_id from, state_id to) {
    check(from);
    check(to);
    source_.push_back(from);
    target_.push_back(to);
    labels_.emplace_back();
    epsilon_.push_back(1);
    return static_cast<edge_id>(source_.size() - 1);
}

automaton_graph automaton_graph::builder::build() const {
    auto d = std::make_shared<graph_data>();
    std::size_t n = accept_.size(), m = source_.size();
    d->initial = initial_;
    d->accept = accept_;
    d->source = source_;
    d->target = target_;
    d->epsilon = epsilon_;
    d->has_epsilon = std::ranges::find(epsilon_, 1) != epsilon_.end();
    d->label_start.assign(m + 1, 0);
    for (std::size_t e = 0; e < m; ++e) {
        for (interval i : labels_[e]) {
            d->mins.push_back(i.min);
            d->maxs.push_back(i.max);
        }
        d->label_start[e + 1] = static_cast<std::uint32_t>(d->mins.size());
    }
    group(d->source, n, d->out_start, d->out_edges);
    group(d->target, n, d->in_start, d->in_edges);
    // Topological order by Kahn's algorithm
    std::vector<std::uint32_t> degree(n);
    for (std::size_t s = 0; s < n; ++s)
        degree[s] = d->in_start[s + 1] - d->in_start[s];
    std::vector<state_id> sorted;
    sorted.reserve(n);
    for (std::size_t s = 0; s < n; ++s)
        if (degree[s] == 0)
            sorted.push_back(static_cast<state_id>(s));
    for (std::size_t head = 0; head < sorted.size(); ++head)
        for (edge_id e : d->out(sorted[head]))
            if (--degree[u(d->target[u(e)])] == 0)
                sorted.push_back(d->target[u(e)]);
    d->acyclic = sorted.size() == n;
    if (d->acyclic)
        d->order = std::move(sorted);
    d->deterministic = check_determinism(*d);
    return automaton_graph(std::move(d));
}

}  // namespace autolib
