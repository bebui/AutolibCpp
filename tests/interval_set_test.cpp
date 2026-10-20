// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <autolib/interval_set.hpp>
#include <catch2/catch_test_macros.hpp>
#include <random>
#include <set>

using namespace autolib;

TEST_CASE("interval sets are normalized", "[interval_set]") {
    interval_set s;
    REQUIRE(s.empty());
    REQUIRE(s.insert(5));
    REQUIRE(s.insert(interval{1, 3}));
    REQUIRE(s.interval_count() == 2);
    REQUIRE(s.insert(4));  // joins [1,3] and [5,5]
    REQUIRE(s.interval_count() == 1);
    REQUIRE(s == interval_set::range(1, 5));
    REQUIRE_FALSE(s.insert(2));
    REQUIRE(s.cardinality() == 5);
    REQUIRE(interval_set::of({9, 1, 2, 2, 3, 7}).to_string() == "{1..3, 7, 9}");
    REQUIRE(interval_set::range(5, 4).empty());
}

TEST_CASE("operators on interval sets", "[interval_set]") {
    auto digits = interval_set::range(0, 9);
    auto odd = interval_set::of({1, 3, 5, 7, 9});
    REQUIRE((digits - odd) == interval_set::of({0, 2, 4, 6, 8}));
    REQUIRE((digits & odd) == odd);
    REQUIRE((odd | interval_set::of({0, 2})) == interval_set::of({0, 1, 2, 3, 5, 7, 9}));
    REQUIRE(~~odd == odd);
    REQUIRE((~interval_set{}) == interval_set::all());
    REQUIRE((~interval_set::all()).empty());
    REQUIRE(interval_set::all().cardinality() == (std::uint64_t{1} << 32));
    REQUIRE((~digits).contains(min_symbol));
    REQUIRE((~digits).contains(max_symbol));
    REQUIRE_FALSE((~digits).contains(5));
    REQUIRE(digits.contains(odd));
    REQUIRE_FALSE(odd.contains(digits));
    REQUIRE(digits.intersects(odd));
    REQUIRE_FALSE(odd.intersects(digits - odd));
    REQUIRE(interval_set::range(1, 2) < interval_set::range(1, 3));
}

TEST_CASE("interval sets match a reference implementation", "[interval_set][random]") {
    std::mt19937 rng(42);
    auto random_set = [&](std::set<int>& ref) {
        interval_set s;
        int n = static_cast<int>(rng() % 6);
        for (int i = 0; i < n; ++i) {
            int lo = static_cast<int>(rng() % 40), hi = lo + static_cast<int>(rng() % 5);
            s.insert(interval{lo, hi});
            for (int v = lo; v <= hi; ++v)
                ref.insert(v);
        }
        return s;
    };
    for (int round = 0; round < 2000; ++round) {
        std::set<int> ra, rb;
        auto a = random_set(ra), b = random_set(rb);
        auto u = a | b, i = a & b, d = a - b;
        for (int v = -2; v < 50; ++v) {
            bool in_a = ra.contains(v), in_b = rb.contains(v);
            REQUIRE(a.contains(v) == in_a);
            REQUIRE(u.contains(v) == (in_a || in_b));
            REQUIRE(i.contains(v) == (in_a && in_b));
            REQUIRE(d.contains(v) == (in_a && !in_b));
            REQUIRE((~a).contains(v) == !in_a);
        }
        REQUIRE(a.intersects(b) == !i.empty());
        REQUIRE(a.contains(b) == (b - a).empty());
        REQUIRE(a.cardinality() == ra.size());
        // Normalized : sorted, disjoint and non adjacent
        for (std::size_t k = 1; k < u.interval_count(); ++k)
            REQUIRE(static_cast<long>(u.intervals()[k - 1].max) + 1 < u.intervals()[k].min);
        // Copies and moves keep the value, inline or on the heap
        interval_set c = u;
        REQUIRE(c == u);
        interval_set m = std::move(c);
        REQUIRE(m == u);
        c = m;
        m = std::move(c);
        REQUIRE(m == u);
    }
}
