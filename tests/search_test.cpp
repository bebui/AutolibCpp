// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <autolib/autolib.hpp>
#include <catch2/catch_test_macros.hpp>
#include <random>

using namespace autolib;
using namespace autolib::literals;

TEST_CASE("compiled automata", "[compiled]") {
    compiled_dfa dfa("(1|2)*12"_re);
    REQUIRE(dfa.has_lookup_table());
    REQUIRE(dfa({2, 1, 2}));
    REQUIRE_FALSE(dfa({2, 1}));
    REQUIRE_FALSE(dfa({3}));
    REQUIRE_FALSE(dfa({-5}));
    compiled_dfa empty;
    REQUIRE_FALSE(empty({}));
    compiled_dfa wide("<-2000000000>.<2000000000>"_re);
    REQUIRE(wide({-2000000000, 0, 2000000000}));
    REQUIRE(wide({-2000000000, max_symbol, 2000000000}));
    REQUIRE_FALSE(wide({-2000000000, 0, 1999999999}));
    compiled_dfa nfa("(0|1)*0(0|1)"_nfa);
    REQUIRE(nfa({1, 0, 1}));
}

TEST_CASE("compiled automata agree with automata on random words", "[compiled][random]") {
    std::mt19937 rng(3);
    for (const char* e : {"(0|1)*0(0|1){4}", "(<100>|<-100>|.)*<7>", "~(.*12.*)", "(<0>|<70000>|<5>)*<70000>"}) {
        auto a = automaton::from_regex(e);
        compiled_dfa table(a), plain(a, false);
        for (int i = 0; i < 2000; ++i) {
            word w(rng() % 12);
            for (auto& s : w) {
                switch (rng() % 4) {
                case 0: s = static_cast<symbol>(rng() % 3); break;
                case 1: s = 7; break;
                case 2: s = 70000; break;
                default: s = static_cast<symbol>(rng());
                }
            }
            REQUIRE(table(w) == a(w));
            REQUIRE(plain(w) == a(w));
        }
    }
}

TEST_CASE("leftmost longest search", "[search]") {
    searcher s("12+"_re);
    REQUIRE(*s.find({3, 1, 2, 2, 4}) == match{1, 4});
    REQUIRE_FALSE(s.find({3, 1, 3, 2}));
    REQUIRE(s.contains({1, 2}));
    REQUIRE_FALSE(s.contains({2, 1}));
    REQUIRE(s.find_all({1, 2, 1, 2, 2, 0, 1}) == std::vector<match>{{0, 2}, {2, 5}});
    word text{1, 2, 0, 1, 2};
    REQUIRE(*s.find(text, 1) == match{3, 5});

    searcher empty_word("0*"_re);
    REQUIRE(empty_word.find_all({1, 0, 0, 1}) == std::vector<match>{{0, 0}, {1, 3}, {3, 3}, {4, 4}});
}

TEST_CASE("search agrees with a naive scan", "[search][random]") {
    std::mt19937 rng(11);
    for (const char* e : {"01*2", "(0|1)*2", "0|00|000", "1(0|2)*1", "()|2"}) {
        auto a = automaton::from_regex(e);
        searcher s(a);
        for (int round = 0; round < 300; ++round) {
            word text(rng() % 20);
            for (auto& c : text)
                c = static_cast<symbol>(rng() % 3);
            std::vector<match> expected;
            for (std::size_t i = 0; i <= text.size();) {
                std::optional<std::size_t> end;
                for (std::size_t j = i; j <= text.size(); ++j)
                    if (a(std::span(text).subspan(i, j - i)))
                        end = j;
                if (!end) {
                    ++i;
                    continue;
                }
                expected.push_back({i, *end});
                i = *end > i ? *end : i + 1;
            }
            REQUIRE(s.find_all(text) == expected);
            REQUIRE(s.contains(text) == !expected.empty());
            if (!expected.empty())
                REQUIRE(*s.find(text) == expected.front());
        }
    }
}
