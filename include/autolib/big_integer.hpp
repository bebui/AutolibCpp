// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Signed integers of arbitrary size.

#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "autolib/export.hpp"

namespace autolib {

/// A signed integer of arbitrary size, with the arithmetic operators of the built-in integers.
///
/// The magnitude is stored as 64-bit limbs, the least significant first. A value fitting in 64 bits is stored
/// inline, without heap allocation, and the operations on such values take a fast path.
///
/// ```cpp
/// big_integer x = 1;
/// x <<= 100;                                  // 2^100
/// auto y = x * x - 1;
/// std::string s = (y % 1000).to_string();     // "375"
/// auto z = "123456789012345678901234567890"_big;
/// ```
///
/// Division truncates toward zero and the remainder takes the sign of the dividend, as for the built-in
/// integers; the right shift rounds toward negative infinity.
class AUTOLIB_EXPORT big_integer {
public:
    using limb = std::uint64_t;

    /// Zero.
    big_integer() noexcept = default;
    template <std::integral T>
        requires(!std::same_as<T, bool>)
    big_integer(T value) noexcept {  // NOLINT(google-explicit-constructor) : integers convert implicitly
        if constexpr (std::is_signed_v<T>) {
            negative_ = value < 0;
            // Negated in unsigned arithmetic, which also handles the minimum value
            small_ = negative_ ? limb{0} - static_cast<limb>(value) : static_cast<limb>(value);
        } else {
            small_ = static_cast<limb>(value);
        }
        size_ = small_ != 0 ? 1 : 0;
    }
    /// Parses a decimal integer, with an optional sign.
    /// \throws std::invalid_argument if the text is not a decimal integer
    explicit big_integer(std::string_view decimal);
    /// Parses an integer written in a base from 2 to 36, with an optional sign; letters stand for the digits
    /// beyond 9, in either case.
    /// \throws std::invalid_argument if the text is not an integer in that base
    [[nodiscard]] static big_integer parse(std::string_view text, int base = 10);
    /// The integer whose magnitude has the given limbs, the least significant first.
    [[nodiscard]] static big_integer from_magnitude(std::span<const limb> limbs, bool negative = false);
    /// The integer part of a finite double, truncated toward zero.
    /// \throws std::invalid_argument if the double is infinite or not a number
    [[nodiscard]] static big_integer from_double(double value);

    big_integer(const big_integer& other);
    big_integer(big_integer&& other) noexcept
        : size_(std::exchange(other.size_, 0)), capacity_(std::exchange(other.capacity_, 1)),
          negative_(std::exchange(other.negative_, false)) {
        if (capacity_ > 1)
            heap_ = other.heap_;
        else
            small_ = other.small_;
    }
    big_integer& operator=(const big_integer& other);
    big_integer& operator=(big_integer&& other) noexcept {
        if (this != &other) {
            release();
            size_ = std::exchange(other.size_, 0);
            capacity_ = std::exchange(other.capacity_, 1);
            negative_ = std::exchange(other.negative_, false);
            if (capacity_ > 1)
                heap_ = other.heap_;
            else
                small_ = other.small_;
        }
        return *this;
    }
    /// Assigns a built-in integer, keeping the storage already allocated.
    template <std::integral T>
        requires(!std::same_as<T, bool>)
    big_integer& operator=(T value) noexcept {
        big_integer small(value);
        data()[0] = small.small_;
        size_ = small.size_;
        negative_ = small.negative_;
        return *this;
    }
    ~big_integer() { release(); }

    /// -1, 0 or 1 according to the sign.
    [[nodiscard]] int signum() const noexcept { return size_ == 0 ? 0 : negative_ ? -1 : 1; }
    [[nodiscard]] bool is_zero() const noexcept { return size_ == 0; }
    [[nodiscard]] bool is_negative() const noexcept { return negative_; }
    [[nodiscard]] explicit operator bool() const noexcept { return size_ != 0; }
    /// The number of bits of the magnitude : 0 for zero, and 1 + floor(log2 |x|) otherwise.
    [[nodiscard]] std::size_t bit_length() const noexcept;
    /// The limbs of the magnitude, the least significant first, without leading zero.
    [[nodiscard]] std::span<const limb> magnitude() const noexcept { return {data(), size_}; }
    /// The absolute value.
    [[nodiscard]] big_integer abs() const& {
        big_integer r(*this);
        r.negative_ = false;
        return r;
    }
    [[nodiscard]] big_integer abs() && {
        negative_ = false;
        return std::move(*this);
    }

    /// The value as a built-in integer, if it fits.
    template <std::integral T>
    [[nodiscard]] std::optional<T> to() const noexcept {
        if (size_ == 0)
            return T{0};
        if (size_ > 1)
            return std::nullopt;
        limb m = small_value();
        if constexpr (std::is_signed_v<T>) {
            using U = std::make_unsigned_t<T>;
            limb bound = negative_ ? limb{static_cast<U>(std::numeric_limits<T>::max())} + 1
                                   : limb{static_cast<U>(std::numeric_limits<T>::max())};
            if (m > bound)
                return std::nullopt;
            return negative_ ? static_cast<T>(-static_cast<T>(m - 1) - 1) : static_cast<T>(m);
        } else {
            if (negative_ || m > std::numeric_limits<T>::max())
                return std::nullopt;
            return static_cast<T>(m);
        }
    }
    /// The nearest double, ties to even; infinite when the magnitude exceeds the largest double.
    [[nodiscard]] double to_double() const noexcept;
    /// The decimal representation.
    [[nodiscard]] std::string to_string() const;

    // Arithmetic
    big_integer& operator+=(const big_integer& other);
    big_integer& operator-=(const big_integer& other);
    big_integer& operator*=(const big_integer& other);
    big_integer& operator/=(const big_integer& other);
    big_integer& operator%=(const big_integer& other);
    big_integer& operator<<=(std::size_t bits);
    big_integer& operator>>=(std::size_t bits);
    big_integer& operator++() { return *this += 1; }
    big_integer& operator--() { return *this -= 1; }
    big_integer operator++(int) {
        big_integer old(*this);
        ++*this;
        return old;
    }
    big_integer operator--(int) {
        big_integer old(*this);
        --*this;
        return old;
    }
    /// Adds `a * b` to this integer, without building the product : the accumulation of dynamic programming.
    big_integer& add_product(const big_integer& a, limb b) {
        if (size_ <= 1 && a.size_ <= 1 && !negative_ && !a.negative_ && fast_add_product(a.small_value(), b))
            return *this;
        return add_product_slow(a, b);
    }
    /// Computes the quotient and the remainder together.
    /// \throws std::domain_error on a division by zero
    [[nodiscard]] static std::pair<big_integer, big_integer> div_mod(const big_integer& a, const big_integer& b);
    /// A uniform random integer in [0, bound), drawn by rejection : each attempt succeeds with probability
    /// above 1/2.
    /// \throws std::domain_error if bound is not positive
    template <std::uniform_random_bit_generator G>
    [[nodiscard]] static big_integer uniform(const big_integer& bound, G& g) {
        if (bound.signum() <= 0)
            throw std::domain_error("Bound not positive");
        auto next = [&g] {
            if constexpr (G::min() == 0 && G::max() == std::numeric_limits<limb>::max())
                return static_cast<limb>(g());
            else
                return std::uniform_int_distribution<limb>{}(g);
        };
        auto top_bits = bound.bit_length() % 64;
        limb top_mask = top_bits == 0 ? ~limb{0} : (limb{1} << top_bits) - 1;
        if (bound.size_ == 1) {
            limb b = bound.small_value();
            for (;;)
                if (limb x = next() & top_mask; x < b)
                    return big_integer(x);
        }
        big_integer x;
        limb* r = x.reserve_limbs(bound.size_);
        for (;;) {
            for (std::uint32_t i = 0; i < bound.size_; ++i)
                r[i] = next();
            r[bound.size_ - 1] &= top_mask;
            x.set_magnitude_size(bound.size_);
            if (compare_magnitudes(x, bound) < 0)
                return x;
        }
    }
    /// `base` raised to the power `exponent`.
    [[nodiscard]] static big_integer pow(big_integer base, unsigned exponent);

    [[nodiscard]] big_integer operator-() const& {
        big_integer r(*this);
        r.negate();
        return r;
    }
    [[nodiscard]] big_integer operator-() && {
        negate();
        return std::move(*this);
    }
    [[nodiscard]] big_integer operator+() const& { return *this; }
    [[nodiscard]] big_integer operator+() && { return std::move(*this); }

    // Binary operators reuse the storage of a temporary left operand
    [[nodiscard]] friend big_integer operator+(big_integer a, const big_integer& b) { return std::move(a += b); }
    [[nodiscard]] friend big_integer operator-(big_integer a, const big_integer& b) { return std::move(a -= b); }
    [[nodiscard]] friend big_integer operator*(big_integer a, const big_integer& b) { return std::move(a *= b); }
    [[nodiscard]] friend big_integer operator/(big_integer a, const big_integer& b) { return std::move(a /= b); }
    [[nodiscard]] friend big_integer operator%(big_integer a, const big_integer& b) { return std::move(a %= b); }
    [[nodiscard]] friend big_integer operator<<(big_integer a, std::size_t bits) { return std::move(a <<= bits); }
    [[nodiscard]] friend big_integer operator>>(big_integer a, std::size_t bits) { return std::move(a >>= bits); }

    [[nodiscard]] friend bool operator==(const big_integer& a, const big_integer& b) noexcept {
        return a.negative_ == b.negative_ && compare_magnitudes(a, b) == 0;
    }
    [[nodiscard]] friend std::strong_ordering operator<=>(const big_integer& a, const big_integer& b) noexcept {
        if (a.negative_ != b.negative_)
            return a.negative_ ? std::strong_ordering::less : std::strong_ordering::greater;
        int c = compare_magnitudes(a, b);
        if (a.negative_)
            c = -c;
        return c < 0 ? std::strong_ordering::less : c > 0 ? std::strong_ordering::greater : std::strong_ordering::equal;
    }

    AUTOLIB_EXPORT friend std::ostream& operator<<(std::ostream& out, const big_integer& x);

    [[nodiscard]] std::size_t hash() const noexcept;

private:
    [[nodiscard]] const limb* data() const noexcept { return capacity_ > 1 ? heap_ : &small_; }
    [[nodiscard]] limb* data() noexcept { return capacity_ > 1 ? heap_ : &small_; }
    /// The magnitude of a value of at most one limb
    [[nodiscard]] limb small_value() const noexcept { return size_ == 0 ? 0 : data()[0]; }
    void release() noexcept;
    void negate() noexcept { negative_ = size_ != 0 && !negative_; }
    [[nodiscard]] static int compare_magnitudes(const big_integer& a, const big_integer& b) noexcept;

    limb* reserve_limbs(std::size_t n);
    void set_magnitude_size(std::size_t n) noexcept;
    bool fast_add_product(limb a, limb b) noexcept;
    big_integer& add_product_slow(const big_integer& a, limb b);

    friend struct big_integer_access;

    union {
        limb small_ = 0;  ///< the magnitude when capacity_ is 1
        limb* heap_;
    };
    std::uint32_t size_ = 0;      ///< number of limbs, the most significant one being nonzero
    std::uint32_t capacity_ = 1;  ///< 1 for the inline limb
    bool negative_ = false;       ///< never true for zero
};

inline bool big_integer::fast_add_product(limb a, limb b) noexcept {
    limb product, sum;
#if defined(__GNUC__) || defined(__clang__)
    if (__builtin_mul_overflow(a, b, &product) || __builtin_add_overflow(small_value(), product, &sum))
        return false;
#else
    if (a != 0 && b > std::numeric_limits<limb>::max() / a)
        return false;
    product = a * b;
    sum = small_value() + product;
    if (sum < product)
        return false;
#endif
    if (sum != 0) {
        data()[0] = sum;
        size_ = 1;
    }
    return true;
}

namespace literals {
/// An integer literal of any size : `123456789012345678901234567890_big`, `0xffff'ffff'ffff'ffff'ffff_big`.
/// Decimal, hexadecimal, binary and octal literals are accepted, with digit separators.
[[nodiscard]] AUTOLIB_EXPORT big_integer operator""_big(const char* literal);
}  // namespace literals

}  // namespace autolib

template <>
struct std::hash<autolib::big_integer> {
    std::size_t operator()(const autolib::big_integer& x) const noexcept { return x.hash(); }
};
