// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Weighted automata : costs of words, best paths, enumeration by cost, and several costs at once.

#include <autolib/autolib.hpp>
#include <iostream>

int main() {
    using namespace autolib;

    weighted_automaton::builder b;
    auto s = b.add_state(), t = b.add_state();
    b.set_initial(s);
    b.set_accept(t, 0.5);                      // final cost
    b.add_transition(s, t, 1, 2.0);            // reads 1 for 2.0
    b.add_transition(s, t, interval_set::range(0, 9), 3.0);
    b.add_transition(t, t, 0, 0.25);
    auto a = b.build();

    std::cout << "cost of 1 0 : " << a({1, 0}) << '\n';  // 2.75
    std::cout << "best path : " << a.best_path()->cost() << '\n';
    for (const auto& w : a.k_best_words(3)) {
        std::cout << "word";
        for (symbol c : w.symbols)
            std::cout << ' ' << c;
        std::cout << " costs " << w.cost << '\n';
    }
    for (const auto& path : a.paths_by_cost()) {
        if (path.cost() > 3)
            break;
        std::cout << "path of " << path.edge_count() << " edges costs " << path.cost() << '\n';
    }
    auto d = a.determinize().minimize();
    std::cout << "minimal deterministic : " << d.graph().state_count() << " states, cost of 1 0 0 : "
              << d.compile()({1, 0, 0}) << '\n';

    // An automaton weighted directly, and several costs sharing one snapshot
    using namespace autolib::literals;
    auto source = "12+"_re;
    std::cout << "12+ at 2 per symbol, 1 2 2 : " << source.weighted([](const automaton_edge&) { return 2.0; })({1, 2, 2}) << '\n';
    auto snapshot = source.snapshot();
    auto price = snapshot.weighted([](const automaton_edge& e) { return e.epsilon ? 0.0 : 2.0; });
    auto both = snapshot.multi_weighted({[](const automaton_edge&) { return 2.0; }, [](const automaton_edge&) { return 5.0; }});
    std::cout << "shared graph : " << price.graph().same_as(both.graph()) << ", 1 2 accepted : " << price.run({1, 2})
              << ", same language : " << (price.to_automaton() == source) << '\n';

    multi_weighted_automaton::builder m(2);    // time and money
    auto p = m.add_state(), q = m.add_state();
    m.set_initial(p);
    m.set_accept(q);
    m.add_transition(p, q, 7, {10, 1});        // slow and cheap
    m.add_transition(p, q, 7, {2, 5});         // fast and expensive
    auto v = m.build();
    for (const auto& path : v.pareto_front().paths())
        std::cout << "Pareto : time " << path.cost(0) << ", money " << path.cost(1) << '\n';
    auto quick = v.lexicographic_best_path({0, 1});
    std::cout << "time first : " << quick->cost(0) << ' ' << quick->cost(1) << '\n';
    std::cout << "time + 2 money : " << v.scalarize({1, 2}).optimal_cost() << '\n';
}
