// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Every operation is checked against a direct simulation of the operands on all the words up to a given length.

#include <algorithm>
#include <autolib/autolib.hpp>
#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <random>
#include <set>

using namespace autolib;

namespace {

constexpr int sigma = 3;
constexpr int max_length = 5;

/// Reference simulation on sets of states, independent from the library
bool accepts(const automaton& a, const word& w) {
    if (a.initial() == no_state)
        return false;
    auto close = [&](std::set<state_id> s) {
        std::vector<state_id> todo(s.begin(), s.end());
        while (!todo.empty()) {
            auto x = todo.back();
            todo.pop_back();
            for (auto t : a.epsilons(x))
                if (s.insert(t).second)
                    todo.push_back(t);
        }
        return s;
    };
    auto current = close({a.initial()});
    for (symbol c : w) {
        std::set<state_id> next;
        for (auto s : current)
            for (const auto& t : a.transitions(s))
                if (t.labels.contains(c))
                    next.insert(t.target);
        current = close(next);
    }
    return std::ranges::any_of(current, [&](state_id s) { return a.is_accept(s); });
}

std::vector<word> all_words() {
    std::vector<word> out{{}};
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (out[i].size() == max_length)
            continue;
        for (symbol c = 0; c < sigma; ++c) {
            word w = out[i];
            w.push_back(c);
            out.push_back(w);
        }
    }
    return out;
}

/// The words up to max_length accepted by the reference simulation
std::set<word> language(const automaton& a) {
    static const auto words = all_words();
    std::set<word> out;
    for (const auto& w : words)
        if (accepts(a, w))
            out.insert(w);
    return out;
}

automaton random_nfa(std::mt19937& rng, bool epsilons) {
    automaton a;
    int n = 1 + static_cast<int>(rng() % 6);
    for (int i = 0; i < n; ++i)
        a.add_state(rng() % 3 == 0);
    a.set_initial(0);
    int m = static_cast<int>(rng() % static_cast<unsigned>(3 * n + 1));
    for (int i = 0; i < m; ++i) {
        auto from = static_cast<state_id>(rng() % static_cast<unsigned>(n)), to = static_cast<state_id>(rng() % static_cast<unsigned>(n));
        if (epsilons && rng() % 5 == 0) {
            a.add_epsilon(from, to);
        } else {
            symbol lo = static_cast<symbol>(rng() % sigma);
            a.add_transition(from, to, interval_set::range(lo, lo + static_cast<symbol>(rng() % 2)));
        }
    }
    return a;
}

void check_language(const automaton& result, const std::function<bool(const word&)>& expected, const char* what) {
    static const auto words = all_words();
    for (const auto& w : words) {
        bool e = expected(w);
        if (accepts(result, w) != e || result.run(w) != e) {
            INFO(what << " differs on a word of length " << w.size());
            REQUIRE(accepts(result, w) == e);
            REQUIRE(result.run(w) == e);
        }
    }
}

}  // namespace

TEST_CASE("transformations preserve the language", "[random]") {
    std::mt19937 rng(20251020);
    for (int round = 0; round < 150; ++round) {
        automaton a = random_nfa(rng, true);
        auto la = language(a);
        auto in_a = [&](const word& w) { return la.contains(w); };
        auto d = a.determinize();
        REQUIRE(d.is_deterministic());
        check_language(d, in_a, "determinize");
        auto m = a.minimize();
        REQUIRE(m.is_deterministic());
        check_language(m, in_a, "minimize");
        auto b = a.minimize(minimization::brzozowski);
        REQUIRE(b.state_count() == m.state_count());
        REQUIRE(m.minimize().state_count() == m.state_count());
        check_language(a.remove_epsilon(), in_a, "remove_epsilon");
        auto r = a.reduce();
        REQUIRE(r.state_count() <= a.remove_epsilon().state_count());
        check_language(r, in_a, "reduce");
        check_language(a.reverse(), [&](const word& w) { return in_a(word(w.rbegin(), w.rend())); }, "reverse");
        compiled_dfa c(a), c2(a, false);
        for (const auto& w : all_words())
            if (c.run(w) != in_a(w) || c2.run(w) != in_a(w))
                FAIL("compiled automaton differs");
    }
}

TEST_CASE("boolean and regular operations", "[random]") {
    std::mt19937 rng(7);
    for (int round = 0; round < 150; ++round) {
        automaton a = random_nfa(rng, round % 2 == 0), b = random_nfa(rng, round % 3 == 0);
        if (round % 4 == 0)
            a = a.minimize();
        if (round % 5 == 0)
            b = b.determinize();
        auto la = language(a), lb = language(b);
        auto in_a = [&](const word& w) { return la.contains(w); };
        auto in_b = [&](const word& w) { return lb.contains(w); };
        check_language(a | b, [&](const word& w) { return in_a(w) || in_b(w); }, "union");
        check_language(a & b, [&](const word& w) { return in_a(w) && in_b(w); }, "intersection");
        check_language(a - b, [&](const word& w) { return in_a(w) && !in_b(w); }, "difference");
        check_language(a.symmetric_difference(b), [&](const word& w) { return in_a(w) != in_b(w); }, "symmetric");
        check_language(~a, [&](const word& w) { return !in_a(w); }, "complement");
        check_language(a + b, [&](const word& w) {
            for (std::size_t i = 0; i <= w.size(); ++i)
                if (in_a(word(w.begin(), w.begin() + static_cast<long>(i))) && in_b(word(w.begin() + static_cast<long>(i), w.end())))
                    return true;
            return false;
        }, "concatenation");
        std::function<bool(const word&)> in_star = [&](const word& w) {
            if (w.empty())
                return true;
            for (std::size_t i = 1; i <= w.size(); ++i)
                if (in_a(word(w.begin(), w.begin() + static_cast<long>(i))) && in_star(word(w.begin() + static_cast<long>(i), w.end())))
                    return true;
            return false;
        };
        check_language(a.star(), in_star, "star");
        check_language(a.plus(), [&](const word& w) { return in_star(w) && (!w.empty() || in_a(w)); }, "plus");
        check_language(a.optional(), [&](const word& w) { return w.empty() || in_a(w); }, "optional");
    }
}

TEST_CASE("decisions agree with each other and with the simulation", "[random]") {
    std::mt19937 rng(99);
    const auto words = all_words();
    for (int round = 0; round < 400; ++round) {
        automaton a = random_nfa(rng, round % 2 == 0), b = random_nfa(rng, round % 3 == 0);
        if (round % 7 == 0)
            b = a.reduce();
        bool same = a.same_language(b);
        auto ce = a.counter_example(b);
        REQUIRE(same == !ce.has_value());
        REQUIRE(same == (a == b));
        if (ce)
            REQUIRE(accepts(a, *ce) != accepts(b, *ce));
        else
            for (const auto& w : words)
                REQUIRE(accepts(a, w) == accepts(b, w));
        bool subset = a.is_subset_of(b);
        auto witness = (a - b).shortest_word();
        REQUIRE(subset == !witness.has_value());
        if (witness)
            REQUIRE((accepts(a, *witness) && !accepts(b, *witness)));
        REQUIRE((a <= b) == subset);
        REQUIRE(same == (subset && b.is_subset_of(a)));

        auto shortest = a.shortest_word();
        REQUIRE(a.is_empty() == !shortest.has_value());
        if (shortest) {
            REQUIRE(accepts(a, *shortest));
            for (const auto& w : words)
                if (w.size() < shortest->size())
                    REQUIRE_FALSE(accepts(a, w));
        }
        // With n states, a language is infinite iff it has a word whose length is between n and 2n - 1
        bool finite = a.is_finite();
        auto m = a.minimize();
        bool long_word = std::ranges::any_of(words, [&](const word& w) {
            return w.size() >= m.state_count() && w.size() < 2 * m.state_count() && accepts(a, w);
        });
        if (2 * m.state_count() - 1 <= max_length)
            REQUIRE(finite == !long_word);
        REQUIRE(a.is_universal() == (~a).is_empty());
    }
}
