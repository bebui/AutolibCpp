// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <autolib/autolib.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <map>
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

TEST_CASE("words are counted", "[words]") {
    REQUIRE(count_words("(0|1)*0(0|1){3}"_re, 4) == 8);
    REQUIRE(count_words("(0|1)*0(0|1){3}"_re, 3) == 0);
    REQUIRE(count_words(automaton::universal(), 3) == big_integer(1) << 96);
    REQUIRE(count_words(automaton{}, 2) == 0);
    REQUIRE(count_words("()"_re, 0) == 1);
    REQUIRE(count_words("<0>|<-2147483648>|<2147483647>"_nfa, 1) == 3);
    // The Fibonacci numbers : words over {0, 1} without two consecutive 1
    auto fibonacci = "~(.*11.*)&(0|1)*"_re;
    big_integer a = 1, b = 2;
    for (int l = 0; l < 300; ++l) {
        REQUIRE(count_words(fibonacci, l) == a);
        a = std::exchange(b, a + b);
    }
    REQUIRE_THROWS_AS(count_words("0"_re, -1), std::invalid_argument);
}

TEST_CASE("ranks follow the lexicographic order of the words", "[words]") {
    for (const char* e : {"(0|1)*0(0|1)", "(01|10)*", "~((0|1)*11(0|1)*)&(0|1)*", "0(1|2)*|2{2,3}", "(0|12)*2?"}) {
        INFO(e);
        auto a = automaton::from_regex(e);
        for (int length = 0; length <= 5; ++length) {
            std::vector<word> expected;
            for (const word& w : words(a, length))
                if (static_cast<int>(w.size()) == length)
                    expected.push_back(w);
            word_sampler sampler(a, length);
            REQUIRE(sampler.length() == length);
            REQUIRE(sampler.empty() == expected.empty());
            REQUIRE(sampler.count() == expected.size());
            for (std::size_t r = 0; r < expected.size(); ++r) {
                REQUIRE(sampler.unrank(r) == expected[r]);
                REQUIRE(sampler.rank(expected[r]) == big_integer(r));
            }
            REQUIRE_THROWS_AS(sampler.unrank(sampler.count()), std::out_of_range);
            REQUIRE_THROWS_AS(sampler.unrank(-1), std::out_of_range);
        }
    }
    word_sampler sampler("<-3>.?|<5><7>"_re, 2);
    REQUIRE(sampler.count() == (big_integer(1) << 32) + 1);
    REQUIRE(sampler.unrank(0) == word{-3, min_symbol});
    REQUIRE(sampler.unrank(sampler.count() - 2) == word{-3, max_symbol});
    REQUIRE(sampler.unrank(sampler.count() - 1) == word{5, 7});
    REQUIRE(sampler.rank({-3, 0}) == big_integer(1) << 31);
    REQUIRE_FALSE(sampler.rank({5, 8}));
    REQUIRE_FALSE(sampler.rank({-3}));
}

TEST_CASE("ranks of huge counts round trip", "[words][random]") {
    std::mt19937_64 rng(9);
    auto a = "(.<3>|<-1>..)*&~(.*<0><0>.*)"_re;
    word_sampler sampler(a, 200);
    REQUIRE(sampler.count().bit_length() > 2000);
    for (int i = 0; i < 50; ++i) {
        word w = sampler(rng);
        REQUIRE(w.size() == 200);
        REQUIRE(a(w));
        auto r = sampler.rank(w);
        REQUIRE(r);
        REQUIRE(sampler.unrank(*r) == w);
    }
    auto r = big_integer::uniform(sampler.count(), rng);
    REQUIRE(sampler.rank(sampler.unrank(r)) == r);
}

TEST_CASE("random words are uniform", "[words][random]") {
    std::mt19937 rng(17);
    // 8 words of length 4 : 0000, then 3 symbols set among 4 positions... checked by frequencies
    auto a = "(0|1)*0(0|1){2}"_re;
    word_sampler sampler(a, 4);
    REQUIRE(sampler.count() == 8);
    std::map<word, int> seen;
    for (int i = 0; i < 16000; ++i)
        seen[sampler(rng)]++;
    REQUIRE(seen.size() == 8);
    for (const auto& [w, c] : seen) {
        REQUIRE(a(w));
        REQUIRE((c > 1700 && c < 2300));
    }
    REQUIRE(random_word(a, 4, rng).has_value());
    REQUIRE(random_word(a, 2, std::mt19937_64(1)) == std::nullopt);
    REQUIRE_THROWS_AS(word_sampler(a, 2)(rng), std::out_of_range);
    REQUIRE(random_word("()"_re, 0, rng) == word{});
}

TEST_CASE("counts of very different sizes are kept exactly", "[words]") {
    // From the states of <5>{n} one word is accepted, beside 2^(32 l) words from the others : the small counts
    // are invisible to the estimate of the size of the table
    auto a = ".{150}|<5>{150}<6>{150}"_re;
    word_sampler sampler(a, 300);
    REQUIRE(sampler.count() == 1);
    REQUIRE(sampler.unrank(0) == [] {
        word w(300, 5);
        std::fill(w.begin() + 150, w.end(), 6);
        return w;
    }());
    word_sampler wide(a, 150);
    REQUIRE(wide.count() == big_integer(1) << (32 * 150));
    REQUIRE(count_words(a, 300) == 1);
    REQUIRE(count_words(a, 150) == big_integer(1) << (32 * 150));
}
