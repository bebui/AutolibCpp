// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Automata weighted by vectors : views, lexicographic optima, Pareto fronts, operations and compiled evaluators.

#include <autolib/autolib.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <limits>
#include <random>
#include <set>

using namespace autolib;

namespace {

constexpr double inf = std::numeric_limits<double>::infinity();
using vector = std::vector<double>;

struct random_source {
    std::mt19937 engine;
    explicit random_source(unsigned seed) : engine(seed) {}
    int next(int bound) { return std::uniform_int_distribution<int>(0, bound - 1)(engine); }
    bool coin() { return next(2) == 1; }
};

std::size_t z(std::int64_t i) { return static_cast<std::size_t>(i); }

vector to_vector(std::span<const double> v) { return {v.begin(), v.end()}; }

/// Five parallel paths reading 7, two of them with the same vector.
multi_weighted_automaton choice() {
    multi_weighted_automaton::builder b(2);
    b.add_state();
    b.add_state();
    b.set_initial(0, {1, 2});
    b.set_accept(1, {3, 4});
    b.add_transition(0, 1, 7, {0, 10});
    b.add_transition(0, 1, 7, {5, 5});
    b.add_transition(0, 1, 7, {10, 0});
    b.add_transition(0, 1, 7, {6, 6});
    b.add_transition(0, 1, 7, {5, 5});
    return b.build();
}

std::set<vector> vectors(std::span<const multi_weighted_path> paths) {
    std::set<vector> result;
    for (const auto& p : paths)
        result.insert(to_vector(p.costs()));
    return result;
}

/// The vectors of every accepting path reading a word.
void all_word_vectors(const multi_weighted_automaton& a, state_id s, const word& w, std::size_t position, vector costs,
                      std::vector<vector>& result) {
    const auto& g = a.graph();
    if (position == w.size() && g.is_accept(s)) {
        auto v = costs;
        for (std::size_t d = 0; d < v.size(); ++d)
            v[d] += a.final_weight(s, d);
        result.push_back(v);
    }
    for (edge_id e : g.outgoing(s)) {
        bool eps = g.is_epsilon(e);
        if (!eps && (position == w.size() || !g.accepts(e, w[position])))
            continue;
        auto v = costs;
        for (std::size_t d = 0; d < v.size(); ++d)
            v[d] += a.weight(e, d);
        all_word_vectors(a, g.target(e), w, position + (eps ? 0 : 1), v, result);
    }
}

std::vector<vector> word_vectors(const multi_weighted_automaton& a, const word& w) {
    std::vector<vector> result;
    if (a.graph().initial() != no_state)
        all_word_vectors(a, a.graph().initial(), w, 0, to_vector(a.initial_weights()), result);
    std::ranges::sort(result);
    return result;
}

std::set<vector> as_set(const std::vector<vector>& v) { return {v.begin(), v.end()}; }

}  // namespace

TEST_CASE("scalar views, lexicographic optima and Pareto fronts", "[multi]") {
    auto a = choice();
    REQUIRE(a.scalarize({1, 2}).graph().same_as(a.graph()));
    REQUIRE(a.project(0).graph().same_as(a.graph()));
    REQUIRE(a.project(0)({7}) == 4);
    REQUIRE(a.project(1)({7}) == 6);
    REQUIRE(a.scalarize({1, 2})({7}) == 26);
    REQUIRE(a.scalarize({-1, 1})({7}) == -8);
    REQUIRE(to_vector(a.lexicographic_best_path()->costs()) == vector{4, 16});
    REQUIRE(to_vector(a.lexicographic_best_path({1, 0})->costs()) == vector{14, 6});
    REQUIRE(to_vector(a.select_dimensions({1, 0}).lexicographic_best_path({1, 0})->costs()) == vector{16, 4});
    REQUIRE(a.pareto_front().size() == 3);
    REQUIRE(a.pareto_front(true).size() == 4);
    REQUIRE(a.pareto_front().is_complete());
    REQUIRE_FALSE(a.pareto_front().includes_all_paths());
    REQUIRE(vectors(a.pareto_front().paths()) == vectors(a.pareto_for_word({7}).paths()));
    REQUIRE(a.pareto_for_word({8}).size() == 0);
    for (const auto& p : a.pareto_front().paths()) {
        vector expected = to_vector(a.initial_weights());
        state_id s = p.start();
        for (edge_id e : p.edges()) {
            REQUIRE(s == a.graph().source(e));
            s = a.graph().target(e);
            for (std::size_t d = 0; d < 2; ++d)
                expected[d] += a.weight(e, d);
        }
        for (std::size_t d = 0; d < 2; ++d)
            expected[d] += a.final_weight(s, d);
        REQUIRE(to_vector(p.costs()) == expected);
        REQUIRE(expected[0] + expected[1] == 20);
        REQUIRE(p.symbols() == word{7});
    }
    REQUIRE(dominates(vector{1, 2}, vector{1, 3}));
    REQUIRE_FALSE(dominates(vector{1, 2}, vector{1, 2}));
    REQUIRE_FALSE(dominates(vector{1, 4}, vector{2, 3}));
    REQUIRE_THROWS_AS(dominates(vector{1}, vector{1, 2}), std::invalid_argument);
    REQUIRE_THROWS_AS(a.scalarize({1}), std::invalid_argument);
    REQUIRE_THROWS_AS(a.lexicographic_best_path({0, 0}), std::invalid_argument);
    REQUIRE_THROWS_AS(a.project(2), std::out_of_range);
}

namespace {

struct edge {
    int from, to;
    vector costs;
};

void enumerate(int s, int final_state, const std::vector<edge>& edges, const vector& prefix, std::vector<vector>& result) {
    if (s == final_state)
        result.push_back(prefix);
    for (const auto& e : edges)
        if (e.from == s) {
            auto v = prefix;
            for (std::size_t d = 0; d < v.size(); ++d)
                v[d] += e.costs[d];
            enumerate(e.to, final_state, edges, v, result);
        }
}

std::set<vector> independent_front(const std::vector<vector>& all) {
    std::set<vector> result;
    for (const auto& x : all) {
        bool dominated = std::ranges::any_of(all, [&](const vector& y) {
            bool weak = true, strict = false;
            for (std::size_t d = 0; d < x.size(); ++d) {
                weak &= y[d] <= x[d];
                strict |= y[d] < x[d];
            }
            return weak && strict;
        });
        if (!dominated)
            result.insert(x);
    }
    return result;
}

}  // namespace

TEST_CASE("Pareto fronts agree with an independent exhaustive enumeration", "[multi]") {
    random_source r(902);
    for (int round = 0; round < 180; ++round) {
        int n = 2 + r.next(6);
        auto dims = static_cast<std::size_t>(2 + r.next(2));
        multi_weighted_automaton::builder b(dims);
        for (int s = 0; s < n; ++s)
            b.add_state();
        b.set_initial(0);
        b.set_accept(n - 1);
        std::vector<edge> edges;
        for (int k = 0; k < 16; ++k) {
            int s = r.next(n - 1), t = s + 1 + r.next(n - s - 1);
            vector w(dims);
            for (auto& x : w)
                x = r.next(9) - 4;
            b.add_transition(s, t, 1, w);
            edges.push_back({s, t, w});
        }
        auto a = b.build();
        std::vector<vector> all;
        enumerate(0, n - 1, edges, vector(dims, 0.0), all);
        REQUIRE(independent_front(all) == vectors(a.pareto_front().paths()));
        if (!all.empty()) {
            std::ranges::sort(all);
            REQUIRE(to_vector(a.lexicographic_best_path()->costs()) == all[0]);
            vector coefficients(dims, 1.0);
            coefficients[0] = -2;
            double best = inf;
            for (const auto& v : all) {
                double total = 0;
                for (std::size_t d = 0; d < dims; ++d)
                    total += coefficients[d] * v[d];
                best = std::min(best, total);
            }
            REQUIRE(a.scalarize(coefficients).optimal_cost() == best);
        } else {
            REQUIRE_FALSE(a.lexicographic_best_path());
        }
    }
}

TEST_CASE("operations keep every vector, dominated or not, and independent resources", "[multi]") {
    auto a = choice();
    auto original = as_set(word_vectors(a, {7}));
    REQUIRE(original.size() == 4);
    REQUIRE(as_set(word_vectors(a | a, {7})) == original);
    std::set<vector> sums;
    for (const auto& x : word_vectors(a, {7}))
        for (const auto& y : word_vectors(a, {7}))
            sums.insert({x[0] + y[0], x[1] + y[1]});
    REQUIRE(as_set(word_vectors(a + a, {7, 7})) == sums);
    REQUIRE(as_set(word_vectors(a & a, {7})) == sums);
    REQUIRE(as_set(word_vectors(a.reverse(), {7})) == original);
    REQUIRE(as_set(word_vectors(a.trim(), {7})) == original);
    REQUIRE(as_set(word_vectors(a.simplify(), {7})) == original);
    auto independent = a.product_independent(a);
    REQUIRE(independent.dimensions() == 4);
    REQUIRE(as_set(word_vectors(independent, {7})).size() == 16);
    REQUIRE(as_set(word_vectors(a.concatenate_independent(a), {7, 7})).size() == 16);
    REQUIRE(as_set(word_vectors(a, {7})) == original);
}

TEST_CASE("the epsilon filter of products does not duplicate pairs of paths", "[multi]") {
    multi_weighted_automaton::builder b(1);
    for (int i = 0; i < 4; ++i)
        b.add_state();
    b.set_initial(0);
    b.set_accept(3);
    b.add_epsilon(0, 1, {2});
    b.add_transition(1, 2, 7, {3});
    b.add_epsilon(2, 3, {4});
    auto a = b.build();
    REQUIRE(word_vectors(a & a, {7}) == std::vector<vector>{{18}});
}

TEST_CASE("dimensions, copies, cycles and extreme symbols", "[multi]") {
    REQUIRE_THROWS_AS(multi_weighted_automaton::builder(0), std::invalid_argument);
    multi_weighted_automaton::builder b(2);
    b.add_state();
    b.add_state();
    b.set_initial(0);
    b.set_accept(1);
    vector costs{1, 2};
    b.add_transition(0, 1, interval_set::all(), costs);
    costs[0] = 999;
    auto a = b.build();
    REQUIRE(a.project(0)({min_symbol}) == 1);
    REQUIRE_THROWS_AS(b.add_epsilon(0, 1, {1}), std::invalid_argument);
    REQUIRE_THROWS_AS(a.scalarize({std::nan(""), 1}), std::invalid_argument);
    multi_weighted_automaton::builder loop(1);
    loop.add_state();
    loop.set_initial(0);
    loop.set_accept(0);
    loop.add_transition(0, 0, 1, {-1});
    auto cyclic = loop.build();
    REQUIRE_THROWS_AS(cyclic.pareto_front(), std::invalid_argument);
    REQUIRE(cyclic.pareto_for_word({1, 1}).size() == 1);
    REQUIRE(cyclic.pareto_for_word({1, 1}).paths()[0].cost(0) == -2);
}

TEST_CASE("exact sums prevent false dominance and false lexicographic ties", "[multi]") {
    multi_weighted_automaton::builder b(2);
    for (int i = 0; i < 4; ++i)
        b.add_state();
    b.set_initial(0);
    b.set_accept(3);
    b.add_transition(0, 1, 7, {1e16, 0});
    b.add_transition(1, 2, 7, {1, 1});
    b.add_transition(2, 3, 7, {-1e16, 0});
    b.add_transition(0, 3, 7, {0, 2});
    auto a = b.build();
    REQUIRE(vectors(a.pareto_front().paths()) == std::set<vector>{{1, 1}, {0, 2}});
    REQUIRE(to_vector(a.lexicographic_best_path()->costs()) == vector{0, 2});
    REQUIRE(a.path_costs(*a.project(0).best_path({7, 7, 7})) == vector{1, 1});
    for (const auto& p : a.pareto_front().paths())
        REQUIRE(p.exact_cost(0) == dyadic(p.cost(0)));
}

// Compiled evaluators

TEST_CASE("compiled vector evaluators agree with independent transition tables", "[multi][compiled]") {
    random_source r(99200);
    const symbol low[] = {min_symbol, -1, 0, 4}, high[] = {-2, -1, 3, max_symbol};
    const symbol alphabet[] = {min_symbol, -2, -1, 0, 3, 4, max_symbol};
    for (int round = 0; round < 65; ++round) {
        constexpr int n = 6;
        constexpr std::size_t dims = 3;
        int target[n][4];
        double weights[n][4][dims], finals[n][dims];
        bool accepting[n];
        const vector initial{0.25, -0.5, 1.75};
        multi_weighted_automaton::builder b(dims);
        for (int s = 0; s < n; ++s)
            b.add_state();
        b.set_initial(0, initial);
        for (int s = 0; s < n; ++s) {
            accepting[s] = r.coin();
            for (auto& f : finals[s])
                f = (r.next(17) - 8) / 4.0;
            if (accepting[s])
                b.set_accept(s, finals[s]);
            for (int c = 3; c >= 0; --c) {
                target[s][c] = r.next(5) == 0 ? -1 : r.next(n);
                for (auto& w : weights[s][c])
                    w = (r.next(17) - 8) / 4.0;
                if (target[s][c] >= 0)
                    b.add_transition(s, target[s][c], interval_set::range(low[c], high[c]), weights[s][c]);
            }
        }
        auto a = b.build();
        auto compiled = a.compile();
        REQUIRE(compiled.graph().same_as(a.graph()));
        REQUIRE(compiled.dimensions() == dims);
        vector buffer(dims);
        for (int query = 0; query < 100; ++query) {
            word w(z(r.next(10)));
            int state = 0;
            vector expected = initial;
            for (auto& c : w) {
                c = alphabet[r.next(7)];
                if (state >= 0) {
                    int k = c <= -2 ? 0 : c == -1 ? 1 : c <= 3 ? 2 : 3;
                    if (target[state][k] >= 0) {
                        for (std::size_t d = 0; d < dims; ++d)
                            expected[d] += weights[state][k][d];
                        state = target[state][k];
                    } else {
                        state = -1;
                    }
                }
            }
            bool accepted = state >= 0 && accepting[state];
            if (accepted)
                for (std::size_t d = 0; d < dims; ++d)
                    expected[d] += finals[state][d];
            REQUIRE(compiled.costs(w, buffer) == accepted);
            if (accepted) {
                REQUIRE(buffer == expected);
                REQUIRE(*compiled.costs(w) == expected);
                for (std::size_t d = 0; d < dims; ++d)
                    REQUIRE(compiled.project(d)(w) == expected[d]);
            } else {
                REQUIRE_FALSE(compiled.costs(w));
                REQUIRE(buffer == vector(dims, inf));
            }
        }
    }
}

TEST_CASE("weightings of one graph are correlated views", "[multi][compiled]") {
    weighted_automaton::builder b;
    b.add_state();
    b.add_state();
    b.set_initial(0, 2);
    b.set_accept(1, 3);
    b.add_transition(0, 1, interval_set::range(-5, 7), 1);
    auto a = b.build();
    auto other = weighted_automaton::of(a.graph(), {9}, -2, {0, 4});
    auto multi = multi_weighted_automaton::from_weightings({a, other});
    REQUIRE(multi.graph().same_as(a.graph()));
    REQUIRE(multi.weight(0, 0) == 1);
    REQUIRE(multi.weight(0, 1) == 9);
    REQUIRE(*multi.compile().costs({0}) == vector{6, 11});
    REQUIRE(multi.compile().scalarize({1, 1})({0}) == 17);
    REQUIRE_THROWS_AS(multi_weighted_automaton::from_weightings({a, b.build()}), std::invalid_argument);
    REQUIRE_THROWS_AS(multi_weighted_automaton::from_weightings(std::span<const weighted_automaton>()), std::invalid_argument);
}

TEST_CASE("independent products differ from correlated views of the same paths", "[multi][compiled]") {
    weighted_automaton::builder b;
    b.add_state();
    b.add_state();
    b.set_initial(0);
    b.set_accept(1);
    b.add_transition(0, 1, 1, 0);
    b.add_transition(0, 1, 1, 10);
    auto a = b.build(), c = a.with_weights({10, 0});
    auto correlated = multi_weighted_automaton::from_weightings({a, c});
    auto independent = multi_weighted_automaton::synchronized_product({a, c});
    REQUIRE(word_vectors(correlated, {1}) == std::vector<vector>{{0, 10}, {10, 0}});
    REQUIRE(word_vectors(independent, {1}) == std::vector<vector>{{0, 0}, {0, 10}, {10, 0}, {10, 10}});
    REQUIRE(correlated.scalarize({1, 1})({1}) == 10);
    REQUIRE(independent.scalarize({1, 1})({1}) == 0);
    REQUIRE_THROWS_AS(correlated.compile(), std::invalid_argument);
}

namespace {

void scalar_paths(const weighted_automaton& a, state_id s, const word& w, std::size_t at, double cost, std::vector<double>& result) {
    if (at == w.size() && a.graph().is_accept(s))
        result.push_back(cost + a.final_weight(s));
    for (edge_id e : a.graph().outgoing(s)) {
        bool eps = a.graph().is_epsilon(e);
        if (eps || (at < w.size() && a.graph().accepts(e, w[at])))
            scalar_paths(a, a.graph().target(e), w, at + (eps ? 0 : 1), cost + a.weight(e), result);
    }
}

}  // namespace

TEST_CASE("independent products keep every combination of paths, epsilon included", "[multi][compiled]") {
    random_source r(99201);
    for (int round = 0; round < 25; ++round) {
        std::vector<weighted_automaton> operands;
        for (int k = 0; k < 3; ++k) {
            weighted_automaton::builder b;
            for (int s = 0; s < 4; ++s)
                b.add_state();
            b.set_initial(0, k / 4.0);
            b.set_accept(3, -k / 4.0);
            for (int s = 0; s < 3; ++s) {
                b.add_epsilon(s, s + 1, r.next(5) / 4.0);
                for (int j = 0; j < 2; ++j)
                    b.add_transition(s, s + 1, interval_set::range(0, 1), (r.next(9) - 4) / 4.0);
            }
            operands.push_back(b.build());
        }
        auto product = multi_weighted_automaton::synchronized_product(operands);
        for (const word& w : std::vector<word>{{}, {0}, {1}, {0, 1}, {1, 0}, {0, 1, 0}}) {
            std::vector<std::vector<double>> costs;
            for (const auto& a : operands) {
                std::vector<double> paths;
                scalar_paths(a, 0, w, 0, a.initial_weight(), paths);
                costs.push_back(paths);
            }
            std::vector<vector> expected;
            for (double x : costs[0])
                for (double y : costs[1])
                    for (double t : costs[2])
                        expected.push_back({x, y, t});
            std::ranges::sort(expected);
            REQUIRE(word_vectors(product, w) == expected);
        }
    }
    REQUIRE_THROWS_AS(multi_weighted_automaton::synchronized_product(std::span<const weighted_automaton>()), std::invalid_argument);
}

TEST_CASE("compiled evaluators reweight, slice, project and scalarize", "[multi][compiled]") {
    multi_weighted_automaton::builder b(2);
    b.add_state();
    b.add_state();
    b.set_initial(0, {1, 2});
    b.set_accept(1, {3, 4});
    b.add_transition(0, 1, interval_set::all(), {0.5, -0.25});
    auto a = b.build();
    auto compiled = a.compile();
    vector out(2);
    word text{99, max_symbol, 99};
    REQUIRE(compiled.costs(std::span(text).subspan(1, 1), out));
    REQUIRE(out == vector{4.5, 5.75});
    REQUIRE(*compiled.costs(std::span(text).subspan(1, 1)) == out);
    REQUIRE(compiled.scalarize({1, -2})({max_symbol}) == 4.5 - 2 * 5.75);
    auto changed = multi_weighted_automaton::of(a.graph(), 3, {7, 8, 9});
    auto reweighted = compiled.reweight(changed);
    REQUIRE(reweighted.dimensions() == 3);
    REQUIRE(*reweighted.costs({1}) == vector{7, 8, 9});
    REQUIRE(*compiled.costs({1}) == vector{4.5, 5.75});
    REQUIRE_THROWS_AS(compiled.reweight(b.build()), std::invalid_argument);
    vector small(1);
    REQUIRE_THROWS_AS(compiled.costs(text, small), std::invalid_argument);
    REQUIRE_THROWS_AS(compiled.project(2), std::out_of_range);
    REQUIRE_THROWS_AS(compiled.scalarize({1}), std::invalid_argument);
    REQUIRE(compiled.transition(no_state, 0) == -1);
    REQUIRE(compiled.transition(0, min_symbol) == 0);
    REQUIRE_FALSE(compiled.costs({}));
    REQUIRE_FALSE(compiled.costs({1, 2}));
}

TEST_CASE("compiled evaluators reject epsilon edges and check overflows", "[multi][compiled]") {
    multi_weighted_automaton::builder b(1);
    b.add_state();
    b.set_initial(0);
    b.set_accept(0);
    b.add_epsilon(0, 0, {0});
    REQUIRE_THROWS_AS(b.build().compile(), std::invalid_argument);
    multi_weighted_automaton::builder empty(2);
    vector out{1, 2};
    REQUIRE_FALSE(empty.build().compile().costs({}, out));
    REQUIRE_FALSE(empty.build().compile().costs({}));
    REQUIRE(out == vector{inf, inf});
    multi_weighted_automaton::builder c(1);
    c.add_state();
    c.set_initial(0, {std::numeric_limits<double>::max()});
    c.set_accept(0);
    c.add_transition(0, 0, 1, {std::numeric_limits<double>::max()});
    auto overflow = c.build().compile();
    REQUIRE_THROWS_AS(overflow.costs({1}), std::overflow_error);
}
