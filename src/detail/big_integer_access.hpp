// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

#include "autolib/big_integer.hpp"
#include "detail/wide.hpp"

namespace autolib {

/// Access to the storage of big integers, for the algorithms on magnitudes.
struct big_integer_access {
    using limb = big_integer::limb;

    /// Makes room for n limbs, keeping the current ones, and returns the storage.
    static limb* reserve(big_integer& x, std::size_t n) {
        if (n <= x.capacity_)
            return x.data();
        if (n > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Integer too large");
        auto grown = std::max<std::size_t>(n, 2 * std::size_t{x.capacity_});
        grown = std::min<std::size_t>(grown, std::numeric_limits<std::uint32_t>::max());
        auto* fresh = new limb[grown];
        std::copy_n(x.data(), x.size_, fresh);
        if (x.capacity_ > 1)
            delete[] x.heap_;
        x.heap_ = fresh;
        x.capacity_ = static_cast<std::uint32_t>(grown);
        return fresh;
    }
    /// Makes room for n limbs, the limbs beyond the current ones being zero.
    static limb* reserve_zeroed(big_integer& x, std::size_t n) {
        limb* r = reserve(x, n);
        std::fill(r + x.size_, r + n, limb{0});
        return r;
    }
    /// Sets the number of limbs, then drops the leading zeros.
    static void normalize(big_integer& x, std::size_t n) noexcept {
        const limb* r = x.data();
        while (n > 0 && r[n - 1] == 0)
            --n;
        x.size_ = static_cast<std::uint32_t>(n);
        if (n == 0)
            x.negative_ = false;
    }
    /// |x| += |a| * b, a not being stored in x
    static void add_product(big_integer& x, const limb* a, std::size_t an, limb b) {
        if (an == 0 || b == 0)
            return;
        std::size_t n = std::max<std::size_t>(x.size_, an + 1) + 1;
        limb* r = reserve_zeroed(x, n);
        limb mul_carry = 0, carry = 0;
        for (std::size_t i = 0; i < an; ++i) {
            auto w = detail::mul_wide(a[i], b);
            limb lo = w.lo + mul_carry;
            mul_carry = w.hi + (lo < w.lo);
            r[i] = detail::add_carry(r[i], lo, carry);
        }
        r[an] = detail::add_carry(r[an], mul_carry, carry);
        for (std::size_t i = an + 1; carry != 0 && i < n; ++i)
            r[i] = detail::add_carry(r[i], 0, carry);
        normalize(x, n);
    }
    /// x = |a|, keeping the storage of x
    static void assign_magnitude(big_integer& x, std::span<const limb> a) {
        x.size_ = 0;
        std::copy(a.begin(), a.end(), reserve(x, a.size()));
        x.size_ = static_cast<std::uint32_t>(a.size());
        x.negative_ = false;
    }
    static void set_negative(big_integer& x, bool negative) noexcept { x.negative_ = negative && x.size_ != 0; }
    static std::size_t size(const big_integer& x) noexcept { return x.size_; }
    static const limb* data(const big_integer& x) noexcept { return x.data(); }

    /// |x| += |b|
    static void add_magnitude(big_integer& x, const limb* b, std::size_t bn) {
        std::size_t xn = x.size_, n = std::max(xn, bn) + 1;
        limb* r = reserve_zeroed(x, n);
        limb carry = 0;
        std::size_t i = 0;
        for (; i < bn; ++i)
            r[i] = detail::add_carry(r[i], b[i], carry);
        for (; carry != 0 && i < n; ++i)
            r[i] = detail::add_carry(r[i], 0, carry);
        normalize(x, n);
    }
    /// |x| -= |b|, with |x| >= |b|
    static void subtract_magnitude(big_integer& x, const limb* b, std::size_t bn) noexcept {
        limb* r = x.data();
        limb borrow = 0;
        std::size_t i = 0;
        for (; i < bn; ++i)
            r[i] = detail::sub_borrow(r[i], b[i], borrow);
        for (; borrow != 0 && i < x.size_; ++i)
            r[i] = detail::sub_borrow(r[i], 0, borrow);
        normalize(x, x.size_);
    }
    /// |x| = |b| - |x|, with |b| > |x|
    static void subtract_from_magnitude(big_integer& x, const limb* b, std::size_t bn) {
        limb* r = reserve_zeroed(x, bn);
        limb borrow = 0;
        for (std::size_t i = 0; i < bn; ++i)
            r[i] = detail::sub_borrow(b[i], r[i], borrow);
        normalize(x, bn);
    }
    static int compare(const limb* a, std::size_t an, const limb* b, std::size_t bn) noexcept {
        if (an != bn)
            return an < bn ? -1 : 1;
        for (std::size_t i = an; i-- > 0;)
            if (a[i] != b[i])
                return a[i] < b[i] ? -1 : 1;
        return 0;
    }
    /// x += (negative ? -1 : 1) * |b|; b must not be stored in x
    static void add_signed(big_integer& x, const limb* b, std::size_t bn, bool negative) {
        if (bn == 0)
            return;
        if (x.size_ == 0 || x.negative_ == negative) {
            add_magnitude(x, b, bn);
            set_negative(x, negative);
            return;
        }
        int c = compare(x.data(), x.size_, b, bn);
        if (c >= 0) {
            subtract_magnitude(x, b, bn);
        } else {
            subtract_from_magnitude(x, b, bn);
            set_negative(x, negative);
        }
    }
    /// |x| = |x| * m + a
    static void multiply_add(big_integer& x, limb m, limb a) {
        std::size_t xn = x.size_;
        limb* r = reserve_zeroed(x, xn + 1);
        limb carry = a;
        for (std::size_t i = 0; i < xn; ++i) {
            auto w = detail::mul_wide(r[i], m);
            r[i] = w.lo + carry;
            carry = w.hi + (r[i] < w.lo);
        }
        r[xn] = carry;
        normalize(x, xn + 1);
    }
    /// |x| /= d, returning the remainder
    static limb divide_small(big_integer& x, limb d) noexcept {
        limb* r = x.data();
        limb remainder = 0;
        for (std::size_t i = x.size_; i-- > 0;)
            r[i] = detail::div_wide(remainder, r[i], d, remainder);
        normalize(x, x.size_);
        return remainder;
    }
};

}  // namespace autolib
