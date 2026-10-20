// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Enumeration of paths and words by increasing cost : a best-first search (A*) whose heuristic is the exact
// cost of the best suffix of each state, so that every popped result is the next one.

#include <algorithm>
#include <memory>
#include <limits>

#include "autolib/layered_automaton.hpp"
#include "detail/symbol_classes.hpp"
#include "weighted/exact_costs.hpp"
#include "weighted/paths.hpp"

namespace autolib {

using detail::add;
using detail::data;
using detail::exact;
using detail::u;
using detail::weights;

namespace {

/// A queue of entries popped by increasing order : priority, then estimated length, then the longest prefix
/// (depth first among ties), then the results before the prefixes, then the order of creation. Entries are moved
/// in and out, never copied.
template <class Entry>
class entry_queue {
public:
    [[nodiscard]] bool empty() const noexcept { return heap_.empty(); }
    void push(Entry e) {
        heap_.push_back(std::move(e));
        std::ranges::push_heap(heap_, after{});
    }
    Entry pop() {
        std::ranges::pop_heap(heap_, after{});
        Entry e = std::move(heap_.back());
        heap_.pop_back();
        return e;
    }

private:
    struct after {
        bool operator()(const Entry& x, const Entry& y) const {
            if (auto c = x.priority <=> y.priority; c != 0)
                return c > 0;
            if (x.estimate != y.estimate)
                return x.estimate > y.estimate;
            if (x.depth != y.depth)
                return x.depth < y.depth;
            if (x.terminal != y.terminal)
                return y.terminal;
            return x.serial > y.serial;
        }
    };
    std::vector<Entry> heap_;
};

/// An entry of the queues : a prefix, stored in an arena, and the bound of its continuations.
struct entry {
    std::uint32_t prefix;
    std::int64_t depth;
    big_integer priority;  ///< in units of the scale of the exact costs
    std::int64_t estimate;
    bool terminal;
    std::uint64_t serial;
};

constexpr std::uint32_t none = std::numeric_limits<std::uint32_t>::max();

/// Lazy enumeration of the accepting paths by increasing exact cost. With `symbols`, the paths differing by the
/// symbol read on an edge are distinct : the enumeration then lists the words of a deterministic automaton.
/// Prefixes share their beginnings in an arena of nodes.
class best_first_paths {
public:
    best_first_paths(const weighted_automaton& a, bool symbols)
        : a_(a), costs_(a), suffix_(costs_.suffixes(data(a).useful())), symbols_(symbols) {
        steps_ = optimal_suffix_steps();
        const auto& g = data(a_);
        if (g.initial != no_state && suffix_[u(g.initial)])
            enqueue(add_node({none, -1, g.initial, 0, -1, costs_.initial, 0}));
    }

    std::optional<weighted_path> next() {
        const auto& g = data(a_);
        while (!queue_.empty()) {
            entry top = queue_.pop();
            if (top.terminal)
                return witness(top.prefix, top.priority);
            auto p = top.prefix;
            sibling(p);
            auto state = nodes_[p].state;
            if (g.accept[u(state)])
                queue_.push({p, nodes_[p].depth, nodes_[p].cost + *costs_.finals[u(state)], nodes_[p].depth, true, serial_++});
            for (edge_id e : g.out(state)) {
                auto t = g.target[u(e)];
                if (!suffix_[u(t)])
                    continue;
                auto interval = static_cast<std::int32_t>(g.label_start[u(e)]);
                symbol c = g.epsilon[u(e)] ? 0 : g.mins[u(interval)];
                enqueue(add_node({p, e, t, c, interval, nodes_[p].cost + costs_.edges[u(e)], nodes_[p].depth + 1}));
            }
        }
        return std::nullopt;
    }

private:
    struct node {
        std::uint32_t parent;
        edge_id edge;
        state_id state;
        symbol c;
        std::int32_t interval;  ///< the interval of the label holding c
        big_integer cost;
        std::int64_t depth;
    };

    std::uint32_t add_node(node n) {
        if (nodes_.size() == none)
            throw std::length_error("Too many prefixes");
        nodes_.push_back(std::move(n));
        return static_cast<std::uint32_t>(nodes_.size() - 1);
    }

    /// Among the best suffixes, the fewest edges : avoids both starving on cycles of cost zero and exploring
    /// breadth first before a first result.
    std::vector<std::int64_t> optimal_suffix_steps() const {
        const auto& g = data(a_);
        std::vector<std::int64_t> steps(g.states(), -1);
        std::vector<state_id> work;
        for (std::size_t s = 0; s < g.states(); ++s)
            if (suffix_[s] && costs_.finals[s] && *suffix_[s] == *costs_.finals[s]) {
                steps[s] = 0;
                work.push_back(static_cast<state_id>(s));
            }
        for (std::size_t head = 0; head < work.size(); ++head) {
            auto t = work[head];
            for (edge_id e : g.in(t)) {
                auto s = u(g.source[u(e)]);
                if (steps[s] < 0 && suffix_[s] && costs_.edges[u(e)] + *suffix_[u(t)] == *suffix_[s]) {
                    steps[s] = steps[u(t)] + 1;
                    work.push_back(static_cast<state_id>(s));
                }
            }
        }
        return steps;
    }

    void enqueue(std::uint32_t p) {
        const auto& n = nodes_[p];
        queue_.push({p, n.depth, n.cost + *suffix_[u(n.state)], n.depth + steps_[u(n.state)], false, serial_++});
    }

    /// The same edge reading the next symbol.
    void sibling(std::uint32_t p) {
        const auto& g = data(a_);
        const auto& n = nodes_[p];
        if (!symbols_ || n.parent == none || g.epsilon[u(n.edge)])
            return;
        auto interval = n.interval;
        symbol c;
        if (n.c < g.maxs[u(interval)]) {
            c = n.c + 1;
        } else {
            ++interval;
            if (u(interval) == g.label_start[u(n.edge) + 1])
                return;
            c = g.mins[u(interval)];
        }
        enqueue(add_node({n.parent, n.edge, n.state, c, interval, n.cost, n.depth}));
    }

    weighted_path witness(std::uint32_t p, const big_integer& total) const {
        const auto& g = data(a_);
        std::vector<edge_id> edges;
        word w;
        for (auto at = p; nodes_[at].parent != none; at = nodes_[at].parent) {
            edges.push_back(nodes_[at].edge);
            if (!g.epsilon[u(nodes_[at].edge)])
                w.push_back(nodes_[at].c);
        }
        std::ranges::reverse(edges);
        std::ranges::reverse(w);
        return weighted_path(a_.graph(), g.initial, std::move(edges), std::move(w), costs_.scale.rounded(total));
    }

    weighted_automaton a_;
    detail::exact_costs costs_;
    std::vector<exact> suffix_;
    std::vector<std::int64_t> steps_;
    bool symbols_;
    std::vector<node> nodes_;
    entry_queue<entry> queue_;
    std::uint64_t serial_ = 0;
};

/// Lazy enumeration of the distinct words by increasing cost, by a search on the costs of the subsets of
/// states reached by each prefix : one prefix per word, never per path, without determinizing.
class best_first_words {
public:
    explicit best_first_words(const weighted_automaton& a) : a_(a), costs_(a), useful_(data(a).useful()) {
        const auto& g = data(a_);
        suffix_ = costs_.suffixes(useful_);
        steps_ = costs_.symbol_steps(suffix_);
        classes_ = detail::symbol_classes([&](auto&& f) {
            for (std::size_t e = 0; e < g.edges(); ++e)
                if (!g.epsilon[e] && useful_[u(g.source[e])] && useful_[u(g.target[e])])
                    f(g.labels(static_cast<edge_id>(e)));
        });
        if (g.initial != no_state && useful_[u(g.initial)]) {
            std::vector<exact> initial(g.states());
            initial[u(g.initial)] = costs_.initial;
            costs_.close_epsilon(initial, &useful_);
            enqueue(add_node({none, 0, -1, state_costs(std::move(initial)), 0}));
        }
    }

    std::optional<weighted_word> next() {
        const auto& g = data(a_);
        while (!queue_.empty()) {
            entry top = queue_.pop();
            auto p = top.prefix;
            if (top.terminal) {
                word w(u(nodes_[p].depth));
                auto at = p;
                for (std::size_t i = w.size(); i-- > 0; at = nodes_[at].parent)
                    w[i] = nodes_[at].c;
                return weighted_word{std::move(w), costs_.scale.rounded(top.priority)};
            }
            auto n = nodes_[p];
            if (n.parent != none && n.c < classes_.max(n.symbol_class))
                enqueue(add_node({n.parent, n.c + 1, n.symbol_class, n.costs, n.depth}));
            if (costs_of_[n.costs].terminal)
                queue_.push({p, n.depth, *costs_of_[n.costs].terminal, n.depth, true, serial_++});
            for (std::int32_t c = 0; c < classes_.size(); ++c) {
                std::vector<exact> next(g.states());
                symbol sym = classes_.min(c);
                const auto& values = costs_of_[n.costs].values;
                for (std::size_t e = 0; e < g.edges(); ++e) {
                    if (!values[u(g.source[e])] || !useful_[u(g.target[e])] || !g.accepts(static_cast<edge_id>(e), sym))
                        continue;
                    auto t = u(g.target[e]);
                    big_integer value = *values[u(g.source[e])] + costs_.edges[e];
                    if (detail::improves(value, next[t]))
                        next[t] = std::move(value);
                }
                costs_.close_epsilon(next, &useful_);
                auto k = state_costs(std::move(next));
                if (k != none)
                    enqueue(add_node({p, sym, c, k, n.depth + 1}));
            }
        }
        return std::nullopt;
    }

private:
    /// The costs of the states reached by a prefix, the best cost of its continuations, its own cost if
    /// accepted, and the fewest symbols of a best continuation.
    struct costs_of {
        std::vector<exact> values;
        big_integer bound;
        exact terminal;
        std::int64_t remaining;
    };
    struct node {
        std::uint32_t parent;
        symbol c;
        std::int32_t symbol_class;
        std::uint32_t costs;  ///< shared by the prefixes differing by their last symbol within a class
        std::int64_t depth;
    };

    std::uint32_t add_node(node n) {
        if (nodes_.size() == none)
            throw std::length_error("Too many prefixes");
        nodes_.push_back(n);
        return static_cast<std::uint32_t>(nodes_.size() - 1);
    }

    std::uint32_t state_costs(std::vector<exact> values) {
        exact bound, terminal;
        std::int64_t remaining = std::numeric_limits<int>::max();
        for (std::size_t s = 0; s < values.size(); ++s) {
            if (!values[s] || !suffix_[s])
                continue;
            big_integer value = *values[s] + *suffix_[s];
            if (!bound || value < *bound) {
                bound = std::move(value);
                remaining = steps_[s];
            } else if (value == *bound) {
                remaining = std::min<std::int64_t>(remaining, steps_[s]);
            }
            if (costs_.finals[s]) {
                big_integer last = *values[s] + *costs_.finals[s];
                if (detail::improves(last, terminal))
                    terminal = std::move(last);
            }
        }
        if (!bound)
            return none;
        costs_of_.push_back({std::move(values), std::move(*bound), std::move(terminal), remaining});
        return static_cast<std::uint32_t>(costs_of_.size() - 1);
    }

    void enqueue(std::uint32_t p) {
        const auto& n = nodes_[p];
        const auto& k = costs_of_[n.costs];
        queue_.push({p, n.depth, k.bound, n.depth + k.remaining, false, serial_++});
    }

    weighted_automaton a_;
    detail::exact_costs costs_;
    std::vector<bool> useful_;
    std::vector<exact> suffix_;
    std::vector<int> steps_;
    detail::symbol_classes classes_;
    std::vector<node> nodes_;
    std::vector<costs_of> costs_of_;
    entry_queue<entry> queue_;
    std::uint64_t serial_ = 0;
};

/// The k best paths of a DAG, keeping the k best suffixes of every state.
std::vector<weighted_path> k_best_dag(const weighted_automaton& a, std::size_t k) {
    const auto& g = data(a);
    const auto& w = detail::weights(a);
    if (k == 0 || g.initial == no_state)
        return {};
    detail::exactness certificate;
    certificate.include(w.initial);
    for (std::size_t s = 0; s < g.states(); ++s)
        if (g.accept[s])
            certificate.include(w.finals[s]);
    for (double x : w.edges)
        certificate.include(x);
    bool exact = !certificate.safe(static_cast<double>(g.states()) + 1.0);
    struct suffix {
        double cost;
        std::optional<dyadic> exact_cost;
        edge_id edge;
        std::shared_ptr<const suffix> next;
    };
    using suffix_ptr = std::shared_ptr<const suffix>;
    std::vector<std::vector<suffix_ptr>> best(g.states());
    for (std::size_t p = g.order.size(); p-- > 0;) {
        auto s = u(g.order[p]);
        std::vector<suffix_ptr> candidates;
        if (g.accept[s])
            candidates.push_back(std::make_shared<const suffix>(
                suffix{w.finals[s], exact ? std::optional<dyadic>(dyadic(w.finals[s])) : std::nullopt, -1, nullptr}));
        for (edge_id e : g.out(static_cast<state_id>(s)))
            for (const auto& next : best[u(g.target[u(e)])]) {
                if (exact) {
                    dyadic value = dyadic(w.edges[u(e)]) + *next->exact_cost;
                    double rounded = value.to_double();
                    if (!std::isfinite(rounded))
                        throw std::overflow_error("Cost overflow");
                    candidates.push_back(std::make_shared<const suffix>(suffix{rounded, std::move(value), e, next}));
                } else {
                    candidates.push_back(std::make_shared<const suffix>(suffix{add(w.edges[u(e)], next->cost), std::nullopt, e, next}));
                }
            }
        if (exact)
            std::ranges::stable_sort(candidates, [](const suffix_ptr& x, const suffix_ptr& y) { return *x->exact_cost < *y->exact_cost; });
        else
            std::ranges::stable_sort(candidates, [](const suffix_ptr& x, const suffix_ptr& y) { return x->cost < y->cost; });
        if (candidates.size() > k)
            candidates.resize(k);
        best[s] = std::move(candidates);
    }
    std::vector<weighted_path> result;
    for (const auto& first : best[u(g.initial)]) {
        std::vector<edge_id> edges;
        word symbols;
        double cost = w.initial;
        state_id s = g.initial;
        for (const suffix* at = first.get(); at->edge >= 0; at = at->next.get()) {
            edges.push_back(at->edge);
            if (!g.epsilon[u(at->edge)])
                symbols.push_back(g.representative(at->edge));
            if (!exact)
                cost = add(cost, w.edges[u(at->edge)]);
            s = g.target[u(at->edge)];
        }
        if (exact) {
            double rounded = (*first->exact_cost + dyadic(w.initial)).to_double();
            if (!std::isfinite(rounded))
                throw std::overflow_error("Cost overflow");
            cost = rounded;
        } else {
            cost = add(cost, w.finals[u(s)]);
        }
        result.emplace_back(a.graph(), g.initial, std::move(edges), std::move(symbols), cost);
    }
    return result;
}

template <class T, class Source>
std::vector<T> first(Source& source, std::size_t k) {
    std::vector<T> result;
    while (result.size() < k)
        if (auto x = source.next())
            result.push_back(std::move(*x));
        else
            break;
    return result;
}

}  // namespace

std::vector<weighted_path> weighted_automaton::k_best_paths(std::size_t k) const {
    if (k == 0)
        return {};
    if (graph_.is_acyclic())
        return k_best_dag(*this, k);
    best_first_paths paths(*this, false);
    return first<weighted_path>(paths, k);
}

lazy_sequence<weighted_path> weighted_automaton::paths_by_cost() const {
    auto paths = std::make_shared<best_first_paths>(*this, false);
    return lazy_sequence<weighted_path>([paths] { return paths->next(); });
}

lazy_sequence<weighted_word> weighted_automaton::words_by_cost() const {
    if (!graph_.is_deterministic()) {
        auto words = std::make_shared<best_first_words>(*this);
        return lazy_sequence<weighted_word>([words] { return words->next(); });
    }
    // Deterministic : the paths reading each symbol are the words
    auto paths = std::make_shared<best_first_paths>(*this, true);
    return lazy_sequence<weighted_word>([paths]() -> std::optional<weighted_word> {
        auto path = paths->next();
        if (!path)
            return std::nullopt;
        return weighted_word{path->symbols(), path->cost()};
    });
}

std::vector<weighted_word> weighted_automaton::k_best_words(std::size_t k) const {
    if (k == 0)
        return {};
    auto words = words_by_cost();
    return first<weighted_word>(words, k);
}

std::vector<weighted_path> k_best_paths_of_length(const weighted_automaton& a, int length, std::size_t k) {
    auto layers = layered_automaton::exactly(a.graph(), length);
    std::vector<weighted_path> result;
    for (const auto& p : layers.evaluate(a).k_best_paths(k))
        result.push_back(layers.map(p));
    return result;
}

std::vector<weighted_word> k_best_words_of_length(const weighted_automaton& a, int length, std::size_t k) {
    return layered_automaton::exactly(a.graph(), length).evaluate(a).k_best_words(k);
}

}  // namespace autolib
