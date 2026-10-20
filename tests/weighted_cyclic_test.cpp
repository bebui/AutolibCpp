// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Weighted automata with cycles, signed weights and exact sums, against exact oracles.

#include <autolib/autolib.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <deque>
#include <limits>
#include <optional>
#include <random>
#include <set>

using namespace autolib;

namespace {

constexpr double inf = std::numeric_limits<double>::infinity();
using exact = std::optional<dyadic>;

struct random_source {
    std::mt19937 engine;
    explicit random_source(unsigned seed) : engine(seed) {}
    int next(int bound) { return std::uniform_int_distribution<int>(0, bound - 1)(engine); }
    bool coin() { return next(2) == 1; }
};

std::size_t z(std::int64_t i) { return static_cast<std::size_t>(i); }

struct walk {
    dyadic cost;
    std::vector<edge_id> edges;
};

/// Every accepting walk of at most `left` edges, with its exact cost.
void enumerate(const weighted_automaton& a, state_id s, int left, const dyadic& cost, std::vector<edge_id>& prefix, std::vector<walk>& result) {
    if (a.graph().is_accept(s))
        result.push_back({cost + dyadic(a.final_weight(s)), prefix});
    if (left == 0)
        return;
    for (edge_id e : a.graph().outgoing(s)) {
        prefix.push_back(e);
        enumerate(a, a.graph().target(e), left - 1, cost + dyadic(a.weight(e)), prefix, result);
        prefix.pop_back();
    }
}

dyadic path_cost(const weighted_automaton& a, const weighted_path& p) {
    dyadic result(a.initial_weight());
    for (edge_id e : p.edges())
        result += dyadic(a.weight(e));
    return result + dyadic(a.final_weight(p.end()));
}

std::vector<word> all_words(int alphabet, int depth) {
    std::vector<word> result{{}};
    std::size_t from = 0;
    for (int length = 1; length <= depth; ++length) {
        auto to = result.size();
        for (auto i = from; i < to; ++i)
            for (symbol c = 0; c < alphabet; ++c) {
                auto w = result[i];
                w.push_back(c);
                result.push_back(w);
            }
        from = to;
    }
    return result;
}

/// The exact cost of a word, by an independent Floyd-Warshall closure of the epsilon edges.
exact word_cost(const weighted_automaton& a, const word& w) {
    const auto& g = a.graph();
    std::size_t n = g.state_count();
    if (g.initial() == no_state)
        return std::nullopt;
    std::vector<std::vector<exact>> eps(n, std::vector<exact>(n));
    for (std::size_t s = 0; s < n; ++s)
        eps[s][s] = dyadic();
    for (std::size_t e = 0; e < g.edge_count(); ++e)
        if (g.is_epsilon(static_cast<edge_id>(e))) {
            auto s = z(g.source(static_cast<edge_id>(e))), t = z(g.target(static_cast<edge_id>(e)));
            dyadic v(a.weight(static_cast<edge_id>(e)));
            if (!eps[s][t] || v < *eps[s][t])
                eps[s][t] = v;
        }
    for (std::size_t k = 0; k < n; ++k)
        for (std::size_t s = 0; s < n; ++s)
            for (std::size_t t = 0; t < n; ++t)
                if (eps[s][k] && eps[k][t]) {
                    dyadic v = *eps[s][k] + *eps[k][t];
                    if (!eps[s][t] || v < *eps[s][t])
                        eps[s][t] = v;
                }
    std::vector<exact> current(n);
    current[z(g.initial())] = dyadic(a.initial_weight());
    for (std::size_t position = 0;; ++position) {
        std::vector<exact> closed(n);
        for (std::size_t s = 0; s < n; ++s)
            for (std::size_t t = 0; t < n; ++t)
                if (current[s] && eps[s][t]) {
                    dyadic v = *current[s] + *eps[s][t];
                    if (!closed[t] || v < *closed[t])
                        closed[t] = v;
                }
        if (position == w.size()) {
            exact best;
            for (std::size_t s = 0; s < n; ++s)
                if (closed[s] && g.is_accept(static_cast<state_id>(s))) {
                    dyadic v = *closed[s] + dyadic(a.final_weight(static_cast<state_id>(s)));
                    if (!best || v < *best)
                        best = v;
                }
            return best;
        }
        current.assign(n, std::nullopt);
        for (std::size_t e = 0; e < g.edge_count(); ++e) {
            auto id = static_cast<edge_id>(e);
            if (closed[z(g.source(id))] && g.accepts(id, w[position])) {
                dyadic v = *closed[z(g.source(id))] + dyadic(a.weight(id));
                auto t = z(g.target(id));
                if (!current[t] || v < *current[t])
                    current[t] = v;
            }
        }
    }
}

void same_costs(const weighted_automaton& a, const weighted_automaton& b, const std::vector<word>& words) {
    for (const auto& w : words) {
        INFO("word of length " << w.size());
        REQUIRE(word_cost(a, w) == word_cost(b, w));
    }
}

/// Whether two states have the same language and suffix costs differing by a constant, by an independent
/// exploration of their pairs.
bool equivalent_up_to_offset(const weighted_automaton& a, state_id left, state_id right) {
    const auto& g = a.graph();
    std::size_t n = g.state_count();
    std::vector<exact> deltas(n * n);
    std::deque<std::size_t> work;
    auto root = z(left) * n + z(right);
    deltas[root] = dyadic();
    work.push_back(root);
    exact offset;
    while (!work.empty()) {
        auto pair = work.front();
        work.pop_front();
        auto s = static_cast<state_id>(pair / n), t = static_cast<state_id>(pair % n);
        if (g.is_accept(s) != g.is_accept(t))
            return false;
        if (g.is_accept(s)) {
            dyadic value = *deltas[pair] + dyadic(a.final_weight(s)) - dyadic(a.final_weight(t));
            if (!offset)
                offset = value;
            else if (*offset != value)
                return false;
        }
        for (symbol c = 0; c < 2; ++c) {
            edge_id es = -1, et = -1;
            for (edge_id e : g.outgoing(s))
                if (g.accepts(e, c))
                    es = e;
            for (edge_id e : g.outgoing(t))
                if (g.accepts(e, c))
                    et = e;
            if ((es < 0) != (et < 0))
                return false;
            if (es < 0)
                continue;
            auto next = z(g.target(es)) * n + z(g.target(et));
            dyadic value = *deltas[pair] + dyadic(a.weight(es)) - dyadic(a.weight(et));
            if (!deltas[next]) {
                deltas[next] = value;
                work.push_back(next);
            } else if (*deltas[next] != value) {
                return false;
            }
        }
    }
    return true;
}

std::size_t minimal_state_count(const weighted_automaton& original) {
    auto a = original.trim();
    std::vector<state_id> representatives;
    for (std::size_t s = 0; s < a.graph().state_count(); ++s) {
        bool found = std::ranges::any_of(representatives, [&](state_id t) { return equivalent_up_to_offset(a, static_cast<state_id>(s), t); });
        if (!found)
            representatives.push_back(static_cast<state_id>(s));
    }
    return representatives.size();
}

}  // namespace

TEST_CASE("k best walks of cyclic automata agree with a bounded exhaustive oracle", "[weighted][cyclic]") {
    random_source r(99120);
    for (int round = 0; round < 60; ++round) {
        int n = 4;
        std::vector<double> p(4);
        for (auto& x : p)
            x = (r.next(5) - 2) / 4.0;
        weighted_automaton::builder b;
        for (int s = 0; s < n; ++s) {
            b.add_state();
            b.set_accept(s);
        }
        b.set_initial(0, 0.5);
        for (int s = 0; s < n; ++s)
            for (symbol c = 0; c < 2; ++c) {
                int t = r.next(n);
                b.add_transition(s, t, c, 1 + r.next(2) + p[z(s)] - p[z(t)]);
            }
        auto a = b.build();
        std::vector<walk> all;
        std::vector<edge_id> prefix;
        enumerate(a, 0, 10, dyadic(0.5), prefix, all);
        std::ranges::stable_sort(all, {}, &walk::cost);
        // Longer walks cost at least 0.5 + 11 - 1 : the first 12 are all listed
        REQUIRE(all[11].cost < dyadic(10.5));
        auto actual = a.k_best_paths(12);
        auto lazy = a.paths_by_cost();
        std::set<std::vector<edge_id>> seen;
        for (std::size_t i = 0; i < 12; ++i) {
            REQUIRE(all[i].cost.to_double() == actual[i].cost());
            REQUIRE(actual[i].cost() == path_cost(a, actual[i]).to_double());
            REQUIRE(seen.insert({actual[i].edges().begin(), actual[i].edges().end()}).second);
            auto next = lazy.next();
            REQUIRE(next);
            REQUIRE(next->cost() == actual[i].cost());
        }
    }
}

TEST_CASE("signed cycles, boundaries, exact cancellations and irrelevant cycles", "[weighted][cyclic]") {
    weighted_automaton::builder b;
    for (int s = 0; s < 7; ++s)
        b.add_state();
    b.set_initial(0, -2);
    b.set_accept(3, 4);
    b.add_transition(0, 1, 1, 1e16);
    b.add_transition(1, 2, 2, 1);
    b.add_transition(2, 3, 3, -1e16);
    b.add_transition(3, 1, 4, 1e16);
    b.add_transition(0, 3, 5, 1.5);
    b.add_epsilon(4, 4, -1);      // unreachable
    b.add_transition(0, 5, 6, 0);  // cannot accept
    b.add_transition(5, 5, 6, -1);
    auto a = b.build();
    auto k = a.k_best_paths(4);
    REQUIRE(k[0].cost() == 3);
    REQUIRE(k[1].cost() == 3.5);
    REQUIRE(k[2].cost() == 4);
    REQUIRE(k[3].cost() == 4.5);
    for (const auto& p : k)
        REQUIRE(path_cost(a, p).to_double() == p.cost());
    REQUIRE(a.k_best_words(1)[0].cost == 3);
    weighted_automaton::builder c;
    c.add_state();
    c.set_initial(0);
    c.set_accept(0);
    c.add_transition(0, 0, 1, -std::numeric_limits<double>::denorm_min());
    auto unbounded = c.build();
    REQUIRE_THROWS_AS(unbounded.paths_by_cost(), std::invalid_argument);
    REQUIRE_THROWS_AS(unbounded.k_best_paths(1), std::invalid_argument);
    REQUIRE(unbounded.k_best_paths(0).empty());
}

TEST_CASE("cycles of cost zero are fair and horizons count symbols", "[weighted][cyclic]") {
    weighted_automaton::builder b;
    b.add_state();
    b.set_initial(0, 2);
    b.set_accept(0, 3);
    b.add_epsilon(0, 0, 0);
    b.add_transition(0, 0, 7, 0);
    auto a = b.build();
    auto paths = a.paths_by_cost();
    std::set<std::vector<edge_id>> seen;
    std::size_t previous = 0;
    int count = 0;
    for (const auto& p : paths) {
        REQUIRE(p.cost() == 5);
        REQUIRE(p.edge_count() >= previous);
        previous = p.edge_count();
        REQUIRE(seen.insert({p.edges().begin(), p.edges().end()}).second);
        if (++count == 50)
            break;
    }
    for (const auto& p : k_best_paths_of_length(a, 2, 10)) {
        REQUIRE(p.length() == 2);
        REQUIRE(p.graph().same_as(a.graph()));
    }
    auto words = k_best_words_of_length(a, 2, 10);
    REQUIRE(words.size() == 1);
    REQUIRE(words[0].symbols == word{7, 7});
    REQUIRE(words[0].cost == 5);
}

TEST_CASE("lazy enumerations expand neither all prefixes nor whole intervals", "[weighted][cyclic]") {
    weighted_automaton::builder b;
    for (int s = 0; s <= 80; ++s)
        b.add_state();
    b.set_initial(0);
    b.set_accept(80);
    b.add_epsilon(0, 0, 0);
    for (int s = 0; s < 80; ++s) {
        b.add_transition(s, s + 1, 1, 0);
        b.add_transition(s, s + 1, 2, 0);
    }
    auto a = b.build();
    REQUIRE(a.paths_by_cost().next()->length() == 80);
    REQUIRE(a.words_by_cost().next()->length() == 80);
    weighted_automaton::builder c;
    c.add_state();
    c.add_state();
    c.set_initial(0);
    c.set_accept(1);
    c.add_transition(0, 1, interval_set::all(), 0.5);
    auto any = c.build();
    auto words = any.k_best_words(4);
    for (int i = 0; i < 4; ++i)
        REQUIRE(words[z(i)].symbols == word{min_symbol + i});
    REQUIRE(any.k_best_paths(9).size() == 1);
    weighted_automaton::builder d;
    d.add_state();
    d.add_state();
    d.set_initial(0);
    d.set_accept(1);
    d.add_transition(0, 1, interval_set::range(min_symbol, min_symbol) | interval_set::range(max_symbol - 1, max_symbol), 1);
    auto it = d.build().words_by_cost();
    REQUIRE(it.next()->symbols[0] == min_symbol);
    REQUIRE(it.next()->symbols[0] == max_symbol - 1);
    REQUIRE(it.next()->symbols[0] == max_symbol);
    REQUIRE_FALSE(it.next());
}

TEST_CASE("residual determinization of cyclic automata agrees with an independent oracle", "[weighted][cyclic]") {
    random_source r(99121);
    auto words = all_words(3, 5);
    for (int round = 0; round < 45; ++round) {
        int n = 4;
        std::vector<double> p(4);
        const double c[] = {-0.5, 0.25, 0.75};
        for (auto& x : p)
            x = (r.next(9) - 4) / 4.0;
        weighted_automaton::builder b;
        for (int s = 0; s < n; ++s) {
            b.add_state();
            b.set_accept(s, p[z(s)] + 0.5);
        }
        b.set_initial(0, 0.25);
        for (int s = 0; s < n; ++s) {
            for (int i = 0; i < 4; ++i) {
                int t = r.next(n), sym = r.next(3);
                b.add_transition(s, t, sym, c[sym] + p[z(s)] - p[z(t)]);
            }
            int t = (s + 1) % n;
            b.add_epsilon(s, t, p[z(s)] - p[z(t)]);
        }
        auto a = b.build(), d = a.determinize(128), m = d.minimize();
        REQUIRE(d.graph().is_deterministic());
        REQUIRE(m.graph().is_deterministic());
        same_costs(a, d, words);
        same_costs(a, m, words);
        REQUIRE(m.minimize().graph().state_count() == m.graph().state_count());
    }
}

TEST_CASE("cyclic minimization with signed dyadic weights and independent offsets", "[weighted][cyclic]") {
    weighted_automaton::builder b;
    for (int s = 0; s < 3; ++s)
        b.add_state();
    b.set_initial(0, 0.5);
    b.set_accept(1, 1.25);
    b.set_accept(2, -0.75);
    b.add_transition(0, 1, 0, 0.25);
    b.add_transition(0, 2, 1, 0.5);
    b.add_transition(1, 1, interval_set::range(0, 2), -0.5);
    b.add_transition(2, 2, interval_set::range(0, 2), -0.5);
    auto a = b.build(), m = a.minimize();
    REQUIRE(m.graph().state_count() == 2);
    same_costs(a, m, all_words(3, 6));
    REQUIRE(m.optimal_cost() == -inf);
    random_source r(99122);
    auto words = all_words(2, 7);
    for (int round = 0; round < 80; ++round) {
        weighted_automaton::builder c;
        int n = 2 + r.next(5);
        for (int s = 0; s < n; ++s)
            c.add_state();
        c.set_initial(0, 0.25);
        c.set_accept(n - 1, -0.5);
        for (int s = 0; s < n; ++s) {
            if (r.coin())
                c.set_accept(s, r.next(9) / 4.0);
            for (symbol sym = 0; sym < 2; ++sym)
                c.add_transition(s, r.next(n), sym, (r.next(17) - 8) / 4.0);
        }
        auto x = c.build(), y = x.minimize();
        same_costs(x, y, words);
        REQUIRE(y.graph().state_count() == minimal_state_count(x));
        REQUIRE(y.minimize().graph().state_count() == y.graph().state_count());
    }
}

TEST_CASE("k distinct words of ambiguous automata with bounded negative cycles", "[weighted][cyclic]") {
    weighted_automaton::builder b;
    for (int i = 0; i < 3; ++i)
        b.add_state();
    b.set_initial(0, 1.25);
    b.set_accept(1, 0.5);
    b.set_accept(2, 0.25);
    b.add_epsilon(0, 0, 0);
    b.add_transition(0, 1, interval_set::range(0, 2), 0.75);
    b.add_transition(0, 2, interval_set::range(0, 2), 1.5);
    b.add_transition(1, 1, interval_set::range(0, 2), 1);
    b.add_transition(2, 2, interval_set::range(0, 2), 1);
    auto a = b.build();
    auto actual = a.k_best_words(30);
    std::vector<dyadic> reference;
    for (const auto& w : all_words(3, 3))
        if (auto cost = word_cost(a, w))
            reference.push_back(*cost);
    std::ranges::sort(reference);
    std::set<word> seen;
    for (std::size_t i = 0; i < 30; ++i) {
        REQUIRE(seen.insert(actual[i].symbols).second);
        REQUIRE(reference[i].to_double() == actual[i].cost);
        REQUIRE(word_cost(a, actual[i].symbols)->to_double() == actual[i].cost);
    }
    weighted_automaton::builder c;
    c.add_state();
    c.set_initial(0);
    c.set_accept(0);
    c.add_transition(0, 0, interval_set::range(0, 1), -1);
    auto loop = c.build();
    REQUIRE_THROWS_AS(loop.words_by_cost(), std::invalid_argument);
    auto fixed = k_best_words_of_length(loop, 3, 20);
    REQUIRE(fixed.size() == 8);
    for (const auto& w : fixed) {
        REQUIRE(w.length() == 3);
        REQUIRE(w.cost == -3);
    }
}

TEST_CASE("distinct words of random automata against all words", "[weighted][cyclic]") {
    random_source r(99124);
    auto words = all_words(2, 6);
    for (int round = 0; round < 40; ++round) {
        weighted_automaton::builder b;
        for (int s = 0; s < 4; ++s) {
            b.add_state();
            b.set_accept(s);
        }
        b.set_initial(0);
        b.add_epsilon(0, 0, 0);
        for (int s = 0; s < 4; ++s)
            for (symbol c = 0; c < 2; ++c)
                for (int j = 0; j < 2; ++j)
                    b.add_transition(s, r.next(4), c, 1 + r.next(2));
        auto a = b.build();
        std::vector<dyadic> expected;
        for (const auto& w : words)
            expected.push_back(*word_cost(a, w));
        std::ranges::sort(expected);
        // The 15 words of length 3 or less cost at most 6, the words of length 7 or more at least 7
        REQUIRE(expected[14] < dyadic(7.0));
        auto actual = a.k_best_words(15);
        std::set<word> seen;
        for (std::size_t i = 0; i < 15; ++i) {
            REQUIRE(expected[i].to_double() == actual[i].cost);
            REQUIRE(word_cost(a, actual[i].symbols)->to_double() == actual[i].cost);
            REQUIRE(seen.insert(actual[i].symbols).second);
        }
    }
}

TEST_CASE("distinct words of a length with signed weights", "[weighted][cyclic]") {
    random_source r(99123);
    for (int round = 0; round < 70; ++round) {
        weighted_automaton::builder b;
        for (int s = 0; s < 3; ++s)
            b.add_state();
        b.set_initial(0, -0.25);
        b.set_accept(2, 0.5);
        for (int s = 0; s < 3; ++s) {
            b.add_epsilon(s, s, 0);
            for (int j = 0; j < 4; ++j)
                b.add_transition(s, r.next(3), r.next(2), (r.next(17) - 8) / 4.0);
        }
        auto a = b.build();
        std::vector<dyadic> expected;
        for (const auto& w : all_words(2, 3))
            if (w.size() == 3)
                if (auto cost = word_cost(a, w))
                    expected.push_back(*cost);
        std::ranges::sort(expected);
        auto actual = k_best_words_of_length(a, 3, 20);
        REQUIRE(actual.size() == expected.size());
        std::set<word> seen;
        for (std::size_t i = 0; i < actual.size(); ++i) {
            REQUIRE(expected[i].to_double() == actual[i].cost);
            REQUIRE(word_cost(a, actual[i].symbols)->to_double() == actual[i].cost);
            REQUIRE(seen.insert(actual[i].symbols).second);
        }
    }
}

TEST_CASE("budgets, unrepresentable weights, empty automata and immutability", "[weighted][cyclic]") {
    weighted_automaton::builder b;
    for (int s = 0; s < 3; ++s)
        b.add_state();
    b.set_initial(0);
    b.set_accept(1);
    b.set_accept(2);
    b.add_epsilon(0, 1, 0);
    b.add_epsilon(0, 2, 0);
    b.add_transition(1, 1, 1, 0);
    b.add_transition(2, 2, 1, 1);
    auto growing = b.build();
    REQUIRE_THROWS_AS(growing.determinize(8), std::length_error);
    REQUIRE(growing.k_best_words(3).size() == 3);
    REQUIRE(growing.k_best_words(3)[2].cost == 0);
    REQUIRE_THROWS_AS(growing.determinize(0), std::invalid_argument);
    REQUIRE(growing.graph().edge_count() == 4);
    weighted_automaton::builder c;
    c.add_state();
    c.set_initial(0);
    c.set_accept(0);
    c.add_epsilon(0, 0, -0.25);
    REQUIRE_THROWS_AS(c.build().determinize(), std::invalid_argument);
    weighted_automaton::builder d;
    d.add_state();
    d.add_state();
    d.set_initial(0);
    d.set_accept(1, 0.2);
    d.add_epsilon(0, 1, 0.1);
    auto unrepresentable = d.build();
    REQUIRE_THROWS_AS(unrepresentable.determinize(), std::range_error);
    REQUIRE(unrepresentable.k_best_words(1)[0].cost == (dyadic(0.1) + dyadic(0.2)).to_double());
    weighted_automaton::builder e;
    e.add_state();
    e.add_state();
    e.set_initial(0, 0.1);
    e.set_accept(1);
    e.add_transition(0, 1, 1, 0.2);
    REQUIRE_THROWS_AS(e.build().minimize(), std::range_error);
    weighted_automaton empty;
    REQUIRE_FALSE(empty.paths_by_cost().next());
    REQUIRE_FALSE(empty.words_by_cost().next());
    REQUIRE(empty.k_best_words(2).empty());
    REQUIRE(empty.minimize().graph().state_count() == 0);
    weighted_automaton::builder f;
    f.add_state();
    f.set_initial(0);
    f.set_accept(0, std::numeric_limits<double>::max());
    f.add_transition(0, 0, 1, std::numeric_limits<double>::max());
    auto overflow = f.build().paths_by_cost();
    REQUIRE(overflow.next()->cost() == std::numeric_limits<double>::max());
    REQUIRE_THROWS_AS(overflow.next(), std::overflow_error);
}
