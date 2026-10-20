// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <autolib/autolib.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace autolib;
using namespace autolib::literals;

TEST_CASE("an automaton is built state by state", "[automaton]") {
    automaton a;
    REQUIRE(a.state_count() == 0);
    REQUIRE(a.initial() == no_state);
    REQUIRE_FALSE(a({}));
    state_id s0 = a.add_state();
    state_id s1 = a.add_state(true);
    a.set_initial(s0);
    a.add_transition(s0, s1, interval_set::range(1, 3));
    a.add_transition(s1, s1, 0);
    REQUIRE(a.is_deterministic());
    REQUIRE(a({2}));
    REQUIRE(a({3, 0, 0}));
    REQUIRE_FALSE(a({0}));
    REQUIRE_FALSE(a({}));
    a.add_transition(s0, s0, 2);
    REQUIRE_FALSE(a.is_deterministic());
    REQUIRE(a({2, 2, 1}));
    REQUIRE_THROWS_AS(a.is_accept(42), std::out_of_range);
}

TEST_CASE("labels towards the same target are merged", "[automaton]") {
    automaton a;
    auto s = a.add_state(), t = a.add_state(true);
    a.set_initial(s);
    a.add_transition(s, t, 1);
    a.add_transition(s, t, 2);
    REQUIRE(a.transitions(s).size() == 1);
    REQUIRE(a.transitions(s)[0].labels == interval_set::range(1, 2));
}

TEST_CASE("epsilon transitions are simulated", "[automaton]") {
    automaton a;
    auto s0 = a.add_state(), s1 = a.add_state(), s2 = a.add_state(true);
    a.set_initial(s0);
    a.add_epsilon(s0, s1);
    a.add_transition(s1, s2, 5);
    a.add_epsilon(s2, s0);
    REQUIRE_FALSE(a.is_deterministic());
    REQUIRE(a({5, 5, 5}));
    REQUIRE_FALSE(a({}));
    REQUIRE(a.determinize().is_deterministic());
    REQUIRE(a.determinize() == a);
    REQUIRE(a.remove_epsilon() == a);
    REQUIRE(a.minimize().state_count() == 2);
}

TEST_CASE("operators follow the operations of languages", "[automaton][operators]") {
    auto a = "(0|1)*0"_re;
    auto b = "1(0|1)*"_re;
    REQUIRE((a | b)({1, 1}));
    REQUIRE((a | b)({0}));
    REQUIRE_FALSE((a | b)({0, 1}));
    REQUIRE((a & b)({1, 0}));
    REQUIRE_FALSE((a & b)({0}));
    REQUIRE((a - b)({0, 0}));
    REQUIRE_FALSE((a - b)({1, 0}));
    REQUIRE((~a)({0, 1}));
    REQUIRE((~a)({}));
    REQUIRE((~a)({7}));
    REQUIRE((a + b)({0, 1}));
    REQUIRE_FALSE((a + b)({1, 0}));
    REQUIRE(a.star()({}));
    REQUIRE(a.star()({1, 0, 0}));
    REQUIRE_FALSE(a.plus()({}));
    REQUIRE(a.optional()({}));
    REQUIRE((a - b) == (a & ~b));
    REQUIRE((a & b) <= a);
    REQUIRE((a & b) < a);
    REQUIRE(a <= (a | b));
    REQUIRE_FALSE(a <= b);
    REQUIRE((a <=> b) == std::partial_ordering::unordered);
    REQUIRE((a <=> (a | b)) == std::partial_ordering::less);
    REQUIRE((a <=> "(0|1)*0"_nfa) == std::partial_ordering::equivalent);
    REQUIRE(~~a == a);
    REQUIRE((a | ~a).is_universal());
    REQUIRE((a & ~a).is_empty());

    auto c = a;
    c |= b;
    REQUIRE(c == (a | b));
    c &= b;
    REQUIRE(c == b);
    c -= "1"_re;
    REQUIRE_FALSE(c({1}));
    c += "2"_re;
    REQUIRE(c({1, 0, 2}));
}

TEST_CASE("elementary languages", "[automaton]") {
    REQUIRE(automaton::empty_language().is_empty());
    REQUIRE(automaton::empty_word()({}));
    REQUIRE_FALSE(automaton::empty_word()({0}));
    REQUIRE(automaton::any_symbol()({-100}));
    REQUIRE(automaton::universal().is_universal());
    REQUIRE(automaton::of_word({1, 2, 3})({1, 2, 3}));
    REQUIRE(automaton::of_word({1, 2, 3}) == "123"_re);
    REQUIRE(automaton::symbols(interval_set::range(0, 9)) == "0|1|2|3|4|5|6|7|8|9"_re);
}

TEST_CASE("repetitions", "[automaton]") {
    auto a = "01"_re;
    REQUIRE(a.repeat(2, 3) == "(01){2,3}"_re);
    REQUIRE(a.repeat(0, -1) == a.star());
    REQUIRE(a.repeat(1, -1) == a.plus());
    REQUIRE(a.repeat(0, 1) == a.optional());
    REQUIRE(a.repeat(2, 2) == a + a);
    REQUIRE_THROWS_AS(a.repeat(3, 2), std::invalid_argument);
}

TEST_CASE("minimization algorithms agree", "[automaton][minimize]") {
    auto a = "(0|1)*0(0|1){5}"_nfa;
    auto m = a.minimize();
    auto b = a.minimize(minimization::brzozowski);
    REQUIRE(m.state_count() == 64);
    REQUIRE(b.state_count() == 64);
    REQUIRE(m == b);
    REQUIRE(m.is_deterministic());
    REQUIRE("()"_re.minimize().state_count() == 1);
    REQUIRE(automaton{}.minimize().state_count() == 0);
}

TEST_CASE("trim removes useless states", "[automaton]") {
    automaton a;
    auto s0 = a.add_state(), s1 = a.add_state(true), dead = a.add_state(), unreachable = a.add_state(true);
    a.set_initial(s0);
    a.add_transition(s0, s1, 1);
    a.add_transition(s0, dead, 2);
    a.add_transition(unreachable, s1, 3);
    a.trim();
    REQUIRE(a.state_count() == 2);
    REQUIRE(a({1}));
}

TEST_CASE("printing", "[automaton]") {
    automaton a = "1|2"_re;
    REQUIRE(a.to_string() == "0 -{1..2}-> 1\n");
    REQUIRE(a.to_dot().find("doublecircle") != std::string::npos);
}
