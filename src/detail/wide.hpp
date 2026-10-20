// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <bit>
#include <cstdint>

#if defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
#include <intrin.h>
#endif

namespace autolib::detail {

/// A 128-bit unsigned value.
struct wide {
    std::uint64_t lo;
    std::uint64_t hi;
};

#if defined(__SIZEOF_INT128__) && !defined(AUTOLIB_PORTABLE_WIDE)
__extension__ using uint128 = unsigned __int128;
#define AUTOLIB_HAS_INT128 1
#endif

/// The full product of two 64-bit values, without the native 128-bit type.
[[nodiscard]] constexpr wide portable_mul_wide(std::uint64_t a, std::uint64_t b) noexcept {
    std::uint64_t a0 = a & 0xffffffffU, a1 = a >> 32, b0 = b & 0xffffffffU, b1 = b >> 32;
    std::uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    std::uint64_t middle = (p00 >> 32) + (p01 & 0xffffffffU) + (p10 & 0xffffffffU);
    return {(middle << 32) | (p00 & 0xffffffffU), p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32)};
}

/// The quotient of the 128-bit value (hi, lo) by d, which must exceed hi, and its remainder : the division of
/// a two-digit number by a one-digit number in base 2^32 (Hacker's Delight, divlu).
[[nodiscard]] constexpr std::uint64_t portable_div_wide(std::uint64_t hi, std::uint64_t lo, std::uint64_t d,
                                                        std::uint64_t& remainder) noexcept {
    constexpr std::uint64_t b = std::uint64_t{1} << 32;
    int s = std::countl_zero(d);
    d <<= s;
    std::uint64_t vn1 = d >> 32, vn0 = d & 0xffffffffU;
    std::uint64_t un32 = s == 0 ? hi : (hi << s) | (lo >> (64 - s));
    std::uint64_t un10 = lo << s;
    std::uint64_t un1 = un10 >> 32, un0 = un10 & 0xffffffffU;
    std::uint64_t q1 = un32 / vn1, rhat = un32 - q1 * vn1;
    while (q1 >= b || q1 * vn0 > b * rhat + un1) {
        --q1;
        rhat += vn1;
        if (rhat >= b)
            break;
    }
    std::uint64_t un21 = un32 * b + un1 - q1 * d;
    std::uint64_t q0 = un21 / vn1;
    rhat = un21 - q0 * vn1;
    while (q0 >= b || q0 * vn0 > b * rhat + un0) {
        --q0;
        rhat += vn1;
        if (rhat >= b)
            break;
    }
    remainder = (un21 * b + un0 - q0 * d) >> s;
    return q1 * b + q0;
}

[[nodiscard]] inline wide mul_wide(std::uint64_t a, std::uint64_t b) noexcept {
#if defined(AUTOLIB_HAS_INT128)
    uint128 p = static_cast<uint128>(a) * b;
    return {static_cast<std::uint64_t>(p), static_cast<std::uint64_t>(p >> 64)};
#elif defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64) && !defined(AUTOLIB_PORTABLE_WIDE)
    std::uint64_t hi;
    std::uint64_t lo = _umul128(a, b, &hi);
    return {lo, hi};
#else
    return portable_mul_wide(a, b);
#endif
}

/// The quotient of (hi, lo) by d, which must exceed hi, and its remainder.
[[nodiscard]] inline std::uint64_t div_wide(std::uint64_t hi, std::uint64_t lo, std::uint64_t d, std::uint64_t& remainder) noexcept {
#if defined(AUTOLIB_HAS_INT128)
    uint128 n = (static_cast<uint128>(hi) << 64) | lo;
    remainder = static_cast<std::uint64_t>(n % d);
    return static_cast<std::uint64_t>(n / d);
#elif defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64) && !defined(AUTOLIB_PORTABLE_WIDE)
    return _udiv128(hi, lo, d, &remainder);
#else
    return portable_div_wide(hi, lo, d, remainder);
#endif
}

/// a + b + carry, the carry being updated.
[[nodiscard]] inline std::uint64_t add_carry(std::uint64_t a, std::uint64_t b, std::uint64_t& carry) noexcept {
    std::uint64_t s = a + b;
    std::uint64_t c1 = s < a;
    std::uint64_t r = s + carry;
    carry = c1 | (r < s);
    return r;
}

/// a - b - borrow, the borrow being updated.
[[nodiscard]] inline std::uint64_t sub_borrow(std::uint64_t a, std::uint64_t b, std::uint64_t& borrow) noexcept {
    std::uint64_t d = a - b;
    std::uint64_t b1 = a < b;
    std::uint64_t r = d - borrow;
    borrow = b1 | (d < borrow);
    return r;
}

}  // namespace autolib::detail
