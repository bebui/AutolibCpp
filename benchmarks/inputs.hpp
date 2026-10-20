// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Inputs identical to those of the Java benchmarks (tools/java-bench), generated with the algorithm of java.util.Random.

#pragma once

#include <autolib/autolib.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace inputs {

/// The linear congruential generator of java.util.Random
class java_random {
public:
    explicit java_random(std::int64_t seed) : seed_((static_cast<std::uint64_t>(seed) ^ 0x5DEECE66DULL) & mask) {}

    std::int32_t next_int(std::int32_t bound) {
        std::int32_t r = next(31);
        std::int32_t m = bound - 1;
        if ((bound & m) == 0)
            return static_cast<std::int32_t>((static_cast<std::int64_t>(bound) * r) >> 31);
        for (std::int32_t u = r; u - (r = u % bound) + m < 0; u = next(31)) {
        }
        return r;
    }

private:
    static constexpr std::uint64_t mask = (std::uint64_t{1} << 48) - 1;
    std::int32_t next(int bits) {
        seed_ = (seed_ * 0x5DEECE66DULL + 0xBULL) & mask;
        return static_cast<std::int32_t>(static_cast<std::int64_t>(seed_ >> (48 - bits)));
    }
    std::uint64_t seed_;
};

/// The n-th symbol from the end is 0 : minimal DFA with 2^(k+1) states
inline std::string nth_from_end(int k) {
    return "(0|1)*0(0|1){" + std::to_string(k) + "}";
}

inline std::string symbol(int v) {
    return v < 10 ? std::to_string(v) : "<" + std::to_string(v) + ">";
}

/// Alternation of n random words of length 4 over 20 symbols
inline std::string words(int n, std::int64_t seed) {
    java_random r(seed);
    std::string out;
    for (int i = 0; i < n; ++i) {
        if (i > 0)
            out += '|';
        for (int j = 0; j < 4; ++j)
            out += symbol(r.next_int(20));
    }
    return out;
}

/// n states, per_state transitions per state on sigma symbols, a third of the states accepting
inline autolib::automaton random_nfa(std::int64_t seed, int n, int sigma, int per_state) {
    java_random r(seed);
    std::vector<std::array<int, 3>> edges;
    for (int s = 0; s < n; ++s)
        for (int j = 0; j < per_state; ++j) {
            int v = r.next_int(sigma);
            edges.push_back({s, v, r.next_int(n)});
        }
    autolib::automaton a;
    for (int s = 0; s < n; ++s)
        a.add_state(r.next_int(3) == 0);
    a.set_initial(0);
    for (auto [s, v, t] : edges)
        a.add_transition(s, t, v);
    return a;
}

/// A DFA where each state reads distinct symbols, plus the symbol sigma towards the next state
inline autolib::automaton random_dfa(std::int64_t seed, int n, int sigma, int per_state) {
    java_random r(seed);
    std::vector<std::array<int, 3>> edges;
    for (int s = 0; s < n; ++s) {
        std::set<int> used;
        for (int j = 0; j < per_state; ++j) {
            int v = r.next_int(sigma);
            if (used.insert(v).second)
                edges.push_back({s, v, r.next_int(n)});
        }
        edges.push_back({s, sigma, (s + 1) % n});
    }
    autolib::automaton a;
    for (int s = 0; s < n; ++s)
        a.add_state(r.next_int(3) == 0);
    a.set_initial(0);
    for (auto [s, v, t] : edges)
        a.add_transition(s, t, v);
    return a;
}

inline autolib::word random_word(std::int64_t seed, int length, int sigma) {
    java_random r(seed);
    autolib::word w(static_cast<std::size_t>(length));
    for (auto& s : w)
        s = r.next_int(sigma);
    return w;
}

/// A weighted automaton : `per_state` transitions and `epsilons` epsilon edges leaving each state, toward any
/// state or, for a DAG, one of the 4 next states; weights in quarters, from low / 4 to (low + 16) / 4.
inline autolib::weighted_automaton weighted(std::int64_t seed, int n, int sigma, int per_state, int epsilons, int low, bool dag) {
    java_random r(seed);
    autolib::weighted_automaton::builder b;
    for (int s = 0; s < n; ++s)
        b.add_state();
    b.set_initial(0);
    for (int s = 0; s < n; ++s)
        if (r.next_int(4) == 0 || (dag && s == n - 1))
            b.set_accept(s, r.next_int(8) / 4.0);
    for (int s = 0; s < (dag ? n - 1 : n); ++s) {
        auto target = [&] { return dag ? s + 1 + r.next_int(std::min(4, n - 1 - s)) : r.next_int(n); };
        for (int j = 0; j < per_state; ++j) {
            int v = r.next_int(sigma);
            int t = target();
            b.add_transition(s, t, v, (r.next_int(17) + low) / 4.0);
        }
        for (int j = 0; j < epsilons; ++j) {
            int t = target();
            b.add_epsilon(s, t, r.next_int(9) / 4.0);
        }
    }
    return b.build();
}

/// A complete weighted DFA over sigma symbols, weights in quarters from low / 4 to (low + 16) / 4.
inline autolib::weighted_automaton weighted_dfa(std::int64_t seed, int n, int sigma, int low) {
    java_random r(seed);
    autolib::weighted_automaton::builder b;
    for (int s = 0; s < n; ++s)
        b.add_state();
    b.set_initial(0);
    for (int s = 0; s < n; ++s)
        if (r.next_int(4) == 0)
            b.set_accept(s, r.next_int(8) / 4.0);
    for (int s = 0; s < n; ++s)
        for (int c = 0; c < sigma; ++c) {
            int t = r.next_int(n);
            b.add_transition(s, t, c, (r.next_int(17) + low) / 4.0);
        }
    return b.build();
}

/// A DAG weighted by pairs of integers, `per_state` edges reading 0 toward one of the 4 next states.
inline autolib::multi_weighted_automaton multi_dag(std::int64_t seed, int n, int per_state) {
    java_random r(seed);
    autolib::multi_weighted_automaton::builder b(2);
    for (int s = 0; s < n; ++s)
        b.add_state();
    b.set_initial(0);
    b.set_accept(n - 1);
    for (int s = 0; s < n - 1; ++s)
        for (int j = 0; j < per_state; ++j) {
            int t = s + 1 + r.next_int(std::min(4, n - 1 - s));
            double x = r.next_int(9), y = r.next_int(9);
            b.add_transition(s, t, 0, {x, y});
        }
    return b.build();
}

}  // namespace inputs
