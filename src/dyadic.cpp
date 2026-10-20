// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "autolib/dyadic.hpp"

#include <bit>
#include <cmath>
#include <limits>
#include <ostream>
#include <stdexcept>

namespace autolib {

namespace {

/// The number of trailing zero bits of a nonzero integer.
std::size_t trailing_zeros(const big_integer& x) noexcept {
    std::size_t zeros = 0;
    for (auto limb : x.magnitude()) {
        if (limb != 0)
            return zeros + static_cast<std::size_t>(std::countr_zero(limb));
        zeros += 64;
    }
    return zeros;
}

/// The position of the leading bit plus one, from the binary point : |x| is in [2^(top-1), 2^top).
std::int64_t top(const dyadic& x) noexcept {
    return static_cast<std::int64_t>(x.mantissa().bit_length()) + x.exponent();
}

/// |x| as top * 2^shift rounded to odd : the 64 leading bits, the lowest one also standing for the bits below,
/// so that rounding top to 53 bits rounds |x| correctly.
std::uint64_t leading_bits(const big_integer& x, std::int64_t& shift) noexcept {
    auto m = x.magnitude();
    auto length = x.bit_length();
    if (length <= 64) {
        shift = 0;
        return m.empty() ? 0 : m[0];
    }
    auto low = length - 64;
    shift = static_cast<std::int64_t>(low);
    std::size_t i = low / 64;
    auto offset = static_cast<int>(low % 64);
    std::uint64_t result = offset == 0 ? m[i] : (m[i] >> offset) | (m[i + 1] << (64 - offset));
    bool sticky = offset != 0 && (m[i] & ((std::uint64_t{1} << offset) - 1)) != 0;
    for (std::size_t j = 0; j < i && !sticky; ++j)
        sticky = m[j] != 0;
    return sticky ? result | 1 : result;
}

#if defined(__SIZEOF_INT128__)
#define AUTOLIB_DYADIC_FAST 1
__extension__ using int128 = __int128;
__extension__ using uint128 = unsigned __int128;

/// Aligns two mantissas of at most 64 bits whose exponents differ by at most 62 : x * 2^ex and y * 2^ey become
/// integers of the least exponent, returned.
std::int64_t align(std::int64_t x, std::int64_t ex, std::int64_t y, std::int64_t ey, int128& a, int128& b) noexcept {
    a = x;
    b = y;
    if (ex >= ey) {
        a *= int128{1} << (ex - ey);
        return ey;
    }
    b *= int128{1} << (ey - ex);
    return ex;
}
#endif

}  // namespace

dyadic::dyadic(double value) {
    if (!std::isfinite(value))
        throw std::invalid_argument("Not a finite double");
    if (value == 0)
        return;
    int e;
    double fraction = std::frexp(value, &e);  // value = fraction * 2^e, |fraction| in [0.5, 1)
    mantissa_ = static_cast<std::int64_t>(std::ldexp(fraction, 53));
    exponent_ = e - 53;
    normalize();
}

dyadic::dyadic(big_integer mantissa, std::int64_t exponent) : mantissa_(std::move(mantissa)), exponent_(exponent) {
    normalize();
}

void dyadic::normalize() {
    if (mantissa_.is_zero()) {
        exponent_ = 0;
        return;
    }
    auto zeros = trailing_zeros(mantissa_);
    if (zeros != 0) {
        mantissa_ >>= zeros;  // exact : only zeros are shifted out
        exponent_ += static_cast<std::int64_t>(zeros);
    }
}

std::strong_ordering dyadic::compare(const dyadic& other) const {
#if defined(AUTOLIB_DYADIC_FAST)
    // Mantissas of 64 bits, close exponents : compared in 128-bit integers
    if (auto x = mantissa_.to<std::int64_t>(), y = other.mantissa_.to<std::int64_t>(); x && y) {
        auto diff = exponent_ - other.exponent_;
        if (diff >= -62 && diff <= 62) {
            int128 a, b;
            align(*x, exponent_, *y, other.exponent_, a, b);
            return a < b ? std::strong_ordering::less : a > b ? std::strong_ordering::greater : std::strong_ordering::equal;
        }
    }
#endif
    int s = signum(), t = other.signum();
    if (s != t)
        return s <=> t;
    if (s == 0 || *this == other)
        return std::strong_ordering::equal;
    // Same sign : the magnitudes are compared by their leading bits first
    auto a = top(*this), b = top(other);
    if (a != b)
        return s > 0 ? a <=> b : b <=> a;
    auto e = std::min(exponent_, other.exponent_);
    return (mantissa_ << static_cast<std::size_t>(exponent_ - e)) <=>
           (other.mantissa_ << static_cast<std::size_t>(other.exponent_ - e));
}

dyadic& dyadic::operator+=(const dyadic& other) {
    if (other.is_zero())
        return *this;
    if (is_zero())
        return *this = other;
#if defined(AUTOLIB_DYADIC_FAST)
    // Mantissas of 64 bits, close exponents : the sum in 128-bit integers, without allocation
    if (auto x = mantissa_.to<std::int64_t>(), y = other.mantissa_.to<std::int64_t>(); x && y) {
        auto diff = exponent_ - other.exponent_;
        if (diff >= -62 && diff <= 62) {
            int128 a, b;
            auto e = align(*x, exponent_, *y, other.exponent_, a, b);
            int128 sum = a + b;
            if (sum == 0) {
                mantissa_ = 0;
                exponent_ = 0;
                return *this;
            }
            uint128 magnitude = sum < 0 ? uint128{0} - static_cast<uint128>(sum) : static_cast<uint128>(sum);
            auto low = static_cast<std::uint64_t>(magnitude);
            int zeros = low != 0 ? std::countr_zero(low) : 64 + std::countr_zero(static_cast<std::uint64_t>(magnitude >> 64));
            magnitude >>= zeros;
            exponent_ = e + zeros;
            if (magnitude >> 63 == 0) {
                auto m = static_cast<std::int64_t>(magnitude);
                mantissa_ = sum < 0 ? -m : m;
            } else {
                std::uint64_t limbs[2] = {static_cast<std::uint64_t>(magnitude), static_cast<std::uint64_t>(magnitude >> 64)};
                mantissa_ = big_integer::from_magnitude(limbs, sum < 0);
            }
            return *this;
        }
    }
#endif
    if (exponent_ == other.exponent_) {
        mantissa_ += other.mantissa_;
    } else if (exponent_ > other.exponent_) {
        mantissa_ <<= static_cast<std::size_t>(exponent_ - other.exponent_);
        mantissa_ += other.mantissa_;
        exponent_ = other.exponent_;
    } else {
        mantissa_ += other.mantissa_ << static_cast<std::size_t>(other.exponent_ - exponent_);
    }
    normalize();
    return *this;
}

dyadic& dyadic::operator-=(const dyadic& other) {
    return *this += -other;
}

dyadic& dyadic::operator*=(const dyadic& other) {
    mantissa_ *= other.mantissa_;
    exponent_ += other.exponent_;
    normalize();
    return *this;
}

double dyadic::to_double() const noexcept {
    if (is_zero())
        return 0.0;
    constexpr std::int64_t least = -1074;  // exponent of the smallest subnormal
    auto t = top(*this);
    if (t > 1025)
        return signum() > 0 ? HUGE_VAL : -HUGE_VAL;
    double magnitude;
    if (t - 53 >= least || exponent_ >= least) {
        // The rounding to 53 bits of the mantissa is the final one : scaling is exact, or overflows
        std::int64_t shift;
        auto bits = leading_bits(mantissa_, shift);
        magnitude = std::ldexp(static_cast<double>(bits), static_cast<int>(shift + exponent_));
    } else {
        // Subnormal result : rounded at the bit of weight 2^-1074, ties to even
        if (t < least - 1)
            return signum() > 0 ? 0.0 : -0.0;
        auto shift = static_cast<std::size_t>(least - exponent_);
        big_integer m = mantissa_.abs();
        big_integer q = m >> shift;
        big_integer rest = m - (q << shift);
        big_integer half = big_integer(1) << (shift - 1);
        auto c = rest <=> half;
        if (c > 0 || (c == 0 && q.magnitude().size() > 0 && (q.magnitude()[0] & 1) != 0))
            ++q;
        magnitude = std::ldexp(q.to_double(), static_cast<int>(least));
    }
    return signum() > 0 ? magnitude : -magnitude;
}

std::string dyadic::to_string() const {
    if (exponent_ >= 0)
        return (mantissa_ << static_cast<std::size_t>(exponent_)).to_string();
    // m / 2^k = m * 5^k / 10^k
    auto k = static_cast<std::size_t>(-exponent_);
    std::string digits = (mantissa_.abs() * big_integer::pow(5, static_cast<unsigned>(k))).to_string();
    if (digits.size() <= k)
        digits.insert(0, k + 1 - digits.size(), '0');
    digits.insert(digits.size() - k, 1, '.');
    return signum() < 0 ? "-" + digits : digits;
}

std::ostream& operator<<(std::ostream& out, const dyadic& x) {
    return out << x.to_string();
}

std::size_t dyadic::hash() const noexcept {
    return mantissa_.hash() ^ (static_cast<std::size_t>(exponent_) * 0x9e3779b97f4a7c15ULL);
}

}  // namespace autolib
