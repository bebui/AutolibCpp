// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <autolib/autolib.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace autolib;
using namespace autolib::literals;

TEST_CASE("equality and inclusion of languages", "[language]") {
    REQUIRE("(0|1)*"_re == "(0*1*)*"_nfa);
    REQUIRE("(0|1)*0(0|1)"_nfa == "(0|1)*0(0|1)"_re);
    REQUIRE_FALSE("(0|1)*"_re == "(0|1)+"_re);
    REQUIRE("(0|1)+"_re <= "(0|1)*"_re);
    REQUIRE_FALSE("(0|1)*"_re <= "(0|1)+"_re);
    REQUIRE(automaton{} <= "0"_re);
    REQUIRE(automaton{} == automaton::empty_language());
    REQUIRE(automaton{} == "0&1"_re);
    REQUIRE_FALSE("0"_re <= automaton{});
    REQUIRE(*"(0|1)*"_re.counter_example("(0|1)+"_re) == word{});
    REQUIRE(*"0*"_re.counter_example("0{,3}"_re) == word{0, 0, 0, 0});
    REQUIRE_FALSE("01"_re.counter_example("01"_nfa));
}

TEST_CASE("shortest words", "[language]") {
    REQUIRE(*"5|34|<-1><-2><-3>"_re.shortest_word() == word{5});
    REQUIRE(*"(1|2)(3|4)"_nfa.shortest_word() == word{1, 3});
    REQUIRE(*"0*"_re.shortest_word() == word{});
    REQUIRE_FALSE("0&1"_re.shortest_word());
    REQUIRE_FALSE(automaton{}.shortest_word());
}

TEST_CASE("finiteness", "[language]") {
    REQUIRE("0|11|222"_re.is_finite());
    REQUIRE("(0|1){3,5}"_nfa.is_finite());
    REQUIRE_FALSE("0*"_re.is_finite());
    REQUIRE("(0*)&(1*)"_re.is_finite());
    REQUIRE(automaton{}.is_finite());
    REQUIRE("0*1&0"_re.is_finite());
}

TEST_CASE("universality", "[language]") {
    REQUIRE(".*"_re.is_universal());
    REQUIRE((".*0"_re | "~(.*0)"_re).is_universal());
    REQUIRE_FALSE("(0|1)*"_re.is_universal());
    REQUIRE((~automaton{}).is_universal());
}
