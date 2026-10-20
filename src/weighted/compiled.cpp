// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "autolib/compiled_weighted_dfa.hpp"

#include <algorithm>

#include "detail/symbol_classes.hpp"

#include "weighted/common.hpp"

namespace autolib {

using detail::add;
using detail::data;
using detail::inf;
using detail::u;

/// The labels of the edges leaving each state, as disjoint intervals sorted by their least symbol. When small
/// enough, a table gives the edge of every state and symbol class, the class of a symbol being read in an
/// array when the symbols span a small range.
struct weighted_dfa_index::data {
    std::vector<std::uint32_t> start;
    std::vector<symbol> min, max;
    std::vector<edge_id> edge;

    detail::symbol_classes classes;
    std::vector<edge_id> table;          ///< edge of (state, class), or -1; empty without table
    std::vector<std::int32_t> direct;    ///< class of the symbols direct_low + i, or -1
    symbol direct_low = 0;

    [[nodiscard]] std::int32_t class_of(symbol c) const noexcept {
        if (!direct.empty()) {
            auto i = static_cast<std::int64_t>(c) - direct_low;
            return i < 0 || static_cast<std::size_t>(i) >= direct.size() ? -1 : direct[static_cast<std::size_t>(i)];
        }
        // The last class starting at or before c
        std::int32_t lo = 0, hi = classes.size();
        while (lo < hi) {
            auto mid = lo + (hi - lo) / 2;
            if (classes.min(mid) <= c)
                lo = mid + 1;
            else
                hi = mid;
        }
        return lo > 0 && c <= classes.max(lo - 1) ? lo - 1 : -1;
    }
};

weighted_dfa_index::weighted_dfa_index(const automaton_graph& graph) : graph_(graph) {
    const auto& g = detail::data(graph);
    if (!g.deterministic)
        throw std::invalid_argument("Deterministic epsilon-free graph required");
    auto d = std::make_shared<data>();
    d->start.reserve(g.states() + 1);
    struct entry {
        symbol min, max;
        edge_id edge;
    };
    std::vector<entry> entries;
    for (std::size_t s = 0; s < g.states(); ++s) {
        d->start.push_back(static_cast<std::uint32_t>(d->min.size()));
        entries.clear();
        for (edge_id e : g.out(static_cast<state_id>(s)))
            for (auto j = g.label_start[u(e)]; j < g.label_start[u(e) + 1]; ++j)
                entries.push_back({g.mins[j], g.maxs[j], e});
        std::ranges::sort(entries, {}, &entry::min);
        for (const auto& x : entries) {
            d->min.push_back(x.min);
            d->max.push_back(x.max);
            d->edge.push_back(x.edge);
        }
    }
    d->start.push_back(static_cast<std::uint32_t>(d->min.size()));
    // Transition table, when it holds in 16 MiB
    d->classes = detail::symbol_classes([&g](auto&& f) {
        for (std::size_t e = 0; e < g.edges(); ++e)
            f(g.labels(static_cast<edge_id>(e)));
    });
    auto classes = static_cast<std::size_t>(d->classes.size());
    if (classes > 0 && g.states() * classes <= (std::size_t{1} << 22)) {
        d->table.assign(g.states() * classes, -1);
        for (std::size_t s = 0; s < g.states(); ++s)
            for (auto i = d->start[s]; i < d->start[s + 1]; ++i) {
                d->classes.for_each_class(interval_set::range(d->min[i], d->max[i]),
                                          [&](std::int32_t c) { d->table[s * classes + static_cast<std::size_t>(c)] = d->edge[i]; });
            }
        auto low = d->classes.min(0);
        auto span = static_cast<std::int64_t>(d->classes.max(d->classes.size() - 1)) - low + 1;
        if (span <= (std::int64_t{1} << 16)) {
            d->direct_low = low;
            d->direct.assign(static_cast<std::size_t>(span), -1);
            for (std::int32_t c = 0; c < d->classes.size(); ++c)
                for (std::int64_t x = d->classes.min(c); x <= d->classes.max(c); ++x)
                    d->direct[static_cast<std::size_t>(x - low)] = c;
        }
    }
    data_ = std::move(d);
}

edge_id weighted_dfa_index::transition(state_id s, symbol c) const noexcept {
    if (s == no_state)
        return -1;
    if (!data_->table.empty()) {
        auto k = data_->class_of(c);
        return k < 0 ? -1 : data_->table[u(s) * static_cast<std::size_t>(data_->classes.size()) + u(k)];
    }
    auto lo = data_->start[u(s)], hi = data_->start[u(s) + 1];
    while (lo < hi) {
        auto mid = lo + (hi - lo) / 2;
        if (data_->max[mid] < c)
            lo = mid + 1;
        else if (data_->min[mid] > c)
            hi = mid;
        else
            return data_->edge[mid];
    }
    return -1;
}

compiled_weighted_dfa::compiled_weighted_dfa(const weighted_automaton& a) : automaton_(a), index_(a.graph()) {}

compiled_weighted_dfa::compiled_weighted_dfa(const weighted_automaton& a, weighted_dfa_index index)
    : automaton_(a), index_(std::move(index)) {
    if (!a.graph().same_as(index_.graph()))
        throw std::invalid_argument("Same graph required");
}

double compiled_weighted_dfa::cost(std::span<const symbol> w) const {
    const auto& g = data(automaton_.graph());
    const auto& wt = detail::weights(automaton_);
    state_id s = g.initial;
    if (s == no_state)
        return inf;
    // A sum overflowing the doubles stays infinite, the weights being finite : overflows are checked once, at
    // the end or at a rejection, with the result of a check at every step
    double result = wt.initial;
    const double* weight = wt.edges.data();
    const state_id* target = g.target.data();
    for (symbol c : w) {
        edge_id e = transition(s, c);
        if (e < 0) {
            if (!std::isfinite(result))
                throw std::overflow_error("Cost overflow");
            return inf;
        }
        result += weight[u(e)];
        s = target[u(e)];
    }
    if (!std::isfinite(result))
        throw std::overflow_error("Cost overflow");
    return g.accept[u(s)] ? add(result, wt.finals[u(s)]) : inf;
}

compiled_multi_weighted_dfa::compiled_multi_weighted_dfa(const multi_weighted_automaton& a) : automaton_(a), index_(a.graph()) {}

compiled_multi_weighted_dfa::compiled_multi_weighted_dfa(const multi_weighted_automaton& a, weighted_dfa_index index)
    : automaton_(a), index_(std::move(index)) {
    if (!a.graph().same_as(index_.graph()))
        throw std::invalid_argument("Same graph required");
}

compiled_weighted_dfa compiled_multi_weighted_dfa::project(std::size_t dimension) const {
    return {automaton_.project(dimension), index_};
}

compiled_weighted_dfa compiled_multi_weighted_dfa::scalarize(std::span<const double> coefficients) const {
    return {automaton_.scalarize(coefficients), index_};
}

std::optional<std::vector<double>> compiled_multi_weighted_dfa::costs(std::span<const symbol> w) const {
    std::vector<double> output(dimensions());
    if (!costs(w, output))
        return std::nullopt;
    return output;
}

bool compiled_multi_weighted_dfa::costs(std::span<const symbol> w, std::span<double> output) const {
    std::size_t d = dimensions();
    if (output.size() != d)
        throw std::invalid_argument("Expected " + std::to_string(d) + " output components");
    const auto& g = data(automaton_.graph());
    const auto& v = detail::weighted_access::vectors_of(automaton_);
    state_id s = g.initial;
    if (s == no_state) {
        std::ranges::fill(output, inf);
        return false;
    }
    std::ranges::copy(v.initial, output.begin());
    for (symbol c : w) {
        edge_id e = transition(s, c);
        if (e < 0) {
            std::ranges::fill(output, inf);
            return false;
        }
        const double* x = v.edges.data() + u(e) * d;
        for (std::size_t j = 0; j < d; ++j)
            output[j] = add(output[j], x[j]);
        s = g.target[u(e)];
    }
    if (!g.accept[u(s)]) {
        std::ranges::fill(output, inf);
        return false;
    }
    const double* f = v.finals.data() + u(s) * d;
    for (std::size_t j = 0; j < d; ++j)
        output[j] = add(output[j], f[j]);
    return true;
}

}  // namespace autolib
