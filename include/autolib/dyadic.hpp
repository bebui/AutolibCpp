// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Exact binary fractions, for sums of doubles without rounding.

#pragma once

#include <compare>
#include <cstdint>
#include <iosfwd>
#include <string>

#include "autolib/big_integer.hpp"
#include "autolib/export.hpp"

namespace autolib {

/// An exact binary fraction `mantissa * 2^exponent`. Every finite double is one, and sums, differences and
/// products of dyadics are exact : they compute the sums of costs that doubles would round.
///
/// ```cpp
/// dyadic x(0.1), y(0.2);
/// bool exact = (x + y).to_double() == 0.30000000000000004;   // rounded once, from the exact sum
/// bool equal = x + y == dyadic(0.1 + 0.2);                   // false : the double sum was rounded
/// std::string s = dyadic(0.75).to_string();                  // "0.75"
/// ```
///
/// The representation is canonical : the mantissa is odd, or zero with a zero exponent.
class AUTOLIB_EXPORT dyadic {
public:
    /// Zero.
    dyadic() = default;
    /// The exact value of a finite double.
    /// \throws std::invalid_argument if the double is infinite or not a number
    explicit dyadic(double value);
    /// `mantissa * 2^exponent`.
    dyadic(big_integer mantissa, std::int64_t exponent = 0);  // NOLINT(google-explicit-constructor) : integers are exact

    [[nodiscard]] const big_integer& mantissa() const noexcept { return mantissa_; }
    [[nodiscard]] std::int64_t exponent() const noexcept { return exponent_; }
    [[nodiscard]] int signum() const noexcept { return mantissa_.signum(); }
    [[nodiscard]] bool is_zero() const noexcept { return mantissa_.is_zero(); }

    /// The nearest double, ties to even, including among the subnormal numbers; infinite beyond the largest
    /// double.
    [[nodiscard]] double to_double() const noexcept;
    /// The exact decimal representation, which is always finite : "-12.375", "0.1000000000000000055511151231257827021181583404541015625".
    [[nodiscard]] std::string to_string() const;

    dyadic& operator+=(const dyadic& other);
    dyadic& operator-=(const dyadic& other);
    dyadic& operator*=(const dyadic& other);
    [[nodiscard]] dyadic operator-() const {
        dyadic r(*this);
        r.mantissa_ = -std::move(r.mantissa_);
        return r;
    }
    [[nodiscard]] friend dyadic operator+(dyadic a, const dyadic& b) { return std::move(a += b); }
    [[nodiscard]] friend dyadic operator-(dyadic a, const dyadic& b) { return std::move(a -= b); }
    [[nodiscard]] friend dyadic operator*(dyadic a, const dyadic& b) { return std::move(a *= b); }

    [[nodiscard]] friend bool operator==(const dyadic& a, const dyadic& b) noexcept {
        return a.exponent_ == b.exponent_ && a.mantissa_ == b.mantissa_;
    }
    [[nodiscard]] friend std::strong_ordering operator<=>(const dyadic& a, const dyadic& b) {
        return a.exponent_ == b.exponent_ ? a.mantissa_ <=> b.mantissa_ : a.compare(b);
    }

    AUTOLIB_EXPORT friend std::ostream& operator<<(std::ostream& out, const dyadic& x);
    [[nodiscard]] std::size_t hash() const noexcept;

private:
    [[nodiscard]] std::strong_ordering compare(const dyadic& other) const;
    void normalize();

    big_integer mantissa_;
    std::int64_t exponent_ = 0;
};

}  // namespace autolib

template <>
struct std::hash<autolib::dyadic> {
    std::size_t operator()(const autolib::dyadic& x) const noexcept { return x.hash(); }
};
