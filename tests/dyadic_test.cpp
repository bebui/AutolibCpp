// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <autolib/dyadic.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <vector>

using namespace autolib;

TEST_CASE("dyadics hold doubles exactly", "[dyadic]") {
    REQUIRE(dyadic().is_zero());
    REQUIRE(dyadic(0.0) == dyadic());
    REQUIRE(dyadic(-0.0) == dyadic());
    REQUIRE(dyadic(0.75).mantissa() == 3);
    REQUIRE(dyadic(0.75).exponent() == -2);
    REQUIRE(dyadic(12.0) == dyadic(3, 2));
    REQUIRE(dyadic(big_integer(40), -3) == dyadic(5.0));
    REQUIRE(dyadic(0.75).to_string() == "0.75");
    REQUIRE(dyadic(-12.375).to_string() == "-12.375");
    REQUIRE(dyadic(0.1).to_string() == "0.1000000000000000055511151231257827021181583404541015625");
    REQUIRE(dyadic(1e20).to_string() == "100000000000000000000");
    REQUIRE(dyadic(0.0).to_string() == "0");
    for (double x : {1.0, -2.5, 0.1, 1e300, 5e-324, -std::numeric_limits<double>::max(), 2.2250738585072014e-308})
        REQUIRE(dyadic(x).to_double() == x);
    REQUIRE_THROWS_AS(dyadic(std::numeric_limits<double>::infinity()), std::invalid_argument);
    REQUIRE_THROWS_AS(dyadic(std::nan("")), std::invalid_argument);
}

TEST_CASE("dyadic sums are exact", "[dyadic]") {
    dyadic x(0.1), y(0.2);
    REQUIRE((x + y).to_double() == 0.30000000000000004);
    REQUIRE(x + y != dyadic(0.1 + 0.2));
    // 1e16 + 1 - 1e16 is 1 exactly, and 0 in doubles
    REQUIRE(dyadic(1e16) + dyadic(1.0) - dyadic(1e16) == dyadic(1.0));
    REQUIRE((dyadic(0.5) - dyadic(0.5)).is_zero());
    REQUIRE((dyadic(0.5) - dyadic(0.5)).exponent() == 0);
    REQUIRE(dyadic(0.5) * dyadic(-0.25) == dyadic(-0.125));
    REQUIRE(dyadic(3.0) < dyadic(3.5));
    REQUIRE(dyadic(-3.0) > dyadic(-3.5));
    REQUIRE(dyadic(5e-324) > dyadic());
    REQUIRE(dyadic(-5e-324) < dyadic());
    REQUIRE(dyadic(1e300) > dyadic(1e-300));
    REQUIRE(-dyadic(2.0) == dyadic(-2.0));
    // Beyond doubles
    dyadic huge = dyadic(std::numeric_limits<double>::max()) * dyadic(4.0);
    REQUIRE(huge.to_double() == HUGE_VAL);
    REQUIRE((-huge).to_double() == -HUGE_VAL);
    dyadic tiny = dyadic(5e-324) * dyadic(0.25);
    REQUIRE(tiny.to_double() == 0.0);
    REQUIRE(tiny > dyadic());
    std::unordered_set<dyadic> seen{dyadic(0.5), dyadic(1.0)};
    REQUIRE(seen.contains(dyadic(0.25) + dyadic(0.25)));
    REQUIRE_FALSE(seen.contains(dyadic(0.75)));
    std::ostringstream out;
    out << dyadic(-0.5);
    REQUIRE(out.str() == "-0.5");
}

TEST_CASE("dyadics agree with the fractions of Python", "[dyadic]") {
    std::ifstream in(AUTOLIB_TEST_DATA "/dyadic.tsv");
    REQUIRE(in.good());
    auto parse = [](const std::string& text) { return std::strtod(text.c_str(), nullptr); };
    std::size_t lines = 0;
    for (std::string line; std::getline(in, line); ++lines) {
        std::vector<std::string> f;
        std::istringstream fields(line);
        for (std::string x; std::getline(fields, x, '\t');)
            f.push_back(x);
        REQUIRE(f.size() == 9);
        INFO(line.substr(0, 120));
        dyadic a(parse(f[0])), b(parse(f[1])), c(parse(f[2])), d(parse(f[3]));
        dyadic x = a + b + c - d, y = a * b;
        REQUIRE(x.to_double() == parse(f[4]));
        REQUIRE(y.to_double() == parse(f[5]));
        REQUIRE(std::signbit(y.to_double()) == (f[5][0] == '-'));
        int sign = std::stoi(f[6]);
        REQUIRE((a <=> b) == (sign < 0 ? std::strong_ordering::less : sign > 0 ? std::strong_ordering::greater : std::strong_ordering::equal));
        REQUIRE(x.to_string() == f[7]);
        REQUIRE(y.to_string() == f[8]);
        REQUIRE(x - x == dyadic());
    }
    REQUIRE(lines == 600);
}
