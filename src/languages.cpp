// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <algorithm>
#include <bit>
#include <deque>
#include <numeric>

#include "autolib/compiled_dfa.hpp"
#include "detail/access.hpp"
#include "detail/algorithms.hpp"
#include "detail/hash_table.hpp"
#include "detail/symbol_classes.hpp"

namespace autolib {

namespace {

std::size_t u(std::int32_t i) { return static_cast<std::size_t>(i); }

/// Walks the transitions of p in a and q in b together, calling f(min, max, target in a, target in b) on every
/// maximal segment of symbols where at least one of them has a transition, the other target being no_state if absent.
template <class F>
void sweep(const compiled_dfa& a, state_id p, const compiled_dfa& b, state_id q, F&& f) {
    auto ea = a.edges(p), eb = b.edges(q);
    std::size_t i = 0, j = 0;
    std::int64_t x = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t none = std::numeric_limits<std::int64_t>::max();
    while (i < ea.size() || j < eb.size()) {
        std::int64_t next_a = i < ea.size() ? ea[i].min : none;
        std::int64_t next_b = j < eb.size() ? eb[j].min : none;
        x = std::max(x, std::min(next_a, next_b));
        bool in_a = next_a <= x, in_b = next_b <= x;
        std::int64_t end = std::min(in_a ? std::int64_t{ea[i].max} : next_a - 1, in_b ? std::int64_t{eb[j].max} : next_b - 1);
        f(static_cast<symbol>(x), static_cast<symbol>(end), in_a ? ea[i].target : no_state, in_b ? eb[j].target : no_state);
        if (in_a && ea[i].max == end)
            ++i;
        if (in_b && eb[j].max == end)
            ++j;
        x = end + 1;
    }
}

bool accepts(detail::product_kind kind, bool left, bool right) {
    switch (kind) {
    case detail::product_kind::union_of: return left || right;
    case detail::product_kind::intersection_of: return left && right;
    case detail::product_kind::difference_of: return left && !right;
    case detail::product_kind::symmetric_difference_of: return left != right;
    }
    return false;
}

/// Whether a pair of states may still lead to an accepted word
bool alive(detail::product_kind kind, state_id left, state_id right) {
    switch (kind) {
    case detail::product_kind::intersection_of: return left != no_state && right != no_state;
    case detail::product_kind::difference_of: return left != no_state;
    default: return left != no_state || right != no_state;
    }
}

}  // namespace

automaton detail::product(const automaton& left, const automaton& right, product_kind kind) {
    compiled_dfa a(left, false), b(right, false);
    automaton out;
    if (!alive(kind, a.initial(), b.initial())) {
        out.set_initial(out.add_state());
        return out;
    }
    key_numbering pairs(a.state_count() + b.state_count());
    bool added = false;
    pairs.insert(pair_key(a.initial(), b.initial()), added);
    out.set_initial(out.add_state());
    std::vector<std::int32_t> slot_of;
    std::vector<std::pair<state_id, interval_set>> by_target;
    for (std::int32_t v = 0; v < static_cast<std::int32_t>(pairs.size()); ++v) {
        auto key = pairs.key(v);
        state_id p = pair_first(key), q = pair_second(key);
        out.set_accept(v, accepts(kind, a.is_accept(p), b.is_accept(q)));
        sweep(a, p, b, q, [&](symbol min, symbol max, state_id dp, state_id dq) {
            if (!alive(kind, dp, dq))
                return;
            auto target = pairs.insert(pair_key(dp, dq), added);
            if (added)
                out.add_state();
            if (slot_of.size() <= u(target))
                slot_of.resize(2 * u(target) + 1, -1);
            if (slot_of[u(target)] < 0) {
                slot_of[u(target)] = static_cast<std::int32_t>(by_target.size());
                by_target.emplace_back(target, interval_set{});
            }
            by_target[u(slot_of[u(target)])].second.append({min, max});
        });
        for (auto& [target, labels] : by_target) {
            slot_of[u(target)] = -1;
            access::add(out, v, target, std::move(labels));
        }
        by_target.clear();
    }
    return out;
}

automaton automaton::intersect_with(const automaton& other) const {
    return detail::product(*this, other, detail::product_kind::intersection_of).minimize();
}

automaton automaton::minus(const automaton& other) const {
    return detail::product(*this, other, detail::product_kind::difference_of).minimize();
}

automaton automaton::symmetric_difference(const automaton& other) const {
    return detail::product(*this, other, detail::product_kind::symmetric_difference_of).minimize();
}

automaton automaton::complement() const {
    // Swapping accepting states is only valid on a complete deterministic automaton
    automaton out = initial_ == no_state ? automaton{} : determinize();
    if (out.initial_ == no_state)
        out.set_initial(out.add_state());
    state_id sink = out.add_state();
    for (auto& s : out.states_) {
        interval_set missing = interval_set::all();
        for (const auto& t : s.out)
            missing -= t.labels;
        s.accept = !s.accept;
        if (!missing.empty())
            s.out.push_back({sink, std::move(missing)});
    }
    out.states_[u(sink)].accept = true;
    out.deterministic_ = true;
    return out;
}

// ---------------------------------------------------------------------- decisions

bool automaton::is_empty() const {
    return !shortest_word().has_value();
}

bool automaton::is_universal() const {
    return complement().is_empty();
}

bool automaton::same_language(const automaton& other) const {
    if (this == &other)
        return true;
    compiled_dfa a(*this, false), b(other, false);
    auto na = static_cast<std::int32_t>(a.state_count()), nb = static_cast<std::int32_t>(b.state_count());
    // Union-find on the states of a, the dead state of a, the states of b, the dead state of b
    std::int32_t dead_a = na, offset_b = na + 1, dead_b = na + 1 + nb;
    std::vector<std::int32_t> parent(u(na + nb + 2));
    std::iota(parent.begin(), parent.end(), 0);
    auto find = [&](std::int32_t x) {
        while (parent[u(x)] != x) {
            parent[u(x)] = parent[u(parent[u(x)])];
            x = parent[u(x)];
        }
        return x;
    };
    auto unite = [&](std::int32_t x, std::int32_t y) {
        auto rx = find(x), ry = find(y);
        if (rx == ry)
            return false;
        parent[u(rx)] = ry;
        return true;
    };
    auto node_a = [&](state_id p) { return p == no_state ? dead_a : p; };
    auto node_b = [&](state_id q) { return q == no_state ? dead_b : offset_b + q; };
    unite(dead_a, dead_b);
    std::vector<std::pair<state_id, state_id>> todo;
    if (unite(node_a(a.initial()), node_b(b.initial())))
        todo.emplace_back(a.initial(), b.initial());
    while (!todo.empty()) {
        auto [p, q] = todo.back();
        todo.pop_back();
        if (a.is_accept(p) != b.is_accept(q))
            return false;
        sweep(a, p, b, q, [&](symbol, symbol, state_id dp, state_id dq) {
            if (unite(node_a(dp), node_b(dq)))
                todo.emplace_back(dp, dq);
        });
    }
    return true;
}

std::optional<word> automaton::counter_example(const automaton& other) const {
    compiled_dfa a(*this, false), b(other, false);
    // Breadth first search on pairs : nodes (pair, parent node, symbol read)
    detail::key_numbering seen(a.state_count() + b.state_count());
    std::vector<std::pair<std::int32_t, symbol>> parent;
    bool added = false;
    seen.insert(detail::pair_key(a.initial(), b.initial()), added);
    parent.emplace_back(-1, 0);
    for (std::int32_t v = 0; v < static_cast<std::int32_t>(seen.size()); ++v) {
        auto key = seen.key(v);
        state_id p = detail::pair_first(key), q = detail::pair_second(key);
        if (a.is_accept(p) != b.is_accept(q)) {
            word w;
            for (auto x = v; parent[u(x)].first >= 0; x = parent[u(x)].first)
                w.push_back(parent[u(x)].second);
            std::ranges::reverse(w);
            return w;
        }
        sweep(a, p, b, q, [&](symbol min, symbol, state_id dp, state_id dq) {
            seen.insert(detail::pair_key(dp, dq), added);
            if (added)
                parent.emplace_back(v, min);
        });
    }
    return std::nullopt;
}

namespace {

/// Fixed size set of states as a bitset
class state_set {
public:
    explicit state_set(std::size_t n = 0) : words_((n + 63) / 64, 0) {}
    void set(std::int32_t i) noexcept { words_[u(i) / 64] |= std::uint64_t{1} << (u(i) % 64); }
    [[nodiscard]] bool intersects(const state_set& o) const noexcept {
        for (std::size_t i = 0; i < words_.size(); ++i)
            if (words_[i] & o.words_[i])
                return true;
        return false;
    }
    [[nodiscard]] bool subset_of(const state_set& o) const noexcept {
        for (std::size_t i = 0; i < words_.size(); ++i)
            if (words_[i] & ~o.words_[i])
                return false;
        return true;
    }
    template <class F>
    void for_each(F&& f) const {
        for (std::size_t i = 0; i < words_.size(); ++i)
            for (auto w = words_[i]; w != 0; w &= w - 1)
                f(static_cast<std::int32_t>(64 * i + static_cast<std::size_t>(std::countr_zero(w))));
    }

private:
    std::vector<std::uint64_t> words_;
};

/// Symbol edges of an automaton without epsilon transitions, as (class, target) sorted by class, per state
struct class_edges {
    std::vector<std::int32_t> start;
    std::vector<std::pair<std::int32_t, std::int32_t>> edges;

    class_edges(const automaton& a, const detail::symbol_classes& classes) : start(a.state_count() + 1, 0) {
        for (state_id s = 0; s < static_cast<state_id>(a.state_count()); ++s) {
            auto first = edges.size();
            for (const auto& t : a.transitions(s))
                classes.for_each_class(t.labels, [&](std::int32_t c) { edges.emplace_back(c, t.target); });
            std::sort(edges.begin() + static_cast<std::ptrdiff_t>(first), edges.end());
            start[u(s) + 1] = static_cast<std::int32_t>(edges.size());
        }
    }
    [[nodiscard]] std::span<const std::pair<std::int32_t, std::int32_t>> of(state_id s) const noexcept {
        return {edges.data() + start[u(s)], edges.data() + start[u(s) + 1]};
    }
    [[nodiscard]] std::span<const std::pair<std::int32_t, std::int32_t>> of(state_id s, std::int32_t c) const noexcept {
        auto all = of(s);
        auto lo = std::ranges::lower_bound(all, std::pair{c, std::numeric_limits<std::int32_t>::min()});
        auto hi = std::ranges::lower_bound(lo, all.end(), std::pair{c + 1, std::numeric_limits<std::int32_t>::min()});
        return {lo, hi};
    }
};

/// Adds a set to an antichain of minimal sets, unless it contains one of them; returns whether it was added
bool add_to_antichain(std::vector<state_set>& antichain, const state_set& set) {
    for (const auto& other : antichain)
        if (other.subset_of(set))
            return false;
    std::erase_if(antichain, [&](const state_set& other) { return set.subset_of(other); });
    antichain.push_back(set);
    return true;
}

}  // namespace

bool automaton::is_subset_of(const automaton& other) const {
    automaton a = remove_epsilon(), b = other.remove_epsilon();
    if (a.initial_ == no_state)
        return true;
    if (b.initial_ == no_state)
        return a.is_empty();
    detail::symbol_classes classes([&](auto&& f) {
        for (const automaton* x : {&a, &b})
            for (const auto& s : x->states_)
                for (const auto& t : s.out)
                    f(t.labels);
    });
    class_edges succ_a(a, classes), succ_b(b, classes);
    auto nb = b.state_count();
    state_set accept_b(nb);
    for (state_id s = 0; s < static_cast<state_id>(nb); ++s)
        if (b.is_accept(s))
            accept_b.set(s);
    // Pairs (state of a, set of states of b reached by the same word), keeping only the minimal sets for each state
    std::vector<std::vector<state_set>> antichains(a.state_count());
    state_set start(nb);
    start.set(b.initial_);
    antichains[u(a.initial_)].push_back(start);
    std::vector<std::pair<state_id, state_set>> todo{{a.initial_, start}};
    while (!todo.empty()) {
        auto [p, set] = std::move(todo.back());
        todo.pop_back();
        if (a.is_accept(p) && !set.intersects(accept_b))
            return false;
        auto edges = succ_a.of(p);
        for (std::size_t i = 0; i < edges.size();) {
            auto c = edges[i].first;
            state_set next(nb);
            set.for_each([&](std::int32_t s) {
                for (auto [cls, target] : succ_b.of(s, c))
                    next.set(target);
            });
            for (; i < edges.size() && edges[i].first == c; ++i)
                if (add_to_antichain(antichains[u(edges[i].second)], next))
                    todo.emplace_back(edges[i].second, next);
        }
    }
    return true;
}

std::partial_ordering automaton::compare(const automaton& other) const {
    bool le = is_subset_of(other), ge = other.is_subset_of(*this);
    if (le && ge)
        return std::partial_ordering::equivalent;
    if (le)
        return std::partial_ordering::less;
    if (ge)
        return std::partial_ordering::greater;
    return std::partial_ordering::unordered;
}

std::optional<word> automaton::shortest_word() const {
    if (initial_ == no_state)
        return std::nullopt;
    // 0-1 breadth first search : epsilon transitions cost 0, symbol transitions 1
    auto n = states_.size();
    std::vector<std::int32_t> dist(n, std::numeric_limits<std::int32_t>::max()), parent(n, -1);
    std::vector<symbol> read(n, 0);
    std::vector<char> done(n, 0), by_symbol(n, 0);
    std::deque<state_id> todo{initial_};
    dist[u(initial_)] = 0;
    while (!todo.empty()) {
        state_id s = todo.front();
        todo.pop_front();
        if (done[u(s)])
            continue;
        done[u(s)] = 1;
        if (states_[u(s)].accept) {
            word w;
            for (state_id v = s; parent[u(v)] >= 0; v = parent[u(v)])
                if (by_symbol[u(v)])
                    w.push_back(read[u(v)]);
            std::ranges::reverse(w);
            return w;
        }
        for (state_id t : states_[u(s)].eps) {
            if (dist[u(s)] < dist[u(t)]) {
                dist[u(t)] = dist[u(s)];
                parent[u(t)] = s;
                by_symbol[u(t)] = 0;
                todo.push_front(t);
            }
        }
        for (const auto& t : states_[u(s)].out) {
            if (dist[u(s)] + 1 < dist[u(t.target)]) {
                dist[u(t.target)] = dist[u(s)] + 1;
                parent[u(t.target)] = s;
                read[u(t.target)] = t.labels.min();
                by_symbol[u(t.target)] = 1;
                todo.push_back(t.target);
            }
        }
    }
    return std::nullopt;
}

bool automaton::is_finite() const {
    automaton a = remove_epsilon();  // also trimmed
    if (a.initial_ == no_state)
        return true;
    // Iterative depth first search looking for a cycle, every state being useful
    auto n = a.states_.size();
    std::vector<char> color(n, 0);  // 0 unvisited, 1 on the path, 2 done
    std::vector<std::pair<state_id, std::size_t>> path{{a.initial_, 0}};
    color[u(a.initial_)] = 1;
    while (!path.empty()) {
        auto& [s, next] = path.back();
        const auto& out = a.states_[u(s)].out;
        if (next == out.size()) {
            color[u(s)] = 2;
            path.pop_back();
            continue;
        }
        state_id t = out[next++].target;
        if (color[u(t)] == 1)
            return false;
        if (color[u(t)] == 0) {
            color[u(t)] = 1;
            path.emplace_back(t, 0);
        }
    }
    return true;
}

}  // namespace autolib
