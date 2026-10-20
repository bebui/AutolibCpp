// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Transformations preserving the costs of the words : epsilon removal, determinization and minimization, on
// the doubles for the integer DAGs where they are exact, and on exact integers of units otherwise.

#include <algorithm>
#include <bit>
#include <unordered_map>

#include "detail/symbol_classes.hpp"
#include "weighted/exact_costs.hpp"
#include "weighted/paths.hpp"

namespace autolib {

using detail::add;
using detail::data;
using detail::exact;
using detail::inf;
using detail::u;
using detail::weights;

namespace {

/// Combines hashes.
std::size_t mix(std::size_t h, std::size_t x) noexcept {
    return h ^ (x + std::uint64_t{0x9e3779b97f4a7c15U} + (h << 6) + (h >> 2));
}

void require_integer(double value, double bound) {
    if (value != std::rint(value) || std::fabs(value) > bound)
        throw std::invalid_argument("This transformation requires bounded integer costs for exact normalization");
}

/// A conservative condition under which every normalization, sum and difference of potentials of the DAG
/// transformations is exact in doubles.
void certify_integer_dag(const weighted_automaton& a) {
    const auto& g = data(a);
    g.require_dag();
    double n = static_cast<double>(g.states()) + 1.0, limit = 0x1.0p48 / (n * n);
    require_integer(a.initial_weight(), limit);
    for (double f : detail::weights(a).finals)
        require_integer(f, limit);
    for (double w : a.weights())
        require_integer(w, limit);
}

bool certified_integer_dag(const weighted_automaton& a) {
    try {
        certify_integer_dag(a);
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

/// The symbol classes of the edges reading symbols.
detail::symbol_classes alphabet(const detail::graph_data& g) {
    return detail::symbol_classes([&g](auto&& f) {
        for (std::size_t e = 0; e < g.edges(); ++e)
            if (!g.epsilon[e])
                f(g.labels(static_cast<edge_id>(e)));
    });
}

/// Residual costs in doubles, compared bit for bit.
struct residual_hash {
    std::size_t operator()(const std::vector<double>& v) const noexcept {
        std::size_t h = v.size();
        for (double x : v)
            h = mix(h, std::hash<std::uint64_t>{}(std::bit_cast<std::uint64_t>(x)));
        return h;
    }
};
struct residual_equal {
    bool operator()(const std::vector<double>& a, const std::vector<double>& b) const noexcept {
        return std::ranges::equal(a, b, [](double x, double y) { return std::bit_cast<std::uint64_t>(x) == std::bit_cast<std::uint64_t>(y); });
    }
};

struct exact_hash {
    std::size_t operator()(const std::vector<exact>& v) const noexcept {
        std::size_t h = v.size();
        for (const auto& x : v)
            h = mix(h, x ? x->hash() : 0x51ed27f1U);
        return h;
    }
};

/// Subtracts the least value from all : returns it, or nothing if all are absent.
exact normalize(std::vector<exact>& values) {
    const big_integer* least = nullptr;
    for (const auto& v : values)
        if (v && (least == nullptr || *v < *least))
            least = &*v;
    if (least == nullptr)
        return std::nullopt;
    exact common = *least;
    for (auto& v : values)
        if (v)
            *v -= *common;
    return common;
}

/// The edges reading the symbols of each class.
std::vector<std::vector<edge_id>> edges_by_class(const detail::graph_data& g, const detail::symbol_classes& classes) {
    std::vector<std::vector<edge_id>> result(static_cast<std::size_t>(classes.size()));
    for (std::size_t e = 0; e < g.edges(); ++e)
        if (!g.epsilon[e])
            classes.for_each_class(g.labels(static_cast<edge_id>(e)), [&](std::int32_t c) { result[u(c)].push_back(static_cast<edge_id>(e)); });
    return result;
}

}  // namespace

weighted_automaton weighted_automaton::remove_epsilon() const {
    if (!data(graph_).has_epsilon)
        return *this;
    auto a = trim();
    const auto& g = data(a);
    const auto& w = detail::weights(a);
    std::size_t n = g.states();
    if (g.initial == no_state)
        return a;
    builder only_epsilon;
    for (std::size_t s = 0; s < n; ++s)
        only_epsilon.add_state();
    for (std::size_t e = 0; e < g.edges(); ++e)
        if (g.epsilon[e])
            only_epsilon.add_epsilon(g.source[e], g.target[e], w.edges[e]);
    auto epsilon = only_epsilon.build();
    builder out;
    for (std::size_t s = 0; s < n; ++s)
        out.add_state();
    out.set_initial(g.initial, w.initial);
    for (std::size_t s = 0; s < n; ++s) {
        auto distances = shortest_paths_from(epsilon, static_cast<state_id>(s)).distances();
        double last = inf;
        for (std::size_t t = 0; t < n; ++t) {
            if (distances[t] == -inf)
                throw std::invalid_argument("Negative epsilon cycle cannot be eliminated into finite costs");
            if (g.accept[t] && std::isfinite(distances[t]))
                last = std::min(last, add(distances[t], w.finals[t]));
        }
        if (std::isfinite(last))
            out.set_accept(static_cast<state_id>(s), last);
        for (std::size_t e = 0; e < g.edges(); ++e)
            if (!g.epsilon[e] && std::isfinite(distances[u(g.source[e])]))
                out.add_transition(static_cast<state_id>(s), g.target[e], g.labels(static_cast<edge_id>(e)),
                                   add(distances[u(g.source[e])], w.edges[e]));
    }
    return out.build();
}

weighted_automaton determinize_dag(const weighted_automaton& input, std::size_t max_states) {
    if (max_states == 0)
        throw std::invalid_argument("Positive state budget required");
    if (input.graph().is_deterministic())
        return input;
    certify_integer_dag(input);
    auto a = input.remove_epsilon();
    const auto& g = data(a);
    const auto& w = detail::weights(a);
    weighted_automaton::builder out;
    if (g.initial == no_state)
        return out.build();
    auto classes = alphabet(g);
    auto by_class = edges_by_class(g, classes);
    std::size_t n = g.states();
    std::vector<std::vector<double>> queue;
    std::unordered_map<std::vector<double>, state_id, residual_hash, residual_equal> ids;
    std::vector<double> initial(n, inf);
    initial[u(g.initial)] = 0;
    ids.emplace(initial, out.add_state());
    queue.push_back(std::move(initial));
    out.set_initial(0, w.initial);
    for (std::size_t at = 0; at < queue.size(); ++at) {
        auto from = static_cast<state_id>(at);
        double terminal = inf;
        for (std::size_t s = 0; s < n; ++s)
            if (std::isfinite(queue[at][s]) && g.accept[s])
                terminal = std::min(terminal, add(queue[at][s], w.finals[s]));
        if (std::isfinite(terminal))
            out.set_accept(from, terminal);
        for (std::int32_t c = 0; c < classes.size(); ++c) {
            std::vector<double> next(n, inf);
            const auto& current = queue[at];
            for (edge_id e : by_class[u(c)])
                if (std::isfinite(current[u(g.source[u(e)])]))
                    next[u(g.target[u(e)])] = std::min(next[u(g.target[u(e)])], add(current[u(g.source[u(e)])], w.edges[u(e)]));
            double common = std::ranges::min(next);
            if (!std::isfinite(common))
                continue;
            for (double& x : next)
                if (std::isfinite(x))
                    x = add(x, -common);
            auto found = ids.find(next);
            state_id to;
            if (found == ids.end()) {
                if (queue.size() == max_states)
                    throw std::length_error("Determinization state budget exhausted; no complete result");
                to = out.add_state();
                ids.emplace(next, to);
                queue.push_back(std::move(next));
            } else {
                to = found->second;
            }
            out.add_transition(from, to, interval{classes.min(c), classes.max(c)}, common);
        }
    }
    return out.build().simplify();
}

namespace {

struct segment {
    symbol min, max;
    state_id target;
    double weight;
};

struct signature {
    bool accept;
    double terminal;
    std::vector<segment> edges;

    bool operator==(const signature& o) const noexcept {
        auto same = [](double x, double y) { return std::bit_cast<std::uint64_t>(x) == std::bit_cast<std::uint64_t>(y); };
        return accept == o.accept && same(terminal, o.terminal) &&
               std::ranges::equal(edges, o.edges, [&](const segment& x, const segment& y) {
                   return x.min == y.min && x.max == y.max && x.target == y.target && same(x.weight, y.weight);
               });
    }
};

struct signature_hash {
    std::size_t operator()(const signature& s) const noexcept {
        std::size_t h = mix(s.accept ? 1 : 0, std::bit_cast<std::uint64_t>(s.terminal));
        for (const auto& e : s.edges)
            h = mix(mix(mix(mix(h, static_cast<std::size_t>(e.min)), static_cast<std::size_t>(e.max)), static_cast<std::size_t>(e.target)),
                    std::bit_cast<std::uint64_t>(e.weight));
        return h;
    }
};

}  // namespace

weighted_automaton minimize_dag(const weighted_automaton& input) {
    if (!input.graph().is_deterministic())
        throw std::invalid_argument("Deterministic weighting required");
    certify_integer_dag(input);
    auto a = input.trim();
    const auto& g = data(a);
    const auto& w = detail::weights(a);
    if (g.initial == no_state)
        return a;
    auto potential = backward_distances(a);
    std::vector<state_id> group(g.states());
    weighted_automaton::builder out;
    std::unordered_map<signature, state_id, signature_hash> groups;
    for (std::size_t k = g.order.size(); k-- > 0;) {
        auto s = g.order[k];
        std::vector<segment> segments;
        for (edge_id e : g.out(s)) {
            double weight = add(add(w.edges[u(e)], potential[u(g.target[u(e)])]), -potential[u(s)]);
            for (auto j = g.label_start[u(e)]; j < g.label_start[u(e) + 1]; ++j)
                segments.push_back({g.mins[j], g.maxs[j], group[u(g.target[u(e)])], weight});
        }
        std::ranges::stable_sort(segments, {}, &segment::min);
        std::vector<segment> merged;
        for (const auto& edge : segments) {
            if (!merged.empty()) {
                auto& last = merged.back();
                if (static_cast<std::int64_t>(last.max) + 1 == edge.min && last.target == edge.target && last.weight == edge.weight) {
                    last.max = edge.max;
                    continue;
                }
            }
            merged.push_back(edge);
        }
        bool accept = g.accept[u(s)] != 0;
        double terminal = accept ? add(w.finals[u(s)], -potential[u(s)]) : 0;
        signature sig{accept, terminal, std::move(merged)};
        auto found = groups.find(sig);
        state_id id;
        if (found == groups.end()) {
            id = out.add_state();
            if (accept)
                out.set_accept(id, terminal);
            for (const auto& e : sig.edges)
                out.add_transition(id, e.target, interval{e.min, e.max}, e.weight);
            groups.emplace(std::move(sig), id);
        } else {
            id = found->second;
        }
        group[u(s)] = id;
    }
    out.set_initial(group[u(g.initial)], add(w.initial, potential[u(g.initial)]));
    return out.build();
}

weighted_automaton weighted_automaton::determinize(std::size_t max_states) const {
    if (max_states == 0)
        throw std::invalid_argument("Positive state budget required");
    if (graph_.is_deterministic())
        return *this;
    // The DAGs of bounded integers keep the faster construction on doubles
    if (graph_.is_acyclic() && certified_integer_dag(*this))
        return determinize_dag(*this, max_states);
    auto a = trim();
    const auto& g = data(a);
    builder out;
    if (g.initial == no_state)
        return out.build();
    detail::exact_costs costs(a);
    std::size_t n = g.states();
    auto classes = alphabet(g);
    auto by_class = edges_by_class(g, classes);
    std::vector<exact> initial(n);
    initial[u(g.initial)] = big_integer();
    costs.close_epsilon(initial);
    auto common = normalize(initial);
    // The residuals are stored once, as the keys of the map; the queue points to them
    std::unordered_map<std::vector<exact>, state_id, exact_hash> ids;
    std::vector<const std::vector<exact>*> queue;
    queue.push_back(&ids.emplace(std::move(initial), out.add_state()).first->first);
    out.set_initial(0, costs.scale.representable(costs.initial + *common));
    std::vector<exact> next(n);
    for (std::size_t at = 0; at < queue.size(); ++at) {
        auto from = static_cast<state_id>(at);
        const auto& current = *queue[at];
        exact terminal;
        for (std::size_t s = 0; s < n; ++s)
            if (current[s] && costs.finals[s]) {
                big_integer value = *current[s] + *costs.finals[s];
                if (detail::improves(value, terminal))
                    terminal = std::move(value);
            }
        if (terminal)
            out.set_accept(from, costs.scale.representable(*terminal));
        for (std::int32_t c = 0; c < classes.size(); ++c) {
            next.assign(n, std::nullopt);
            for (edge_id e : by_class[u(c)])
                if (const auto& from_value = current[u(g.source[u(e)])]) {
                    big_integer value = *from_value + costs.edges[u(e)];
                    auto t = u(g.target[u(e)]);
                    if (detail::improves(value, next[t]))
                        next[t] = std::move(value);
                }
            if (g.has_epsilon)
                costs.close_epsilon(next);
            common = normalize(next);
            if (!common)
                continue;
            auto found = ids.find(next);
            state_id to;
            if (found == ids.end()) {
                if (queue.size() == max_states)
                    throw std::length_error("Determinization state budget exhausted; no complete result");
                to = out.add_state();
                queue.push_back(&ids.emplace(next, to).first->first);
            } else {
                to = found->second;
            }
            out.add_transition(from, to, interval{classes.min(c), classes.max(c)}, costs.scale.representable(*common));
        }
    }
    return out.build().simplify();
}

namespace {

struct exact_segment {
    symbol min, max;
    std::int32_t target;
    big_integer weight;
    bool operator==(const exact_segment&) const = default;
};

struct exact_signature {
    exact terminal;
    std::vector<exact_segment> edges;
    bool operator==(const exact_signature&) const = default;
};

struct exact_signature_hash {
    std::size_t operator()(const exact_signature& s) const noexcept {
        std::size_t h = s.terminal ? s.terminal->hash() : 0x2545f491U;
        for (const auto& e : s.edges)
            h = mix(mix(mix(mix(h, static_cast<std::size_t>(e.min)), static_cast<std::size_t>(e.max)), static_cast<std::size_t>(e.target)),
                    e.weight.hash());
        return h;
    }
};

exact_signature signature_of(const detail::graph_data& g, state_id s, const std::vector<std::int32_t>& groups,
                             const std::vector<big_integer>& weights, const exact& terminal) {
    std::vector<exact_segment> segments;
    for (edge_id e : g.out(s))
        for (auto j = g.label_start[u(e)]; j < g.label_start[u(e) + 1]; ++j)
            segments.push_back({g.mins[j], g.maxs[j], groups[u(g.target[u(e)])], weights[u(e)]});
    std::ranges::stable_sort(segments, {}, &exact_segment::min);
    std::vector<exact_segment> merged;
    for (auto& edge : segments) {
        if (!merged.empty()) {
            auto& last = merged.back();
            if (static_cast<std::int64_t>(last.max) + 1 == edge.min && last.target == edge.target && last.weight == edge.weight) {
                last.max = edge.max;
                continue;
            }
        }
        merged.push_back(std::move(edge));
    }
    return {terminal, std::move(merged)};
}

}  // namespace

weighted_automaton weighted_automaton::minimize() const {
    if (!graph_.is_deterministic())
        throw std::invalid_argument("Deterministic weighting required");
    if (graph_.is_acyclic() && certified_integer_dag(*this))
        return minimize_dag(*this);
    auto a = trim();
    const auto& g = data(a);
    if (g.initial == no_state)
        return a;
    detail::exact_costs costs(a);
    std::size_t n = g.states();
    // Potentials : costs of the canonical accepted suffixes, the shortest then the least symbol first
    std::vector<int> depth(n, -1);
    std::vector<state_id> queue;
    for (std::size_t s = 0; s < n; ++s)
        if (g.accept[s]) {
            depth[s] = 0;
            queue.push_back(static_cast<state_id>(s));
        }
    for (std::size_t head = 0; head < queue.size(); ++head) {
        auto t = queue[head];
        for (edge_id e : g.in(t)) {
            auto s = u(g.source[u(e)]);
            if (depth[s] < 0) {
                depth[s] = depth[u(t)] + 1;
                queue.push_back(static_cast<state_id>(s));
            }
        }
    }
    std::vector<big_integer> potential(n);
    for (state_id s : queue) {
        if (g.accept[u(s)]) {
            potential[u(s)] = *costs.finals[u(s)];
            continue;
        }
        edge_id chosen = -1;
        for (edge_id e : g.out(s))
            if (depth[u(g.target[u(e)])] == depth[u(s)] - 1 && (chosen < 0 || g.representative(e) < g.representative(chosen)))
                chosen = e;
        potential[u(s)] = costs.edges[u(chosen)] + potential[u(g.target[u(chosen)])];
    }
    std::vector<big_integer> normalized(costs.edges.size());
    for (std::size_t e = 0; e < normalized.size(); ++e)
        normalized[e] = costs.edges[e] + potential[u(g.target[e])] - potential[u(g.source[e])];
    std::vector<exact> terminal(n);
    for (std::size_t s = 0; s < n; ++s)
        if (g.accept[s])
            terminal[s] = *costs.finals[s] - potential[s];
    // Refinement of the groups of equal signatures, to a fixed point
    std::vector<std::int32_t> groups(n, 0);
    std::int32_t count;
    while (true) {
        std::unordered_map<exact_signature, std::int32_t, exact_signature_hash> ids;
        std::vector<std::int32_t> next(n);
        for (std::size_t s = 0; s < n; ++s) {
            auto [it, fresh] = ids.try_emplace(signature_of(g, static_cast<state_id>(s), groups, normalized, terminal[s]),
                                               static_cast<std::int32_t>(ids.size()));
            next[s] = it->second;
        }
        count = static_cast<std::int32_t>(ids.size());
        bool stable = groups == next;
        groups = std::move(next);
        if (stable)
            break;
    }
    builder out;
    for (std::int32_t i = 0; i < count; ++i)
        out.add_state();
    out.set_initial(groups[u(g.initial)], costs.scale.representable(costs.initial + potential[u(g.initial)]));
    std::vector<bool> emitted(u(count));
    for (std::size_t s = 0; s < n; ++s) {
        auto id = groups[s];
        if (emitted[u(id)])
            continue;
        emitted[u(id)] = true;
        auto sig = signature_of(g, static_cast<state_id>(s), groups, normalized, terminal[s]);
        if (sig.terminal)
            out.set_accept(id, costs.scale.representable(*sig.terminal));
        for (const auto& edge : sig.edges)
            out.add_transition(id, edge.target, interval{edge.min, edge.max}, costs.scale.representable(edge.weight));
    }
    return out.build();
}

}  // namespace autolib
