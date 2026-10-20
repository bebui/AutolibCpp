// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <autolib/autolib.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace autolib;
using namespace autolib::literals;

TEST_CASE("regular expression syntax", "[regex]") {
    REQUIRE("12"_re({1, 2}));
    REQUIRE("1|2"_re({2}));
    REQUIRE("<42><-7>"_re({42, -7}));
    REQUIRE(".5"_re({-123456, 5}));
    REQUIRE("(12)*"_re({1, 2, 1, 2}));
    REQUIRE_FALSE("(12)+"_re({}));
    REQUIRE("1?2"_re({2}));
    REQUIRE("1{3}"_re({1, 1, 1}));
    REQUIRE_FALSE("1{3}"_re({1, 1}));
    REQUIRE("1{2,}"_re({1, 1, 1, 1}));
    REQUIRE("1{,2}"_re({}));
    REQUIRE_FALSE("1{,2}"_re({1, 1, 1}));
    REQUIRE("1{1,2}"_re == "1|11"_re);
    REQUIRE("()"_re({}));
    REQUIRE("1|"_re({}));
    REQUIRE("(0|1)*&(.1)*"_re == "((0|1)1)*"_re);
    REQUIRE_FALSE("~(0*)"_re({0, 0}));
    REQUIRE("~(0*)"_re({0, 1}));
    REQUIRE("1*+?"_re == "1*"_re);
    REQUIRE("1{2}{3}"_re == "1{6}"_re);
}

TEST_CASE("precedence", "[regex]") {
    REQUIRE("12|3"_re == "(12)|3"_re);
    REQUIRE("1|2&2"_re == "1|(2&2)"_re);
    REQUIRE("12*"_re == "1(2*)"_re);
    REQUIRE("~12"_re == "(~1)2"_re);
    REQUIRE("~1*"_re == "~(1*)"_re);
}

TEST_CASE("malformed expressions report their position", "[regex]") {
    auto position = [](std::string_view e) {
        try {
            (void)regex(e);
        } catch (const regex_error& error) {
            return static_cast<long>(error.position());
        }
        return -1L;
    };
    REQUIRE(position("(12") == 3);
    REQUIRE(position("12)") == 2);
    REQUIRE(position("a") == 0);
    REQUIRE(position("1{2") == 1);
    REQUIRE(position("1{3,2}") == 1);
    REQUIRE(position("1{x}") == 1);
    REQUIRE(position("1{,}") == 1);
    REQUIRE(position("<12") == 0);
    REQUIRE(position("<1x>") == 0);
    REQUIRE(position("<99999999999>") == 0);
    REQUIRE(position("*") == 0);
    REQUIRE(position("~") == 1);
    REQUIRE(position("(0|1)*0") == -1);
    REQUIRE_THROWS_AS(automaton::from_regex("(("), regex_error);
    REQUIRE_THROWS_AS(automaton::from_regex("(("), std::invalid_argument);
}

TEST_CASE("expressions print back", "[regex]") {
    REQUIRE(regex("(0|1)*0<12>.").to_string() == "((0|1))*0<12>.");
    REQUIRE(regex("1{2,}").to_string() == "(1){2,}");
    REQUIRE(regex("~1&2").to_string() == "(~(1)&2)");
    for (const char* e : {"(0|1)*0<12>.", "1{2,5}2?3+", "~(1|2)&(3|())*", "<-5>{,3}"})
        REQUIRE(automaton::from_regex(regex(e).to_string()) == automaton::from_regex(e));
}

TEST_CASE("Thompson construction is linear", "[regex]") {
    std::string big;
    for (int i = 0; i < 3000; ++i) {
        if (i > 0)
            big += '|';
        big += "<" + std::to_string(i) + "><" + std::to_string(i + 1) + ">";
    }
    auto nfa = automaton::nfa_from_regex(big);
    REQUIRE(nfa.state_count() < 4 * 3000 + 10);
    REQUIRE(nfa({1500, 1501}));
    REQUIRE_FALSE(nfa({1500, 1500}));
    auto dfa = automaton::from_regex(big);
    REQUIRE(dfa({2999, 3000}));
    REQUIRE(dfa.state_count() == 3002);
}
