// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <autolib/autolib.hpp>
#include <iostream>

int main() {
    using namespace autolib;
    using namespace autolib::literals;

    // Words over {0, 1} whose third symbol from the end is 0
    automaton a = "(0|1)*0(0|1){2}"_re;
    // Words containing 11
    automaton b = "(0|1)*11(0|1)*"_re;

    automaton both = a & b;
    std::cout << "a & b has " << both.state_count() << " states\n";
    std::cout << "0110 in a & b : " << both({0, 1, 1, 0}) << '\n';
    std::cout << "a - b is included in a : " << ((a - b) <= a) << '\n';
    std::cout << "a | b == b | a : " << ((a | b) == (b | a)) << '\n';

    if (auto w = (a - b).shortest_word()) {
        std::cout << "shortest word of a - b :";
        for (symbol s : *w)
            std::cout << ' ' << s;
        std::cout << '\n';
    }

    // Fast membership on a compiled automaton, and search in a sequence
    compiled_dfa dfa(both);
    std::cout << "compiled : " << dfa({1, 1, 0, 1, 1}) << '\n';
    searcher s("12+"_re);
    for (match m : s.find_all({3, 1, 2, 2, 4, 1, 2}))
        std::cout << "match [" << m.start << ", " << m.end << ")\n";
}
