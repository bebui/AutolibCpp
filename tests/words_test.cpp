// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <autolib/autolib.hpp>
#include <catch2/catch_test_macros.hpp>
#include <random>
#include <ranges>

using namespace autolib;
using namespace autolib::literals;

namespace {
std::vector<word> collect(const automaton& a, int max_length) {
    std::vector<word> out;
    for (const word& w : words(a, max_length))
        out.push_back(w);
    return out;
}
}  // namespace

static_assert(std::input_iterator<word_range::iterator>);
static_assert(std::ranges::input_range<word_range>);

TEST_CASE("words are enumerated in shortlex order", "[words]") {
    REQUIRE(collect("(1|2)3?"_re, 2) == std::vector<word>{{1}, {2}, {1, 3}, {2, 3}});
    REQUIRE(collect("(1|2)3?"_re, 1) == std::vector<word>{{1}, {2}});
    REQUIRE(collect("(1|2)3?"_re, 0).empty());
    REQUIRE(collect("()|5"_re, 3) == std::vector<word>{{}, {5}});
    REQUIRE(collect("0*"_re, 3) == std::vector<word>{{}, {0}, {0, 0}, {0, 0, 0}});
    REQUIRE(collect(automaton{}, 4).empty());
    REQUIRE(collect("0&1"_re, 4).empty());
    REQUIRE(collect("<-3>|<70>"_nfa, 1) == std::vector<word>{{-3}, {70}});
    auto any = words(automaton::any_symbol(), 1).begin();
    REQUIRE(*any == word{min_symbol});
    ++any;
    REQUIRE(*any == word{min_symbol + 1});
    auto last = collect(automaton::from_regex("<2147483646>|<2147483647>"), 1);
    REQUIRE(last == std::vector<word>{{max_symbol - 1}, {max_symbol}});
    REQUIRE_THROWS_AS(words("0"_re, -1), std::invalid_argument);
}

TEST_CASE("enumeration agrees with membership", "[words][random]") {
    std::mt19937 rng(5);
    for (const char* e : {"(0|1)*0(0|1)", "(01|10)*", "~((0|1)*11(0|1)*)&(0|1)*", "0(1|2)*|2{2,3}", "(0|12)*2?"}) {
        auto a = automaton::from_regex(e);
        auto listed = collect(a, 6);
        std::vector<word> expected;
        // Every word over {0, 1, 2} up to length 6, in shortlex order
        std::vector<word> layer{{}};
        for (int l = 0; l <= 6; ++l) {
            for (const auto& w : layer)
                if (a(w))
                    expected.push_back(w);
            std::vector<word> next;
            for (const auto& w : layer)
                for (symbol c = 0; c < 3; ++c) {
                    auto x = w;
                    x.push_back(c);
                    next.push_back(x);
                }
            layer = std::move(next);
        }
        INFO(e);
        REQUIRE(listed == expected);
    }
}
