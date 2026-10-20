// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <algorithm>
#include <bit>
#include <limits>
#include <unordered_map>

#include "autolib/compiled_weighted_dfa.hpp"
#include "autolib/layered_automaton.hpp"
#include "weighted/common.hpp"
#include "weighted/layered_data.hpp"

namespace autolib {

using detail::add;
using detail::data;
using detail::u;
using access = detail::weighted_access;

namespace {

const access::vectors& vectors_of(const multi_weighted_automaton& a) { return access::vectors_of(a); }

std::size_t positive(std::size_t d) {
    if (d == 0)
        throw std::invalid_argument("Positive dimension required");
    return d;
}

std::vector<double> zeros_or(std::vector<double> values, std::size_t size) {
    return values.empty() ? std::vector<double>(size, 0.0) : detail::copy_weights(std::move(values), size);
}

void same_dimensions(const multi_weighted_automaton& a, const multi_weighted_automaton& b) {
    if (a.dimensions() != b.dimensions())
        throw std::invalid_argument("Equal dimensions required for componentwise addition");
}

/// Componentwise checked sum.
std::vector<double> sum(std::span<const double> a, std::span<const double> b) {
    std::vector<double> r(a.size());
    for (std::size_t d = 0; d < r.size(); ++d)
        r[d] = add(a[d], b[d]);
    return r;
}

std::vector<state_id> copy(const multi_weighted_automaton& a, multi_weighted_automaton::builder& b, bool finals) {
    const auto& g = data(a.graph());
    std::vector<state_id> map(g.states());
    for (std::size_t s = 0; s < map.size(); ++s) {
        map[s] = b.add_state();
        if (finals && g.accept[s])
            b.set_accept(map[s], a.final_weights(static_cast<state_id>(s)));
    }
    for (std::size_t e = 0; e < g.edges(); ++e) {
        auto from = map[u(g.source[e])], to = map[u(g.target[e])];
        if (g.epsilon[e])
            b.add_epsilon(from, to, a.weights(static_cast<edge_id>(e)));
        else
            b.add_transition(from, to, g.labels(static_cast<edge_id>(e)), a.weights(static_cast<edge_id>(e)));
    }
    return map;
}

/// The vectors of an automaton placed at an offset among more dimensions, the others being zero.
multi_weighted_automaton embed(const multi_weighted_automaton& a, std::size_t dimensions, std::size_t offset) {
    const auto& v = vectors_of(a);
    std::size_t d = v.dimensions;
    access::vectors out;
    out.dimensions = dimensions;
    auto spread = [&](const std::vector<double>& from, std::vector<double>& to) {
        std::size_t rows = from.size() / d;
        to.assign(rows * dimensions, 0.0);
        for (std::size_t r = 0; r < rows; ++r)
            std::copy_n(from.begin() + static_cast<std::ptrdiff_t>(r * d), d, to.begin() + static_cast<std::ptrdiff_t>(r * dimensions + offset));
    };
    spread(v.edges, out.edges);
    spread(v.initial, out.initial);
    spread(v.finals, out.finals);
    return access::make(a.graph(), std::move(out));
}

/// Whether the sums of the propagations are exact in doubles.
bool exact_double_sums(const multi_weighted_automaton& a) {
    const auto& g = data(a.graph());
    const auto& v = vectors_of(a);
    detail::exactness certificate;
    for (std::size_t d = 0; d < v.dimensions; ++d) {
        certificate.include(v.initial[d]);
        for (std::size_t s = 0; s < g.states(); ++s)
            if (g.accept[s])
                certificate.include(v.finals[s * v.dimensions + d]);
        for (std::size_t e = 0; e < g.edges(); ++e)
            certificate.include(v.edges[e * v.dimensions + d]);
    }
    return certificate.safe(static_cast<double>(g.states()) + 1.0);
}

double rounded(const dyadic& value) {
    double result = value.to_double();
    if (!std::isfinite(result))
        throw std::overflow_error("Cost overflow");
    return result == 0 ? 0 : result;
}

/// The partial paths of the label propagations, in an arena : each label has a parent, and its costs, exact
/// when doubles could round, stored contiguously.
class label_arena {
public:
    explicit label_arena(const multi_weighted_automaton& a)
        : a_(a), g_(data(a.graph())), d_(a.dimensions()), exact_(!exact_double_sums(a)) {}

    std::uint32_t root() {
        auto id = push({none, -1, g_.initial});
        auto initial = a_.initial_weights();
        std::ranges::copy(initial, costs_.end() - static_cast<std::ptrdiff_t>(d_));
        if (exact_)
            for (std::size_t k = 0; k < d_; ++k)
                exact_costs_[u(id) * d_ + k] = dyadic(initial[k]);
        return id;
    }

    /// A label extended by an edge, or by the final costs of its state when `edge` is negative.
    std::uint32_t extend(std::uint32_t prefix, edge_id edge) {
        node n = edge >= 0 ? node{prefix, edge, g_.target[u(edge)]} : nodes_[prefix];
        auto costs = edge >= 0 ? a_.weights(edge) : a_.final_weights(nodes_[prefix].state);
        auto id = push(n);
        for (std::size_t k = 0; k < d_; ++k) {
            auto from = u(prefix) * d_ + k, to = u(id) * d_ + k;
            if (exact_) {
                exact_costs_[to] = exact_costs_[from] + dyadic(costs[k]);
                costs_[to] = rounded(exact_costs_[to]);
            } else {
                costs_[to] = detail::add(costs_[from], costs[k]);
            }
        }
        return id;
    }

    [[nodiscard]] int compare(std::uint32_t x, std::uint32_t y, std::span<const std::size_t> order) const {
        for (auto k : order)
            if (int c = component(x, y, k); c != 0)
                return c;
        return 0;
    }

    /// -1 if x dominates y, 1 if y dominates x, 0 if equal, 2 if incomparable.
    [[nodiscard]] int relation(std::uint32_t x, std::uint32_t y) const {
        bool less = false, more = false;
        for (std::size_t k = 0; k < d_; ++k) {
            int c = component(x, y, k);
            less |= c < 0;
            more |= c > 0;
            if (less && more)
                return 2;
        }
        return less ? -1 : more ? 1 : 0;
    }

    void insert(std::vector<std::uint32_t>& front, std::uint32_t value, bool all_paths) const {
        for (auto old : front) {
            int r = relation(old, value);
            if (r == -1 || (!all_paths && r == 0))
                return;
        }
        std::erase_if(front, [&](std::uint32_t old) { return relation(value, old) == -1; });
        front.push_back(value);
    }

    [[nodiscard]] multi_weighted_path path(std::uint32_t l) const {
        std::vector<edge_id> edges;
        word w;
        for (auto p = l; nodes_[p].parent != none; p = nodes_[p].parent) {
            edges.push_back(nodes_[p].edge);
            if (!g_.epsilon[u(nodes_[p].edge)])
                w.push_back(g_.representative(nodes_[p].edge));
        }
        std::ranges::reverse(edges);
        std::ranges::reverse(w);
        auto first = costs_.begin() + static_cast<std::ptrdiff_t>(u(l) * d_);
        std::vector<dyadic> exact;
        if (exact_)
            exact.assign(exact_costs_.begin() + static_cast<std::ptrdiff_t>(u(l) * d_),
                         exact_costs_.begin() + static_cast<std::ptrdiff_t>((u(l) + 1) * d_));
        return {weighted_path(a_.graph(), g_.initial, std::move(edges), std::move(w), 0),
                {first, first + static_cast<std::ptrdiff_t>(d_)}, std::move(exact)};
    }

private:
    static constexpr std::uint32_t none = std::numeric_limits<std::uint32_t>::max();
    struct node {
        std::uint32_t parent;
        edge_id edge;
        state_id state;
    };

    std::uint32_t push(node n) {
        if (nodes_.size() == none)
            throw std::length_error("Too many labels");
        nodes_.push_back(n);
        costs_.resize(costs_.size() + d_);
        if (exact_)
            exact_costs_.resize(exact_costs_.size() + d_);
        return static_cast<std::uint32_t>(nodes_.size() - 1);
    }

    [[nodiscard]] int component(std::uint32_t x, std::uint32_t y, std::size_t k) const {
        if (exact_) {
            auto c = exact_costs_[u(x) * d_ + k] <=> exact_costs_[u(y) * d_ + k];
            return c < 0 ? -1 : c > 0 ? 1 : 0;
        }
        double a = costs_[u(x) * d_ + k], b = costs_[u(y) * d_ + k];
        return a < b ? -1 : a > b ? 1 : 0;
    }

    const multi_weighted_automaton& a_;
    const detail::graph_data& g_;
    std::size_t d_;
    bool exact_;
    std::vector<node> nodes_;
    std::vector<double> costs_;
    std::vector<dyadic> exact_costs_;
};

}  // namespace

// multi_weighted_path

multi_weighted_path::multi_weighted_path(weighted_path path, std::vector<double> costs, std::vector<dyadic> exact)
    : path_(std::move(path)), costs_(std::move(costs)), exact_(std::move(exact)) {}

dyadic multi_weighted_path::exact_cost(std::size_t dimension) const {
    if (dimension >= costs_.size())
        throw std::out_of_range("Dimension " + std::to_string(dimension));
    return exact_.empty() ? dyadic(costs_[dimension]) : exact_[dimension];
}

bool dominates(std::span<const double> x, std::span<const double> y) {
    if (x.size() != y.size() || x.empty())
        throw std::invalid_argument("Equal positive dimensions required");
    for (std::size_t d = 0; d < x.size(); ++d) {
        (void)detail::finite(x[d]);
        (void)detail::finite(y[d]);
    }
    bool strict = false;
    for (std::size_t d = 0; d < x.size(); ++d) {
        if (x[d] > y[d])
            return false;
        strict |= x[d] < y[d];
    }
    return strict;
}

// Construction

multi_weighted_automaton multi_weighted_automaton::of(automaton_graph graph, std::size_t dimensions, std::vector<double> edge_weights,
                                                      std::vector<double> initial, std::vector<double> finals) {
    positive(dimensions);
    vectors v;
    v.dimensions = dimensions;
    v.edges = detail::copy_weights(std::move(edge_weights), graph.edge_count() * dimensions);
    v.initial = zeros_or(std::move(initial), dimensions);
    v.finals = zeros_or(std::move(finals), graph.state_count() * dimensions);
    return {std::move(graph), std::make_shared<const vectors>(std::move(v))};
}

multi_weighted_automaton multi_weighted_automaton::from_weightings(std::span<const weighted_automaton> weightings) {
    if (weightings.empty())
        throw std::invalid_argument("At least one weighting required");
    const auto& graph = weightings[0].graph();
    for (const auto& a : weightings)
        if (!a.graph().same_as(graph))
            throw std::invalid_argument("Identical shared graph required; use synchronized_product for independent graphs");
    std::size_t d = weightings.size(), m = graph.edge_count(), n = graph.state_count();
    vectors v;
    v.dimensions = d;
    v.edges.resize(m * d);
    v.initial.resize(d);
    v.finals.resize(n * d);
    for (std::size_t k = 0; k < d; ++k) {
        const auto& w = detail::weights(weightings[k]);
        for (std::size_t e = 0; e < m; ++e)
            v.edges[e * d + k] = w.edges[e];
        v.initial[k] = w.initial;
        for (std::size_t s = 0; s < n; ++s)
            v.finals[s * d + k] = w.finals[s];
    }
    return {graph, std::make_shared<const vectors>(std::move(v))};
}

multi_weighted_automaton multi_weighted_automaton::synchronized_product(std::span<const weighted_automaton> automata) {
    if (automata.empty())
        throw std::invalid_argument("At least one operand required");
    auto result = from_weightings(automata.subspan(0, 1));
    for (std::size_t i = 1; i < automata.size(); ++i)
        result = result.product_independent(from_weightings(automata.subspan(i, 1)));
    return result;
}

std::size_t multi_weighted_automaton::dimensions() const noexcept { return vectors_->dimensions; }

namespace {
void check_dimension(std::size_t d, std::size_t dimensions) {
    if (d >= dimensions)
        throw std::out_of_range("Dimension " + std::to_string(d));
}
}  // namespace

double multi_weighted_automaton::weight(edge_id e, std::size_t dimension) const {
    check_dimension(dimension, dimensions());
    data(graph_).check_edge(e);
    return vectors_->edges[u(e) * dimensions() + dimension];
}

std::span<const double> multi_weighted_automaton::weights(edge_id e) const {
    data(graph_).check_edge(e);
    return {vectors_->edges.data() + u(e) * dimensions(), dimensions()};
}

double multi_weighted_automaton::initial_weight(std::size_t dimension) const {
    check_dimension(dimension, dimensions());
    return vectors_->initial[dimension];
}

std::span<const double> multi_weighted_automaton::initial_weights() const noexcept { return vectors_->initial; }

double multi_weighted_automaton::final_weight(state_id s, std::size_t dimension) const {
    check_dimension(dimension, dimensions());
    data(graph_).check_state(s);
    return vectors_->finals[u(s) * dimensions() + dimension];
}

std::span<const double> multi_weighted_automaton::final_weights(state_id s) const {
    data(graph_).check_state(s);
    return {vectors_->finals.data() + u(s) * dimensions(), dimensions()};
}

// Views

weighted_automaton multi_weighted_automaton::project(std::size_t dimension) const {
    check_dimension(dimension, dimensions());
    std::size_t d = dimensions();
    access::weights w;
    for (std::size_t e = 0; e < graph_.edge_count(); ++e)
        w.edges.push_back(vectors_->edges[e * d + dimension]);
    w.initial = vectors_->initial[dimension];
    for (std::size_t s = 0; s < graph_.state_count(); ++s)
        w.finals.push_back(vectors_->finals[s * d + dimension]);
    return access::make(graph_, std::move(w));
}

multi_weighted_automaton multi_weighted_automaton::select_dimensions(std::span<const std::size_t> selected) const {
    positive(selected.size());
    for (auto d : selected)
        check_dimension(d, dimensions());
    std::size_t d = dimensions(), k = selected.size();
    vectors v;
    v.dimensions = k;
    auto pick = [&](const std::vector<double>& from, std::vector<double>& to) {
        std::size_t rows = from.size() / d;
        to.resize(rows * k);
        for (std::size_t r = 0; r < rows; ++r)
            for (std::size_t i = 0; i < k; ++i)
                to[r * k + i] = from[r * d + selected[i]];
    };
    pick(vectors_->edges, v.edges);
    pick(vectors_->initial, v.initial);
    pick(vectors_->finals, v.finals);
    return {graph_, std::make_shared<const vectors>(std::move(v))};
}

weighted_automaton multi_weighted_automaton::scalarize(std::span<const double> coefficients) const {
    auto c = detail::copy_weights({coefficients.begin(), coefficients.end()}, dimensions());
    std::size_t d = dimensions();
    auto combine = [&](const double* v) {
        double total = 0;
        for (std::size_t k = 0; k < d; ++k)
            total = add(total, detail::multiply(c[k], v[k]));
        return total;
    };
    access::weights w;
    for (std::size_t e = 0; e < graph_.edge_count(); ++e)
        w.edges.push_back(combine(vectors_->edges.data() + e * d));
    w.initial = combine(vectors_->initial.data());
    for (std::size_t s = 0; s < graph_.state_count(); ++s)
        w.finals.push_back(combine(vectors_->finals.data() + s * d));
    return access::make(graph_, std::move(w));
}

std::vector<double> multi_weighted_automaton::path_costs(const weighted_path& path) const {
    const auto& g = data(graph_);
    if (!path.graph().same_as(graph_) || path.start() != g.initial || !g.accept[u(path.end())])
        throw std::invalid_argument("Accepting path on this graph required");
    std::size_t d = dimensions();
    auto finals = final_weights(path.end());
    detail::exactness certificate;
    for (std::size_t k = 0; k < d; ++k) {
        certificate.include(vectors_->initial[k]);
        certificate.include(finals[k]);
        for (edge_id e : path.edges())
            certificate.include(vectors_->edges[u(e) * d + k]);
    }
    std::vector<double> result(d);
    if (!certificate.safe(static_cast<double>(path.edge_count()) + 2.0)) {
        for (std::size_t k = 0; k < d; ++k) {
            dyadic value(vectors_->initial[k]);
            for (edge_id e : path.edges())
                value += dyadic(vectors_->edges[u(e) * d + k]);
            value += dyadic(finals[k]);
            result[k] = value.to_double();
            if (!std::isfinite(result[k]))
                throw std::overflow_error("Cost overflow");
        }
        return result;
    }
    for (std::size_t k = 0; k < d; ++k) {
        result[k] = vectors_->initial[k];
        for (edge_id e : path.edges())
            result[k] = add(result[k], vectors_->edges[u(e) * d + k]);
        result[k] = add(result[k], finals[k]);
    }
    return result;
}

// Optimization on DAGs

std::optional<multi_weighted_path> multi_weighted_automaton::lexicographic_best_path(std::span<const std::size_t> priority) const {
    const auto& g = data(graph_);
    g.require_dag();
    std::size_t d = dimensions();
    std::vector<std::size_t> order(priority.begin(), priority.end());
    if (order.empty())
        for (std::size_t i = 0; i < d; ++i)
            order.push_back(i);
    if (order.size() != d)
        throw std::invalid_argument("A permutation of every dimension is required");
    std::vector<bool> seen(d);
    for (auto i : order) {
        if (i >= d || seen[i])
            throw std::invalid_argument("Invalid dimension permutation");
        seen[i] = true;
    }
    if (g.initial == no_state)
        return std::nullopt;
    label_arena labels(*this);
    constexpr std::uint32_t none = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> best(g.states(), none);
    best[u(g.initial)] = labels.root();
    for (state_id s : g.order) {
        if (best[u(s)] == none)
            continue;
        for (edge_id e : g.out(s)) {
            auto t = u(g.target[u(e)]);
            auto v = labels.extend(best[u(s)], e);
            if (best[t] == none || labels.compare(v, best[t], order) < 0)
                best[t] = v;
        }
    }
    std::uint32_t selected = none;
    for (std::size_t s = 0; s < best.size(); ++s)
        if (g.accept[s] && best[s] != none) {
            auto v = labels.extend(best[s], -1);
            if (selected == none || labels.compare(v, selected, order) < 0)
                selected = v;
        }
    if (selected == none)
        return std::nullopt;
    return labels.path(selected);
}

pareto_result multi_weighted_automaton::pareto_front(bool all_paths) const {
    const auto& g = data(graph_);
    g.require_dag();
    if (g.initial == no_state)
        return {{}, all_paths};
    label_arena labels(*this);
    std::vector<std::vector<std::uint32_t>> fronts(g.states());
    fronts[u(g.initial)].push_back(labels.root());
    for (state_id s : g.order)
        for (std::size_t i = 0; i < fronts[u(s)].size(); ++i) {
            auto l = fronts[u(s)][i];
            for (edge_id e : g.out(s))
                labels.insert(fronts[u(g.target[u(e)])], labels.extend(l, e), all_paths);
        }
    std::vector<std::uint32_t> accepted;
    for (std::size_t s = 0; s < g.states(); ++s)
        if (g.accept[s])
            for (auto l : fronts[s])
                labels.insert(accepted, labels.extend(l, -1), all_paths);
    std::vector<multi_weighted_path> paths;
    for (auto l : accepted)
        paths.push_back(labels.path(l));
    return {std::move(paths), all_paths};
}

pareto_result multi_weighted_automaton::pareto_for_word(std::span<const symbol> w) const {
    auto layers = layered_automaton::for_word(graph_, w);
    auto front = layers.evaluate(*this).pareto_front();
    std::vector<multi_weighted_path> paths;
    for (const auto& p : front.paths())
        paths.push_back(layers.map(p));
    return {std::move(paths), false};
}

// Operations

multi_weighted_automaton multi_weighted_automaton::union_with(const multi_weighted_automaton& other) const {
    same_dimensions(*this, other);
    builder out(dimensions());
    auto root = out.add_state();
    out.set_initial(root);
    auto left = copy(*this, out, true), right = copy(other, out, true);
    if (graph_.initial() != no_state)
        out.add_epsilon(root, left[u(graph_.initial())], initial_weights());
    if (other.graph_.initial() != no_state)
        out.add_epsilon(root, right[u(other.graph_.initial())], other.initial_weights());
    return out.build();
}

multi_weighted_automaton multi_weighted_automaton::concatenate(const multi_weighted_automaton& other) const {
    same_dimensions(*this, other);
    builder out(dimensions());
    auto left = copy(*this, out, false), right = copy(other, out, true);
    if (graph_.initial() != no_state && other.graph_.initial() != no_state) {
        out.set_initial(left[u(graph_.initial())], initial_weights());
        const auto& g = data(graph_);
        for (std::size_t s = 0; s < left.size(); ++s)
            if (g.accept[s])
                out.add_epsilon(left[s], right[u(other.graph_.initial())],
                                sum(final_weights(static_cast<state_id>(s)), other.initial_weights()));
    }
    return out.build();
}

multi_weighted_automaton multi_weighted_automaton::reverse() const {
    const auto& g = data(graph_);
    builder out(dimensions());
    if (g.initial == no_state)
        return out.build();
    for (std::size_t s = 0; s < g.states(); ++s)
        out.add_state();
    auto root = out.add_state();
    out.set_initial(root);
    out.set_accept(g.initial, initial_weights());
    for (std::size_t s = 0; s < g.states(); ++s)
        if (g.accept[s])
            out.add_epsilon(root, static_cast<state_id>(s), final_weights(static_cast<state_id>(s)));
    for (std::size_t e = 0; e < g.edges(); ++e) {
        auto w = weights(static_cast<edge_id>(e));
        if (g.epsilon[e])
            out.add_epsilon(g.target[e], g.source[e], w);
        else
            out.add_transition(g.target[e], g.source[e], g.labels(static_cast<edge_id>(e)), w);
    }
    return out.build();
}

multi_weighted_automaton multi_weighted_automaton::intersect_with(const multi_weighted_automaton& other) const {
    same_dimensions(*this, other);
    const auto& ga = data(graph_);
    const auto& gb = data(other.graph_);
    builder out(dimensions());
    if (ga.initial == no_state || gb.initial == no_state)
        return out.build();
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
    out.set_initial(0, sum(initial_weights(), other.initial_weights()));
    for (std::size_t at = 0; at < queue.size(); ++at) {
        auto p = queue[at];
        auto from = static_cast<state_id>(at);
        if (ga.accept[u(p.left)] && gb.accept[u(p.right)])
            out.set_accept(from, sum(final_weights(p.left), other.final_weights(p.right)));
        for (edge_id e : ga.out(p.left)) {
            if (ga.epsilon[u(e)]) {
                if (p.phase != 2)
                    out.add_epsilon(from, state_of(ga.target[u(e)], p.right, 1), weights(e));
                continue;
            }
            auto le = ga.labels(e);
            for (edge_id f : gb.out(p.right)) {
                if (gb.epsilon[u(f)])
                    continue;
                auto common = le & gb.labels(f);
                if (!common.empty())
                    out.add_transition(from, state_of(ga.target[u(e)], gb.target[u(f)], 0), common, sum(weights(e), other.weights(f)));
            }
        }
        for (edge_id f : gb.out(p.right))
            if (gb.epsilon[u(f)])
                out.add_epsilon(from, state_of(p.left, gb.target[u(f)], 2), other.weights(f));
    }
    return out.build();
}

multi_weighted_automaton multi_weighted_automaton::product_independent(const multi_weighted_automaton& other) const {
    auto d = dimensions() + other.dimensions();
    return embed(*this, d, 0).intersect_with(embed(other, d, dimensions()));
}

multi_weighted_automaton multi_weighted_automaton::concatenate_independent(const multi_weighted_automaton& other) const {
    auto d = dimensions() + other.dimensions();
    return embed(*this, d, 0).concatenate(embed(other, d, dimensions()));
}

multi_weighted_automaton multi_weighted_automaton::trim() const {
    const auto& g = data(graph_);
    auto useful = g.useful();
    builder out(dimensions());
    std::vector<state_id> map(g.states(), no_state);
    for (std::size_t s = 0; s < map.size(); ++s)
        if (useful[s]) {
            map[s] = out.add_state();
            if (g.accept[s])
                out.set_accept(map[s], final_weights(static_cast<state_id>(s)));
        }
    if (g.initial != no_state && useful[u(g.initial)])
        out.set_initial(map[u(g.initial)], initial_weights());
    for (std::size_t e = 0; e < g.edges(); ++e) {
        auto s = u(g.source[e]), t = u(g.target[e]);
        if (!useful[s] || !useful[t])
            continue;
        if (g.epsilon[e])
            out.add_epsilon(map[s], map[t], weights(static_cast<edge_id>(e)));
        else
            out.add_transition(map[s], map[t], g.labels(static_cast<edge_id>(e)), weights(static_cast<edge_id>(e)));
    }
    return out.build();
}

multi_weighted_automaton multi_weighted_automaton::simplify() const {
    auto a = trim();
    const auto& g = data(a.graph_);
    struct merge {
        state_id from, to;
        bool epsilon;
        std::span<const double> vector;
        interval_set labels;
    };
    auto same_vector = [](std::span<const double> x, std::span<const double> y) {
        return std::ranges::equal(x, y, [](double p, double q) { return std::bit_cast<std::uint64_t>(p) == std::bit_cast<std::uint64_t>(q); });
    };
    std::vector<merge> groups;
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> index;
    for (std::size_t e = 0; e < g.edges(); ++e) {
        auto v = a.weights(static_cast<edge_id>(e));
        std::uint64_t key = (static_cast<std::uint64_t>(g.source[e]) * std::uint64_t{0x9e3779b97f4a7c15U}) ^ (static_cast<std::uint64_t>(g.target[e]) << 1) ^
                            static_cast<std::uint64_t>(g.epsilon[e]);
        for (double x : v)
            key = key * 31U + std::bit_cast<std::uint64_t>(x);
        auto& candidates = index[key];
        std::size_t found = groups.size();
        for (std::size_t i : candidates)
            if (groups[i].from == g.source[e] && groups[i].to == g.target[e] && groups[i].epsilon == (g.epsilon[e] != 0) &&
                same_vector(groups[i].vector, v)) {
                found = i;
                break;
            }
        if (found == groups.size()) {
            groups.push_back({g.source[e], g.target[e], g.epsilon[e] != 0, v, {}});
            candidates.push_back(found);
        }
        if (!g.epsilon[e])
            groups[found].labels |= g.labels(static_cast<edge_id>(e));
    }
    builder out(dimensions());
    for (std::size_t s = 0; s < g.states(); ++s) {
        out.add_state();
        if (g.accept[s])
            out.set_accept(static_cast<state_id>(s), a.final_weights(static_cast<state_id>(s)));
    }
    if (g.initial != no_state)
        out.set_initial(g.initial, a.initial_weights());
    for (const auto& m : groups) {
        if (m.epsilon)
            out.add_epsilon(m.from, m.to, m.vector);
        else
            out.add_transition(m.from, m.to, m.labels, m.vector);
    }
    return out.build();
}

compiled_multi_weighted_dfa multi_weighted_automaton::compile() const { return compiled_multi_weighted_dfa(*this); }

// Builder

multi_weighted_automaton::builder::builder(std::size_t dimensions) : dimensions_(positive(dimensions)), initial_(dimensions, 0.0) {}

std::vector<double> multi_weighted_automaton::builder::checked(std::span<const double> costs) const {
    return detail::copy_weights({costs.begin(), costs.end()}, dimensions_);
}

state_id multi_weighted_automaton::builder::add_state() {
    finals_.insert(finals_.end(), dimensions_, 0.0);
    return graph_.add_state();
}

void multi_weighted_automaton::builder::set_initial(state_id s, std::span<const double> costs) {
    auto v = costs.empty() ? std::vector<double>(dimensions_, 0.0) : checked(costs);
    graph_.set_initial(s);
    initial_ = std::move(v);
}

void multi_weighted_automaton::builder::set_accept(state_id s, std::span<const double> costs) {
    auto v = costs.empty() ? std::vector<double>(dimensions_, 0.0) : checked(costs);
    graph_.set_accept(s);
    std::ranges::copy(v, finals_.begin() + static_cast<std::ptrdiff_t>(u(s) * dimensions_));
}

edge_id multi_weighted_automaton::builder::add_transition(state_id from, state_id to, const interval_set& labels,
                                                          std::span<const double> costs) {
    auto v = checked(costs);
    edge_id e = graph_.add_transition(from, to, labels);
    edges_.insert(edges_.end(), v.begin(), v.end());
    return e;
}

edge_id multi_weighted_automaton::builder::add_epsilon(state_id from, state_id to, std::span<const double> costs) {
    auto v = checked(costs);
    edge_id e = graph_.add_epsilon(from, to);
    edges_.insert(edges_.end(), v.begin(), v.end());
    return e;
}

multi_weighted_automaton multi_weighted_automaton::builder::build() const {
    return of(graph_.build(), dimensions_, edges_, initial_, finals_);
}

// Unfoldings of vector weightings

multi_weighted_automaton layered_automaton::evaluate(const multi_weighted_automaton& a) const {
    check(a.graph());
    const auto& v = vectors_of(a);
    std::size_t d = v.dimensions;
    access::vectors out;
    out.dimensions = d;
    for (edge_id e : data_->original_edge)
        out.edges.insert(out.edges.end(), v.edges.begin() + static_cast<std::ptrdiff_t>(u(e) * d),
                         v.edges.begin() + static_cast<std::ptrdiff_t>((u(e) + 1) * d));
    out.initial = v.initial;
    for (state_id s : data_->original_state)
        out.finals.insert(out.finals.end(), v.finals.begin() + static_cast<std::ptrdiff_t>(u(s) * d),
                          v.finals.begin() + static_cast<std::ptrdiff_t>((u(s) + 1) * d));
    return access::make(data_->graph, std::move(out));
}

multi_weighted_path layered_automaton::map(const multi_weighted_path& path) const {
    if (!path.graph().same_as(data_->graph))
        throw std::invalid_argument("Foreign unfolded path");
    std::vector<edge_id> edges;
    for (edge_id e : path.edges())
        edges.push_back(data_->original_edge[u(e)]);
    return {weighted_path(data_->base, data_->original_state[u(path.start())], std::move(edges), path.symbols(), 0),
            {path.costs().begin(), path.costs().end()},
            {path.exact_costs().begin(), path.exact_costs().end()}};
}

}  // namespace autolib
