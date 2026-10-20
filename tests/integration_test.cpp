// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Automata and their weightings : snapshots shared by several costs, membership independent of the costs, and
// projections back to automata.

#include <autolib/autolib.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <random>

using namespace autolib;
using namespace autolib::literals;

namespace {

double constant(double value, const automaton_edge&) { return value; }
edge_cost costs(double value) {
    return [value](const automaton_edge& e) { return constant(value, e); };
}

}  // namespace

TEST_CASE("automata are weighted directly", "[integration]") {
    auto source = "12+"_re;
    auto scalar = source.weighted(costs(2));
    auto vector = source.multi_weighted({costs(2), costs(5)});
    REQUIRE(scalar({1, 2, 2}) == 6);
    REQUIRE(vector.project(1)({1, 2, 2}) == 15);
    REQUIRE(scalar.run({1, 2}));
    REQUIRE_FALSE(vector.run({1}));
    REQUIRE(scalar.to_automaton() == source);
    REQUIRE(vector.to_automaton() == source);
    REQUIRE(weighted_automaton::from(source, costs(2))({1, 2, 2}) == 6);
    REQUIRE(multi_weighted_automaton::from(source, {costs(2), costs(5)}).project(1)({1, 2, 2}) == 15);
    // Each direct weighting captures its own snapshot
    REQUIRE_FALSE(source.weighted(costs(1)).graph().same_as(source.weighted(costs(1)).graph()));
}

TEST_CASE("a snapshot keeps its origins and boundaries and shares its graph after the source changes", "[integration]") {
    automaton source;
    auto end = source.add_state(), start = source.add_state(), middle = source.add_state();
    source.set_initial(start);
    source.set_accept(end);
    source.add_transition(start, middle, 1);
    source.add_transition(middle, end, 2);
    auto snapshot = source.snapshot();
    auto first = snapshot.origin(0);
    REQUIRE(first.from == start);
    REQUIRE(first.to == middle);
    REQUIRE_FALSE(first.epsilon);
    REQUIRE(first.labels == interval_set(1));
    REQUIRE(first.index == 0);
    std::vector<state_id> finals;
    auto cost = snapshot.weighted(costs(2), 0.5, [&](state_id s) {
        finals.push_back(s);
        return 1.5;
    });
    REQUIRE(finals == std::vector<state_id>{end});
    auto duration = snapshot.weighted(costs(7));
    auto vectors = snapshot.multi_weighted({costs(2), costs(7)});
    REQUIRE(snapshot.graph().same_as(cost.graph()));
    REQUIRE(cost.graph().same_as(duration.graph()));
    REQUIRE(cost.graph().same_as(vectors.graph()));
    REQUIRE(cost.graph().same_as(multi_weighted_automaton::from_weightings({cost, duration}).graph()));
    REQUIRE(cost({1, 2}) == 6);
    REQUIRE(duration({1, 2}) == 14);
    // Later changes of the source do not reach the snapshot
    source.set_accept(end, false);
    source.add_transition(start, end, 9);
    REQUIRE(snapshot.run({1, 2}));
    REQUIRE_FALSE(snapshot.run({9}));
    REQUIRE_FALSE(source({1, 2}));
    REQUIRE(cost({1, 2}) == 6);
    auto exported = snapshot.to_automaton();
    exported.set_accept(exported.initial());
    REQUIRE(exported({}));
    REQUIRE_FALSE(snapshot.run({}));
    REQUIRE_THROWS_AS(snapshot.origin(2), std::out_of_range);
}

TEST_CASE("epsilon and symbol transitions receive their own costs", "[integration]") {
    automaton source;
    auto start = source.add_state(), end = source.add_state(true);
    source.set_initial(start);
    source.add_epsilon(start, end);
    source.add_transition(start, end, interval_set::range(4, 8));
    auto snapshot = source.snapshot();
    REQUIRE(snapshot.graph().edge_count() == 2);
    REQUIRE_FALSE(snapshot.origin(0).epsilon);
    REQUIRE(snapshot.origin(1).epsilon);
    REQUIRE(snapshot.origin(1).labels.empty());
    auto weighted = snapshot.weighted([](const automaton_edge& e) { return e.epsilon ? -3.0 : 7.0; }, 2, [](state_id) { return 0.5; });
    REQUIRE(weighted({}) == -0.5);
    REQUIRE(weighted({5}) == 9.5);
    auto projected = weighted.to_automaton();
    REQUIRE(projected({}));
    REQUIRE(projected({5}));
    REQUIRE_FALSE(projected({5, 5}));
    REQUIRE(projected.transitions(start).size() == 1);
    REQUIRE(projected.epsilons(start).size() == 1);
}

TEST_CASE("vector costs stay correlated along each path of a non-deterministic automaton", "[integration]") {
    automaton source;
    auto s = source.add_state(), left = source.add_state(), right = source.add_state(), end = source.add_state(true);
    source.set_initial(s);
    source.add_transition(s, left, 7);
    source.add_transition(s, right, 7);
    source.add_transition(left, end, 8);
    source.add_transition(right, end, 8);
    auto multi = source.multi_weighted({[&](const automaton_edge& e) { return e.from == s ? (e.to == left ? 1.0 : 9.0) : 0.0; },
                                        [&](const automaton_edge& e) { return e.from == s ? (e.to == left ? 9.0 : 1.0) : 0.0; }});
    REQUIRE(multi.scalarize({1, 1})({7, 8}) == 10);
    REQUIRE(multi.pareto_for_word({7, 8}).size() == 2);
    REQUIRE(multi.run({7, 8}));
}

TEST_CASE("membership ignores overflows, negative cycles and parallel costs", "[integration]") {
    weighted_automaton::builder b;
    b.add_state();
    b.set_initial(0);
    b.set_accept(0);
    b.add_transition(0, 0, interval_set::all(), std::numeric_limits<double>::max());
    auto huge = b.build();
    REQUIRE_THROWS_AS(huge({1, 2}), std::overflow_error);
    REQUIRE(huge.run({min_symbol, max_symbol}));
    b.add_epsilon(0, 0, -1);
    auto unbounded = b.build();
    REQUIRE(unbounded({1}) == -std::numeric_limits<double>::infinity());
    REQUIRE(unbounded.run({1}));
    REQUIRE(unbounded.to_automaton()({1}));
    REQUIRE(multi_weighted_automaton::from_weightings({unbounded, unbounded}).run({1, 2, 3}));
    weighted_automaton::builder parallel;
    parallel.add_state();
    parallel.add_state();
    parallel.set_initial(0);
    parallel.set_accept(1);
    parallel.add_transition(0, 1, 7, 2);
    parallel.add_transition(0, 1, 7, 9);
    auto a = parallel.build();
    auto copy = a.to_automaton();
    REQUIRE(copy.transitions(0).size() == 1);
    REQUIRE(a.graph().edge_count() == 2);
    REQUIRE(a({7}) == 2);
    REQUIRE(a.run({7}));
}

namespace {

struct arc {
    int from, to;
    symbol low, high;
    bool epsilon;
};

void close(std::vector<std::vector<bool>>& relation) {
    auto n = relation.size();
    for (std::size_t k = 0; k < n; ++k)
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j)
                if (relation[i][k] && relation[k][j])
                    relation[i][j] = true;
}

/// Membership by an independent closure of the epsilon transitions, as boolean matrices.
bool accepts(std::size_t n, int initial, const std::vector<bool>& finals, const std::vector<arc>& arcs, const word& w) {
    if (initial < 0)
        return false;
    std::vector<std::vector<bool>> closure(n, std::vector<bool>(n));
    for (std::size_t s = 0; s < n; ++s)
        closure[s][s] = true;
    for (const auto& a : arcs)
        if (a.epsilon)
            closure[static_cast<std::size_t>(a.from)][static_cast<std::size_t>(a.to)] = true;
    close(closure);
    auto active = closure[static_cast<std::size_t>(initial)];
    for (symbol c : w) {
        std::vector<bool> next(n);
        for (const auto& a : arcs)
            if (!a.epsilon && active[static_cast<std::size_t>(a.from)] && c >= a.low && c <= a.high)
                for (std::size_t t = 0; t < n; ++t)
                    if (closure[static_cast<std::size_t>(a.to)][t])
                        next[t] = true;
        active = next;
    }
    for (std::size_t s = 0; s < n; ++s)
        if (active[s] && finals[s])
            return true;
    return false;
}

}  // namespace

TEST_CASE("membership and reachability of random graphs agree with matrix oracles", "[integration][random]") {
    std::mt19937 rng(817593);
    auto next = [&](int bound) { return std::uniform_int_distribution<int>(0, bound - 1)(rng); };
    std::vector<word> words{{}};
    std::size_t from = 0;
    for (int length = 1; length <= 3; ++length) {
        auto to = words.size();
        for (auto i = from; i < to; ++i)
            for (symbol c : {min_symbol, -1, 0, 1, max_symbol}) {
                auto w = words[i];
                w.push_back(c);
                words.push_back(w);
            }
        from = to;
    }
    for (int round = 0; round < 70; ++round) {
        auto n = static_cast<std::size_t>(1 + next(7));
        int initial = next(5) == 0 ? -1 : next(static_cast<int>(n));
        std::vector<bool> finals(n);
        automaton_graph::builder builder;
        for (std::size_t s = 0; s < n; ++s) {
            finals[s] = next(2) == 1;
            builder.add_state(finals[s]);
        }
        if (initial >= 0)
            builder.set_initial(initial);
        std::vector<arc> arcs;
        for (int e = 0; e < 18; ++e) {
            int a = next(static_cast<int>(n)), b = next(static_cast<int>(n));
            symbol low = next(5) - 2, high = low + next(3);
            bool epsilon = next(4) == 0;
            arcs.push_back({a, b, low, high, epsilon});
            if (epsilon)
                builder.add_epsilon(a, b);
            else
                builder.add_transition(a, b, interval_set::range(low, high));
        }
        auto graph = builder.build();
        auto projected = graph.to_automaton();
        auto snapshot = projected.snapshot();
        auto scalar = weighted_automaton::of(graph, std::vector<double>(arcs.size(), 0.0));
        auto multi = multi_weighted_automaton::from_weightings({scalar});
        for (const auto& w : words) {
            bool expected = accepts(n, initial, finals, arcs, w);
            REQUIRE(graph.run(w) == expected);
            REQUIRE(projected(w) == expected);
            REQUIRE(snapshot.run(w) == expected);
            REQUIRE(scalar.run(w) == expected);
            REQUIRE(multi.run(w) == expected);
        }
        std::vector<std::vector<bool>> reach(n, std::vector<bool>(n));
        for (std::size_t s = 0; s < n; ++s)
            reach[s][s] = true;
        for (const auto& a : arcs)
            reach[static_cast<std::size_t>(a.from)][static_cast<std::size_t>(a.to)] = true;
        close(reach);
        auto coaccessible = graph.coaccessible_states(), useful = graph.useful_states();
        for (std::size_t s = 0; s < n; ++s) {
            bool expected = false;
            for (std::size_t t = 0; t < n; ++t)
                expected = expected || (finals[t] && reach[s][t]);
            REQUIRE(coaccessible[s] == expected);
            REQUIRE(useful[s] == (initial >= 0 && reach[static_cast<std::size_t>(initial)][s] && expected));
        }
    }
}

TEST_CASE("empty automata and invalid costs invent no path nor weight", "[integration]") {
    automaton empty;
    auto snapshot = empty.snapshot();
    REQUIRE_FALSE(snapshot.run({}));
    REQUIRE_FALSE(empty.weighted(costs(0)).run({}));
    REQUIRE_FALSE(empty.multi_weighted({costs(0)}).run({}));
    REQUIRE(snapshot.to_automaton().is_empty());
    REQUIRE(snapshot.graph().coaccessible_states().empty());
    REQUIRE_THROWS_AS(empty.weighted(edge_cost()), std::invalid_argument);
    REQUIRE_THROWS_AS(snapshot.multi_weighted(std::span<const edge_cost>()), std::invalid_argument);
    REQUIRE_THROWS_AS(snapshot.multi_weighted({edge_cost()}), std::invalid_argument);
    REQUIRE_THROWS_AS(snapshot.weighted(costs(0), std::nan(""), nullptr), std::invalid_argument);
    auto source = "1"_re;
    REQUIRE_THROWS_AS(source.weighted(costs(std::numeric_limits<double>::infinity())), std::invalid_argument);
    REQUIRE_THROWS_AS(source.multi_weighted({costs(0), costs(std::nan(""))}), std::invalid_argument);
}
