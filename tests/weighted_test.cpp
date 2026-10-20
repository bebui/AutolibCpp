// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Weighted automata on DAGs and small cyclic graphs, against exhaustive enumerations.

#include <autolib/autolib.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <string>

using namespace autolib;

namespace {

constexpr double inf = std::numeric_limits<double>::infinity();

struct random_source {
    std::mt19937 engine;
    explicit random_source(unsigned seed) : engine(seed) {}
    int next(int bound) { return std::uniform_int_distribution<int>(0, bound - 1)(engine); }
    bool coin() { return next(2) == 1; }
};

struct arc {
    int from, to;
    symbol low, high;
    double cost;
    bool epsilon;
};

struct fixture {
    weighted_automaton a;
    std::vector<arc> arcs;
    std::vector<bool> finals;
    std::vector<double> terminal;
    double initial;
};

/// The best cost from a state by enumerating every path, on the whole word or freely when there is none.
double exhaustive(const fixture& f, int s, const word* w, std::size_t position, double cost) {
    double best = f.finals[static_cast<std::size_t>(s)] && (w == nullptr || position == w->size())
                      ? cost + f.terminal[static_cast<std::size_t>(s)]
                      : inf;
    for (const auto& e : f.arcs) {
        if (e.from != s)
            continue;
        if (e.epsilon)
            best = std::min(best, exhaustive(f, e.to, w, position, cost + e.cost));
        else if (w == nullptr || (position < w->size() && (*w)[position] >= e.low && (*w)[position] <= e.high))
            best = std::min(best, exhaustive(f, e.to, w, position + 1, cost + e.cost));
    }
    return best;
}

/// Every word over the symbols up to a length.
std::vector<word> all_words(std::initializer_list<symbol> symbols, int max_length) {
    std::vector<word> result{{}};
    std::size_t start = 0;
    for (int l = 1; l <= max_length; ++l) {
        auto end = result.size();
        for (auto i = start; i < end; ++i)
            for (symbol c : symbols) {
                auto w = result[i];
                w.push_back(c);
                result.push_back(w);
            }
        start = end;
    }
    return result;
}

fixture dag(random_source& r) {
    int n = 2 + r.next(6);
    weighted_automaton::builder b;
    std::vector<bool> finals(static_cast<std::size_t>(n));
    std::vector<double> terminal(static_cast<std::size_t>(n));
    double initial = r.next(7) - 3;
    for (int s = 0; s < n; ++s) {
        b.add_state();
        finals[static_cast<std::size_t>(s)] = r.coin();
        terminal[static_cast<std::size_t>(s)] = r.next(7) - 3;
        if (finals[static_cast<std::size_t>(s)])
            b.set_accept(s, terminal[static_cast<std::size_t>(s)]);
    }
    b.set_initial(0, initial);
    std::vector<arc> arcs;
    for (int i = 0; i < 15; ++i) {
        int from = r.next(n - 1), to = from + 1 + r.next(n - from - 1);
        symbol lo = r.next(4) - 1, hi = lo + r.next(3);
        double cost = r.next(11) - 5;
        bool eps = r.next(5) == 0;
        if (eps)
            b.add_epsilon(from, to, cost);
        else
            b.add_transition(from, to, interval_set::range(lo, hi), cost);
        arcs.push_back({from, to, lo, hi, cost, eps});
    }
    return {b.build(), arcs, finals, terminal, initial};
}

void validate_path(const weighted_automaton& a, const std::optional<weighted_path>& p, double expected, bool terminal) {
    if (expected == inf) {
        REQUIRE_FALSE(p);
        return;
    }
    REQUIRE(p);
    state_id s = p->start();
    std::size_t position = 0;
    double sum = a.initial_weight();
    const auto& w = p->symbols();
    for (edge_id e : p->edges()) {
        REQUIRE(s == a.graph().source(e));
        if (!a.graph().is_epsilon(e))
            REQUIRE(a.graph().accepts(e, w[position++]));
        sum += a.weight(e);
        s = a.graph().target(e);
    }
    REQUIRE(position == w.size());
    REQUIRE(s == p->end());
    if (terminal) {
        REQUIRE(a.graph().is_accept(s));
        sum += a.final_weight(s);
    }
    REQUIRE(sum == expected);
    REQUIRE(p->cost() == expected);
}

}  // namespace

TEST_CASE("graphs keep parallel edges and their numbers", "[weighted]") {
    automaton_graph::builder b;
    auto p = b.add_state(), q = b.add_state();
    b.set_initial(p);
    b.set_accept(q);
    REQUIRE(b.add_transition(p, q, interval_set::all()) == 0);
    REQUIRE(b.add_transition(p, q, 4) == 1);
    auto g = b.build();
    b.add_state();
    REQUIRE(g.state_count() == 2);
    REQUIRE(g.edge_count() == 2);
    REQUIRE_FALSE(g.is_deterministic());
    std::vector<double> weights{3.5, -2};
    auto a = weighted_automaton::of(g, weights);
    weights[1] = 999;
    REQUIRE(a({4}) == -2);
    REQUIRE(a({min_symbol}) == 3.5);
    REQUIRE(a({max_symbol}) == 3.5);
    REQUIRE(a.with_weights({1, 2}).graph().same_as(g));
    REQUIRE(a({4}) == -2);
    REQUIRE(g.topological_order().size() == 2);
    REQUIRE_THROWS_AS(b.add_transition(p, q, interval_set()), std::invalid_argument);
    REQUIRE_THROWS_AS(g.is_accept(5), std::out_of_range);
    REQUIRE_THROWS_AS(g.source(2), std::out_of_range);
    REQUIRE_FALSE(automaton_graph().same_as(automaton_graph::builder().build()));

    // Import of an automaton : transitions, then epsilon transitions
    automaton source;
    auto s = source.add_state(), t = source.add_state(true);
    source.set_initial(s);
    source.add_transition(s, t, 4);
    source.add_epsilon(s, t);
    auto snapshot = weighted_automaton::from(source, [](const automaton_edge& e) { return e.epsilon ? 2.0 : 3.0; });
    REQUIRE(snapshot.graph().edge_count() == 2);
    REQUIRE(snapshot.graph().is_epsilon(1));
    REQUIRE(snapshot({4}) == 3);
    REQUIRE(snapshot({}) == 2);
}

TEST_CASE("costs, distances and witnesses of DAGs", "[weighted]") {
    random_source r(731);
    auto words = all_words({-1, 0, 2}, 3);
    for (int iteration = 0; iteration < 180; ++iteration) {
        auto f = dag(r);
        const auto& a = f.a;
        double expected = exhaustive(f, 0, nullptr, 0, f.initial);
        REQUIRE(a.optimal_cost() == expected);
        validate_path(a, a.best_path(), expected, true);
        auto evaluator = a.evaluator();
        for (const auto& w : words) {
            double cost = exhaustive(f, 0, &w, 0, f.initial);
            REQUIRE(a(w) == cost);
            REQUIRE(evaluator.cost(w) == cost);
            if (iteration < 30) {
                auto path = a.best_path(w);
                validate_path(a, path, cost, true);
                if (path)
                    REQUIRE(path->symbols() == w);
            }
        }
        auto backward = backward_distances(a);
        for (std::size_t s = 0; s < f.finals.size(); ++s)
            REQUIRE(exhaustive(f, static_cast<int>(s), nullptr, 0, 0) == backward[s]);
    }
}

TEST_CASE("distances of cyclic graphs agree with Floyd-Warshall", "[weighted]") {
    random_source r(732);
    for (int iteration = 0; iteration < 240; ++iteration) {
        int n = 2 + r.next(6);
        auto un = static_cast<std::size_t>(n);
        std::vector<std::vector<double>> distance(un, std::vector<double>(un, inf));
        weighted_automaton::builder b;
        for (std::size_t s = 0; s < un; ++s) {
            b.add_state();
            distance[s][s] = 0;
        }
        b.set_initial(0, 2);
        b.set_accept(n - 1, -3);
        for (int i = 0; i < 3 * n; ++i) {
            auto s = static_cast<std::size_t>(r.next(n)), t = static_cast<std::size_t>(r.next(n));
            double w = r.next(11) - (iteration % 2 == 0 ? 0 : 5);
            b.add_transition(static_cast<state_id>(s), static_cast<state_id>(t), i, w);
            distance[s][t] = std::min(distance[s][t], w);
        }
        for (std::size_t k = 0; k < un; ++k)
            for (std::size_t s = 0; s < un; ++s)
                for (std::size_t t = 0; t < un; ++t)
                    distance[s][t] = std::min(distance[s][t], distance[s][k] + distance[k][t]);
        auto a = b.build();
        auto actual = bellman_ford(a);
        for (std::size_t s = 0; s < un; ++s) {
            double expected = distance[0][s] == inf ? inf : 2 + distance[0][s];
            for (std::size_t k = 0; k < un; ++k)
                if (distance[0][k] != inf && distance[k][k] < 0 && distance[k][s] != inf)
                    expected = -inf;
            auto state = static_cast<state_id>(s);
            REQUIRE(actual.distance_to(state) == expected);
            REQUIRE(shortest_paths(a).distance_to(state) == expected);
            if (std::isfinite(expected))
                validate_path(a, actual.path_to(state), expected, false);
        }
        if (iteration % 2 == 0)
            REQUIRE(actual.distances() == dijkstra(a).distances());
    }
}

TEST_CASE("negative epsilon cycles make costs unbounded, word by word", "[weighted]") {
    weighted_automaton::builder b;
    for (int i = 0; i < 4; ++i)
        b.add_state();
    b.set_initial(0);
    b.set_accept(3);
    b.add_epsilon(0, 1, 0);
    b.add_epsilon(1, 1, -1);
    b.add_transition(1, 3, 1, 0);
    b.add_transition(0, 3, 2, 5);
    auto a = b.build();
    REQUIRE(a({1}) == -inf);
    REQUIRE(a({2}) == 5);
    REQUIRE(a({3}) == inf);
    REQUIRE_THROWS_AS(a.best_path({1}), std::logic_error);
    REQUIRE(a.best_path({2})->cost() == 5);
    REQUIRE(shortest_paths(a).status_to(3) == shortest_path_result::status::unbounded);
    REQUIRE_THROWS_AS(dijkstra(a), std::invalid_argument);
    REQUIRE_THROWS_AS(a.graph().topological_order(), std::invalid_argument);

    // A cycle of cost zero whose sum in doubles would not be zero
    weighted_automaton::builder zero;
    for (int i = 0; i < 5; ++i)
        zero.add_state();
    zero.set_initial(0);
    zero.set_accept(4);
    zero.add_epsilon(0, 1, 1e16);
    zero.add_epsilon(1, 2, 1);
    zero.add_epsilon(2, 3, -1e16);
    zero.add_epsilon(3, 0, -1);
    zero.add_transition(3, 4, 7, 0);
    auto z = zero.build();
    REQUIRE(z({7}) == 1);
    REQUIRE(z.optimal_cost() == 1);
    REQUIRE(z.best_path({7})->cost() == 1);

    weighted_automaton::builder tiny;
    tiny.add_state();
    tiny.set_initial(0);
    tiny.set_accept(0);
    tiny.add_epsilon(0, 0, -std::numeric_limits<double>::denorm_min());
    REQUIRE(tiny.build()({}) == -inf);
}

TEST_CASE("boundaries, empty automata, longest paths and costs through", "[weighted]") {
    weighted_automaton::builder b;
    for (int i = 0; i < 4; ++i)
        b.add_state();
    b.set_initial(0, 2);
    b.set_accept(2, 3);
    b.add_transition(0, 1, 4, -5);
    b.add_transition(1, 2, 5, 1);
    b.add_transition(0, 2, 9, 4);
    auto a = b.build();
    REQUIRE(a({4, 5}) == 1);
    REQUIRE(dag_longest_paths(a).optimal_cost() == 9);
    REQUIRE(dag_longest_paths(a).best_path()->symbols() == word{9});
    REQUIRE(cost_through_state(a, 1) == 1);
    REQUIRE(cost_through_edge(a, 2) == 9);
    REQUIRE(path_analysis(a).cost_through_state(1) == 1);
    REQUIRE(path_analysis(a).cost_through_edge(2) == 9);
    REQUIRE(a.optimal_cost_with_prefix(word{4}) == 1);
    REQUIRE(a.optimal_cost_with_prefix(word{9}) == 9);
    REQUIRE(a.optimal_cost_with_prefix(word{8}) == inf);
    REQUIRE(cost_through_state(a, 3) == inf);
    REQUIRE(a.graph().useful_states() == std::vector<bool>{true, true, true, false});
    REQUIRE(distances_to(a, 2)[0] == -4);
    weighted_automaton empty;
    REQUIRE(empty({}) == inf);
    REQUIRE(empty.optimal_cost() == inf);
    REQUIRE_FALSE(empty.best_path());
    REQUIRE_THROWS_AS(shortest_paths(a, false).best_path(), std::logic_error);
}

TEST_CASE("operations preserve the costs of the words", "[weighted]") {
    random_source r(733);
    auto words = all_words({-1, 0, 2}, 3);
    for (int iteration = 0; iteration < 50; ++iteration) {
        auto fa = dag(r), fb = dag(r);
        const auto &a = fa.a, &b = fb.a;
        auto both = a | b, concatenated = a + b, product = a & b, reversed = a.reverse(), simple = a.simplify();
        for (const auto& w : words) {
            double ca = exhaustive(fa, 0, &w, 0, fa.initial), cb = exhaustive(fb, 0, &w, 0, fb.initial);
            REQUIRE(both(w) == std::min(ca, cb));
            REQUIRE(product(w) == (ca == inf || cb == inf ? inf : ca + cb));
            double expected = inf;
            for (std::size_t i = 0; i <= w.size(); ++i) {
                word left(w.begin(), w.begin() + static_cast<std::ptrdiff_t>(i)), right(w.begin() + static_cast<std::ptrdiff_t>(i), w.end());
                expected = std::min(expected, exhaustive(fa, 0, &left, 0, fa.initial) + exhaustive(fb, 0, &right, 0, fb.initial));
            }
            REQUIRE(concatenated(w) == expected);
            word mirror(w.rbegin(), w.rend());
            REQUIRE(reversed(mirror) == ca);
            REQUIRE(simple(w) == ca);
            REQUIRE(a(w) == ca);
        }
    }
}

TEST_CASE("repetitions and an improving empty word", "[weighted]") {
    weighted_automaton::builder b;
    b.add_state();
    b.add_state();
    b.set_initial(0, 1);
    b.set_accept(1, 2);
    b.add_transition(0, 1, 4, -1);
    auto a = b.build();
    REQUIRE((*a)({}) == 0);
    REQUIRE((*a)({4, 4, 4}) == 6);
    REQUIRE((+a)({4, 4}) == 4);
    REQUIRE((+a)({}) == inf);
    auto range = a.repeat(2, 3);
    REQUIRE(range({4}) == inf);
    REQUIRE(range({4, 4}) == 4);
    REQUIRE(range({4, 4, 4}) == 6);
    REQUIRE(range({4, 4, 4, 4}) == inf);
    REQUIRE_THROWS_AS(a.repeat(3, 2), std::invalid_argument);
    weighted_automaton::builder c;
    c.add_state();
    c.set_initial(0);
    c.set_accept(0, -1);
    REQUIRE(c.build().star()({}) == -inf);
}

TEST_CASE("infinite weights and overflows are rejected", "[weighted]") {
    automaton_graph::builder g;
    g.add_state();
    g.set_initial(0);
    g.set_accept(0);
    g.add_transition(0, 0, 1);
    for (double bad : {std::nan(""), inf, -inf})
        REQUIRE_THROWS_AS(weighted_automaton::of(g.build(), {bad}), std::invalid_argument);
    REQUIRE_THROWS_AS(weighted_automaton::of(g.build(), {}), std::invalid_argument);
    auto a = weighted_automaton::of(g.build(), {std::numeric_limits<double>::max()});
    REQUIRE_THROWS_AS(a({1, 1}), std::overflow_error);
    REQUIRE_FALSE(std::signbit(weighted_automaton::of(g.build(), {-0.0}).weight(0)));
}

TEST_CASE("edges keep their numbers and weights differing by one ulp stay distinct", "[weighted]") {
    weighted_automaton::builder b;
    for (int i = 0; i < 4; ++i)
        b.add_state();
    b.set_initial(0);
    b.set_accept(3);
    auto first = b.add_transition(2, 3, 5, 7);
    auto second = b.add_transition(0, 2, 4, 1);
    auto third = b.add_transition(0, 2, 4, std::nextafter(1.0, 2.0));
    auto a = b.build();
    REQUIRE(first == 0);
    REQUIRE(second == 1);
    REQUIRE(third == 2);
    REQUIRE(std::vector<edge_id>(a.graph().outgoing(0).begin(), a.graph().outgoing(0).end()) == std::vector<edge_id>{1, 2});
    REQUIRE(std::vector<edge_id>(a.graph().outgoing(2).begin(), a.graph().outgoing(2).end()) == std::vector<edge_id>{0});
    REQUIRE(a.weight(second) < a.weight(third));
    REQUIRE(a.simplify().graph().edge_count() == 3);
    REQUIRE(a({4, 5}) == 8);
}

// Transformations

namespace {

double enumerate_word(const weighted_automaton& a, state_id s, const word& w, std::size_t at, double cost) {
    double result = at == w.size() && a.graph().is_accept(s) ? cost + a.final_weight(s) : inf;
    for (edge_id e : a.graph().outgoing(s)) {
        bool eps = a.graph().is_epsilon(e);
        if (eps || (at < w.size() && a.graph().accepts(e, w[at])))
            result = std::min(result, enumerate_word(a, a.graph().target(e), w, at + (eps ? 0 : 1), cost + a.weight(e)));
    }
    return result;
}

}  // namespace

TEST_CASE("determinization, minimization and epsilon removal of DAGs agree with enumeration", "[weighted]") {
    random_source r(7711);
    auto words = all_words({0, 1, 2}, 3);
    for (int round = 0; round < 100; ++round) {
        weighted_automaton::builder b;
        int n = 2 + r.next(6);
        for (int s = 0; s < n; ++s) {
            b.add_state();
            if (r.coin())
                b.set_accept(s, r.next(9) - 4);
        }
        b.set_initial(0, r.next(7) - 3);
        for (int i = 0; i < 15; ++i) {
            int s = r.next(n - 1), t = s + 1 + r.next(n - s - 1);
            double w = r.next(11) - 5;
            if (r.next(4) == 0) {
                b.add_epsilon(s, t, w);
            } else {
                int lo = r.next(3);
                b.add_transition(s, t, interval_set::range(lo, lo + r.next(3 - lo)), w);
            }
        }
        auto a = b.build(), no_epsilon = a.remove_epsilon(), dfa = a.determinize(), minimal = dfa.minimize();
        REQUIRE(dfa.graph().is_deterministic());
        REQUIRE(minimal.graph().is_deterministic());
        REQUIRE(minimal.graph().state_count() <= dfa.graph().state_count());
        for (std::size_t e = 0; e < no_epsilon.graph().edge_count(); ++e)
            REQUIRE_FALSE(no_epsilon.graph().is_epsilon(static_cast<edge_id>(e)));
        for (const auto& w : words) {
            double expected = enumerate_word(a, 0, w, 0, a.initial_weight());
            REQUIRE(no_epsilon(w) == expected);
            REQUIRE(dfa(w) == expected);
            REQUIRE(minimal(w) == expected);
        }
    }
}

TEST_CASE("weighted minimization pushes different offsets without losing costs", "[weighted]") {
    weighted_automaton::builder b;
    for (int i = 0; i < 5; ++i)
        b.add_state();
    b.set_initial(0);
    b.set_accept(3, 4);
    b.set_accept(4, 1);
    b.add_transition(0, 1, 0, 2);
    b.add_transition(0, 2, 1, 3);
    b.add_transition(1, 3, 2, 1);
    b.add_transition(2, 4, 2, 4);
    auto a = b.build(), minimal = a.minimize();
    REQUIRE(minimal.graph().state_count() == 3);
    REQUIRE(minimal({0, 2}) == 7);
    REQUIRE(minimal({1, 2}) == 8);
    REQUIRE(a.graph().state_count() == 5);
    REQUIRE(minimal.minimize().graph().state_count() == minimal.graph().state_count());
}

namespace {

struct cost_path {
    double cost;
    std::string edges;
};

void paths(const weighted_automaton& a, state_id s, int remaining, double cost, std::vector<edge_id>& prefix, std::vector<cost_path>& result) {
    if (remaining <= 0 && a.graph().is_accept(s)) {
        std::string text;
        for (edge_id e : prefix)
            text += std::to_string(e) + ",";
        result.push_back({cost + a.final_weight(s), text});
    }
    if (remaining == 0)
        return;
    for (edge_id e : a.graph().outgoing(s)) {
        prefix.push_back(e);
        paths(a, a.graph().target(e), remaining < 0 ? -1 : remaining - 1, cost + a.weight(e), prefix, result);
        prefix.pop_back();
    }
}

}  // namespace

TEST_CASE("k best paths of DAGs and bounded walks agree with exhaustive lists", "[weighted]") {
    random_source r(7712);
    for (int round = 0; round < 80; ++round) {
        weighted_automaton::builder b;
        for (int i = 0; i < 6; ++i)
            b.add_state();
        b.set_initial(0, 2);
        b.set_accept(5, 3);
        for (int i = 0; i < 15; ++i) {
            int s = r.next(5), t = s + 1 + r.next(5 - s);
            b.add_transition(s, t, 1, r.next(9) - 4);
        }
        auto a = b.build();
        std::vector<cost_path> all;
        std::vector<edge_id> prefix;
        paths(a, 0, -1, 2, prefix, all);
        std::ranges::stable_sort(all, {}, &cost_path::cost);
        auto actual = a.k_best_paths(12);
        REQUIRE(actual.size() == std::min<std::size_t>(12, all.size()));
        std::set<std::vector<edge_id>> distinct;
        for (std::size_t i = 0; i < actual.size(); ++i) {
            REQUIRE(actual[i].cost() == all[i].cost);
            distinct.insert({actual[i].edges().begin(), actual[i].edges().end()});
        }
        REQUIRE(distinct.size() == actual.size());
    }
    weighted_automaton::builder b;
    b.add_state();
    b.add_state();
    b.set_initial(0);
    b.set_accept(1);
    b.add_transition(0, 0, 1, 2);
    b.add_transition(0, 1, 1, 1);
    b.add_transition(1, 0, 1, -1);
    b.add_transition(1, 1, 1, 3);
    auto cyclic = b.build();
    std::vector<cost_path> all;
    std::vector<edge_id> prefix;
    paths(cyclic, 0, 4, 0, prefix, all);
    std::ranges::stable_sort(all, {}, &cost_path::cost);
    auto k = k_best_paths_of_length(cyclic, 4, 5);
    REQUIRE(k.size() == 5);
    for (std::size_t i = 0; i < k.size(); ++i) {
        REQUIRE(k[i].cost() == all[i].cost);
        REQUIRE(k[i].length() == 4);
    }
    REQUIRE(cyclic.k_best_paths(2).size() == 2);
}

TEST_CASE("compiled weighted automata reuse their index", "[weighted]") {
    weighted_automaton::builder b;
    b.add_state();
    b.add_state();
    b.set_initial(0, 1);
    b.set_accept(1, -1);
    b.add_transition(0, 1, interval_set::range(min_symbol, -1) | interval_set::range(5, max_symbol), 2.5);
    b.add_transition(0, 1, interval_set::range(0, 4), 4);
    auto a = b.build();
    auto compiled = a.compile();
    for (symbol c : {min_symbol, -1, 0, 4, 5, max_symbol})
        REQUIRE(a({c}) == compiled({c}));
    word text{99, -1, 99};
    REQUIRE(compiled.cost(std::span(text).subspan(1, 1)) == 2.5);
    REQUIRE(compiled({}) == inf);
    REQUIRE(compiled.reweight(a.with_weights({7, 9}))({-1}) == 7);
    REQUIRE_THROWS_AS(compiled.reweight(weighted_automaton()), std::invalid_argument);
}

TEST_CASE("unsupported families and budgets fail explicitly", "[weighted]") {
    weighted_automaton::builder b;
    b.add_state();
    b.add_state();
    b.set_initial(0);
    b.set_accept(1);
    b.add_transition(0, 1, 1, 0.5);
    b.add_transition(0, 1, 1, 1);
    auto decimal = b.build();
    REQUIRE(decimal.determinize()({1}) == 0.5);
    REQUIRE_THROWS_AS(decimal.compile(), std::invalid_argument);
    auto integral = decimal.with_weights({1, 2});
    REQUIRE_THROWS_AS(determinize_dag(integral, 1), std::length_error);
    weighted_automaton::builder c;
    c.add_state();
    c.add_state();
    c.set_initial(0);
    c.set_accept(1);
    c.add_epsilon(0, 0, -1);
    c.add_transition(0, 1, 2, 0);
    auto cycle = c.build();
    REQUIRE_THROWS_AS(cycle.remove_epsilon(), std::invalid_argument);
    REQUIRE_THROWS_AS(cycle.determinize(), std::invalid_argument);
}

TEST_CASE("k best paths sum exactly when doubles would cancel", "[weighted]") {
    weighted_automaton::builder b;
    for (int i = 0; i < 4; ++i)
        b.add_state();
    b.set_initial(0);
    b.set_accept(3);
    b.add_transition(0, 1, 1, 1e16);
    b.add_transition(1, 2, 1, 1);
    b.add_transition(2, 3, 1, -1e16);
    b.add_transition(0, 3, 2, 0.5);
    auto best = b.build().k_best_paths(2);
    REQUIRE(best[0].cost() == 0.5);
    REQUIRE(best[1].cost() == 1);
    REQUIRE(best[0].symbols() == word{2});
}

// Unfoldings

namespace {

double bounded(const weighted_automaton& a, state_id s, int left, double prefix) {
    double best = left == 0 && a.graph().is_accept(s) ? prefix + a.final_weight(s) : inf;
    if (left == 0)
        return best;
    for (edge_id e : a.graph().outgoing(s))
        best = std::min(best, bounded(a, a.graph().target(e), left - 1, prefix + a.weight(e)));
    return best;
}

}  // namespace

TEST_CASE("fixed lengths agree with an enumeration of the walks", "[weighted][layered]") {
    random_source r(804);
    for (int round = 0; round < 100; ++round) {
        weighted_automaton::builder b;
        int n = 2 + r.next(5);
        for (int s = 0; s < n; ++s) {
            b.add_state();
            if (r.coin())
                b.set_accept(s, r.next(5) - 2);
        }
        b.set_initial(0, 2);
        for (int k = 0; k < 2 * n; ++k)
            b.add_transition(r.next(n), r.next(n), interval_set::range(-2, 3), r.next(9) - 4);
        auto a = b.build();
        double at_most = inf;
        for (int length = 0; length < 5; ++length) {
            double expected = bounded(a, 0, length, 2);
            at_most = std::min(at_most, expected);
            REQUIRE(a.cost_of_length(length) == expected);
            REQUIRE(a.cost_of_length_at_most(length) == at_most);
            auto layers = layered_automaton::exactly(a.graph(), length);
            REQUIRE(layers.evaluate(a).optimal_cost() == expected);
            auto p = layers.best_path(a);
            if (expected == inf) {
                REQUIRE_FALSE(p);
            } else {
                REQUIRE(p->length() == static_cast<std::size_t>(length));
                REQUIRE(p->cost() == expected);
                REQUIRE(a(p->symbols()) == expected);
            }
            REQUIRE(layered_automaton::at_most(a.graph(), length).evaluate(a).optimal_cost() == at_most);
        }
    }
}

TEST_CASE("unfoldings are reused by new weights and positional costs", "[weighted][layered]") {
    weighted_automaton::builder b;
    for (int i = 0; i < 3; ++i)
        b.add_state();
    b.set_initial(0, 1);
    b.set_accept(1, 2);
    b.add_transition(0, 1, 3, 4);
    b.add_transition(1, 1, 5, 7);
    // State 2 is useless and never unfolded
    auto a = b.build();
    auto layers = layered_automaton::exactly(a.graph(), 3);
    REQUIRE(layers.graph().state_count() == 4);
    REQUIRE(layers.graph().edge_count() == 3);
    auto changed = a.with_weights({-1, 2});
    REQUIRE(layers.evaluate(a).graph().same_as(layers.graph()));
    REQUIRE(layers.evaluate(changed).graph().same_as(layers.graph()));
    REQUIRE(layers.analyze(a).optimal_cost() == 21);
    REQUIRE(layers.analyze(changed).optimal_cost() == 6);
    auto analysis = layers.analyze(a, [](int position, edge_id e) { return static_cast<double>(position + e); });
    REQUIRE(analysis.optimal_cost() == 8);
    REQUIRE(analysis.best_path()->cost() == 8);
    REQUIRE(analysis.cost_at(0, 3) == 8);
    REQUIRE(analysis.cost_at(0, 5) == inf);
    REQUIRE(analysis.cost_at(2, 5) == 8);
    REQUIRE(analysis.forward_distances().size() == 4);
    REQUIRE(analysis.backward_distances().size() == 4);
    REQUIRE(analysis.best_path()->graph().same_as(a.graph()));
    REQUIRE_THROWS_AS(analysis.cost_at(3, 5), std::out_of_range);
    REQUIRE_THROWS_AS(layers.evaluate(weighted_automaton()), std::invalid_argument);
}

TEST_CASE("epsilon edges read no position and may stay cyclic", "[weighted][layered]") {
    weighted_automaton::builder b;
    for (int i = 0; i < 3; ++i)
        b.add_state();
    b.set_initial(0);
    b.set_accept(2);
    b.add_epsilon(0, 1, -2);
    b.add_transition(1, 2, 9, 5);
    b.add_epsilon(1, 1, 0);
    auto a = b.build();
    auto layers = layered_automaton::exactly(a.graph(), 1);
    REQUIRE_FALSE(layers.graph().is_acyclic());
    REQUIRE(a.cost_of_length(1) == 3);
    REQUIRE(layers.best_path(a)->length() == 1);
    REQUIRE(layers.analyze(a).cost_at(0, 9) == 3);
    REQUIRE(a.cost_of_length(0) == inf);
    auto negative = a.with_weights({-2, 5, -1});
    REQUIRE(negative.cost_of_length(1) == -inf);
    REQUIRE(layers.analyze(negative).optimal_cost() == -inf);
    REQUIRE_THROWS_AS(layers.best_path(negative), std::logic_error);
    REQUIRE_THROWS_AS(layered_automaton::exactly(a.graph(), -1), std::invalid_argument);
    REQUIRE_THROWS_AS(layered_automaton::exactly(a.graph(), std::numeric_limits<int>::max()), std::length_error);
}
