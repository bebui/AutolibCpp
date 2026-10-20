// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <autolib/big_integer.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>
#include <unordered_set>
#include <vector>

using namespace autolib;
using namespace autolib::literals;

TEST_CASE("big integers convert from and to built-in integers", "[big_integer]") {
    REQUIRE(big_integer().is_zero());
    REQUIRE(big_integer(0).signum() == 0);
    REQUIRE(big_integer(-5).signum() == -1);
    REQUIRE(big_integer(-5).to_string() == "-5");
    auto min = big_integer(std::numeric_limits<std::int64_t>::min());
    REQUIRE(min.to_string() == "-9223372036854775808");
    REQUIRE(min.to<std::int64_t>() == std::numeric_limits<std::int64_t>::min());
    REQUIRE_FALSE((min - 1).to<std::int64_t>());
    REQUIRE_FALSE(min.to<std::uint64_t>());
    auto max = big_integer(std::numeric_limits<std::uint64_t>::max());
    REQUIRE(max.to<std::uint64_t>() == std::numeric_limits<std::uint64_t>::max());
    REQUIRE_FALSE(max.to<std::int64_t>());
    REQUIRE_FALSE((max + 1).to<std::uint64_t>());
    REQUIRE(big_integer(200).to<std::int8_t>() == std::nullopt);
    REQUIRE(big_integer(-128).to<std::int8_t>() == std::int8_t{-128});
    REQUIRE(big_integer(-129).to<std::int8_t>() == std::nullopt);
    REQUIRE(big_integer(255U).to<std::uint8_t>() == std::uint8_t{255});
}

TEST_CASE("big integers parse and print", "[big_integer]") {
    REQUIRE(big_integer("0").is_zero());
    REQUIRE(big_integer("-0").signum() == 0);
    REQUIRE(big_integer("+42") == 42);
    REQUIRE(big_integer("-000123").to_string() == "-123");
    std::string digits = "123456789012345678901234567890123456789012345678901234567890";
    REQUIRE(big_integer(digits).to_string() == digits);
    REQUIRE(big_integer("-" + digits).to_string() == "-" + digits);
    REQUIRE(big_integer("10000000000000000000000000000000000000").to_string() == "10000000000000000000000000000000000000");
    REQUIRE(big_integer::parse("ff", 16) == 255);
    REQUIRE(big_integer::parse("-Zz", 36) == -(35 * 36 + 35));
    REQUIRE(big_integer::parse("1" + std::string(100, '0'), 2) == big_integer(1) << 100);
    REQUIRE_THROWS_AS(big_integer(""), std::invalid_argument);
    REQUIRE_THROWS_AS(big_integer("-"), std::invalid_argument);
    REQUIRE_THROWS_AS(big_integer("12a"), std::invalid_argument);
    REQUIRE_THROWS_AS(big_integer(" 1"), std::invalid_argument);
    REQUIRE_THROWS_AS(big_integer::parse("2", 2), std::invalid_argument);
    REQUIRE_THROWS_AS(big_integer::parse("1", 37), std::invalid_argument);
    REQUIRE(123456789012345678901234567890_big == big_integer("123456789012345678901234567890"));
    REQUIRE(1'000'000_big == 1000000);
    REQUIRE(0xffff'ffff'ffff'ffff'ffff_big == (big_integer(1) << 80) - 1);
    REQUIRE(0b1010_big == 10);
    REQUIRE(017_big == 15);
    REQUIRE(0_big == 0);
    std::ostringstream out;
    out << -(big_integer(1) << 70);
    REQUIRE(out.str() == "-1180591620717411303424");
}

TEST_CASE("big integers follow the rules of the built-in integers", "[big_integer]") {
    REQUIRE(big_integer(7) / 2 == 3);
    REQUIRE(big_integer(-7) / 2 == -3);
    REQUIRE(big_integer(-7) % 2 == -1);
    REQUIRE(big_integer(7) % -2 == 1);
    REQUIRE((big_integer(-7) >> 1) == -4);
    REQUIRE((big_integer(-8) >> 1) == -4);
    REQUIRE((big_integer(-1) >> 200) == -1);
    REQUIRE((big_integer(5) >> 200) == 0);
    REQUIRE_THROWS_AS(big_integer(1) / 0, std::domain_error);
    REQUIRE_THROWS_AS(big_integer(1) % big_integer(), std::domain_error);
    big_integer x = 5;
    const big_integer& alias = x;  // operations of an integer with itself
    REQUIRE(x++ == 5);
    REQUIRE(++x == 7);
    REQUIRE(--x == 6);
    x += alias;
    REQUIRE(x == 12);
    x *= alias;
    REQUIRE(x == 144);
    x -= alias;
    REQUIRE(x.is_zero());
    REQUIRE_FALSE(x.is_negative());
    x = big_integer(1) << 200;
    x /= alias;
    REQUIRE(x == 1);
    x = big_integer(1) << 200;
    x %= alias;
    REQUIRE(x == 0);
    REQUIRE(big_integer::pow(3, 0) == 1);
    REQUIRE(big_integer::pow(-2, 101) == -(big_integer(1) << 101));
    REQUIRE(big_integer::pow(10, 40).to_string() == "1" + std::string(40, '0'));
    REQUIRE(-big_integer(0) == 0);
    REQUIRE((-big_integer(3)).abs() == 3);
    REQUIRE(big_integer(-2) < big_integer(1));
    REQUIRE(-(big_integer(1) << 100) < -(big_integer(1) << 99));
    REQUIRE((big_integer(1) << 100) > (big_integer(1) << 99));
    REQUIRE(big_integer(3) > 2);
    REQUIRE(2 < big_integer(3));
    REQUIRE(static_cast<bool>(big_integer(1)));
    REQUIRE(big_integer(1).bit_length() == 1);
    REQUIRE((big_integer(1) << 64).bit_length() == 65);
    REQUIRE((big_integer(1) << 64).magnitude().size() == 2);
}

TEST_CASE("big integers accumulate products", "[big_integer]") {
    big_integer total;
    total.add_product(big_integer(std::numeric_limits<std::uint64_t>::max()), std::numeric_limits<std::uint64_t>::max());
    REQUIRE(total == big_integer(std::numeric_limits<std::uint64_t>::max()) * std::numeric_limits<std::uint64_t>::max());
    total.add_product(-total, 2);
    REQUIRE(total == -big_integer(std::numeric_limits<std::uint64_t>::max()) * std::numeric_limits<std::uint64_t>::max());
    total.add_product(total, 3);
    REQUIRE(total == big_integer(-4) * std::numeric_limits<std::uint64_t>::max() * std::numeric_limits<std::uint64_t>::max());
    big_integer small = 2;
    small.add_product(3, 4);
    REQUIRE(small == 14);
    small.add_product(5, 0);
    REQUIRE(small == 14);
}

TEST_CASE("big integers convert to and from doubles", "[big_integer]") {
    REQUIRE(big_integer::from_double(0.0) == 0);
    REQUIRE(big_integer::from_double(-0.9) == 0);
    REQUIRE(big_integer::from_double(-3.9) == -3);
    REQUIRE(big_integer::from_double(1e30) == big_integer("1000000000000000019884624838656"));
    REQUIRE(big_integer::from_double(0x1p200) == big_integer(1) << 200);
    REQUIRE(big_integer::from_double(std::numeric_limits<double>::max()).to_double() == std::numeric_limits<double>::max());
    REQUIRE_THROWS_AS(big_integer::from_double(std::numeric_limits<double>::infinity()), std::invalid_argument);
    REQUIRE_THROWS_AS(big_integer::from_double(std::nan("")), std::invalid_argument);
    REQUIRE(big_integer(0).to_double() == 0.0);
}

TEST_CASE("big integers hash by value", "[big_integer]") {
    std::unordered_set<big_integer> seen{big_integer(1) << 100, 5, -5};
    REQUIRE(seen.contains(big_integer(1) << 100));
    REQUIRE(seen.contains(-5));
    REQUIRE_FALSE(seen.contains(6));
    big_integer a = big_integer(1) << 100;
    a -= big_integer(1) << 100;
    a += 5;
    REQUIRE(std::hash<big_integer>{}(a) == std::hash<big_integer>{}(big_integer(5)));
}

TEST_CASE("uniform big integers are in range and uniform", "[big_integer][random]") {
    std::mt19937 rng32(1);
    std::mt19937_64 rng64(1);
    std::vector<int> histogram(6, 0);
    for (int i = 0; i < 6000; ++i) {
        auto x = big_integer::uniform(6, rng32);
        histogram[static_cast<std::size_t>(*x.to<int>())]++;
    }
    for (int c : histogram)
        REQUIRE((c > 850 && c < 1150));
    auto bound = (big_integer(1) << 130) + 7;
    for (int i = 0; i < 500; ++i) {
        auto x = big_integer::uniform(bound, rng64);
        REQUIRE(x.signum() >= 0);
        REQUIRE(x < bound);
    }
    // The top half of a bound is drawn about half of the time
    int high = 0;
    auto half = bound >> 1;
    for (int i = 0; i < 2000; ++i)
        high += big_integer::uniform(bound, rng64) >= half;
    REQUIRE((high > 900 && high < 1100));
    REQUIRE_THROWS_AS(big_integer::uniform(0, rng64), std::domain_error);
}

TEST_CASE("big integers agree with the integers of Python", "[big_integer]") {
    std::ifstream in(AUTOLIB_TEST_DATA "/big_integer.tsv");
    REQUIRE(in.good());
    std::size_t lines = 0;
    for (std::string line; std::getline(in, line); ++lines) {
        std::vector<std::string> f;
        std::istringstream fields(line);
        for (std::string x; std::getline(fields, x, '\t');)
            f.push_back(x);
        REQUIRE(f.size() == 12);
        INFO(f[0] << " and " << f[1]);
        big_integer a(f[0]), b(f[1]);
        REQUIRE(a.to_string() == f[0]);
        REQUIRE((a + b).to_string() == f[2]);
        REQUIRE((a - b).to_string() == f[3]);
        REQUIRE((a * b).to_string() == f[4]);
        if (f[5] != "x") {
            REQUIRE((a / b).to_string() == f[5]);
            REQUIRE((a % b).to_string() == f[6]);
            auto [q, r] = big_integer::div_mod(a, b);
            REQUIRE(q == big_integer(f[5]));
            REQUIRE(r == big_integer(f[6]));
        }
        auto k = std::stoul(f[7]);
        REQUIRE((a << k).to_string() == f[8]);
        REQUIRE((a >> k).to_string() == f[9]);
        REQUIRE(a.to_double() == std::strtod(f[10].c_str(), nullptr));
        REQUIRE(a.bit_length() == std::stoul(f[11]));
        REQUIRE((a <=> b) == (big_integer(f[3]).signum() <=> 0));
        auto accumulated = a;
        accumulated.add_product(b.abs(), 3);
        REQUIRE(accumulated == a + b.abs() * 3);
        REQUIRE(big_integer::from_magnitude(a.magnitude(), a.is_negative()) == a);
    }
    REQUIRE(lines == 1500);
}
