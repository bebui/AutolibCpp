// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "autolib/big_integer.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <ostream>
#include <stdexcept>
#include <vector>

#include "detail/big_integer_access.hpp"
#include "detail/wide.hpp"

namespace autolib {

using limb = big_integer::limb;
using detail::add_carry;
using detail::div_wide;
using detail::mul_wide;
using detail::sub_borrow;

namespace {

using access = big_integer_access;

/// r[0 .. an+bn) = a * b, r being distinct from a and b
void multiply(limb* r, const limb* a, std::size_t an, const limb* b, std::size_t bn) noexcept {
    std::fill(r, r + an + bn, limb{0});
    for (std::size_t i = 0; i < an; ++i) {
        limb carry = 0;
        limb ai = a[i];
        for (std::size_t j = 0; j < bn; ++j) {
            auto w = mul_wide(ai, b[j]);
            limb lo = w.lo + carry;
            limb hi = w.hi + (lo < w.lo);
            r[i + j] += lo;
            carry = hi + (r[i + j] < lo);
        }
        r[i + bn] = carry;
    }
}

/// Knuth's algorithm D : q = u / v and r = u % v, with un >= vn >= 2 and the top limb of v nonzero.
/// q has un - vn + 1 limbs and r has vn limbs.
void divide(const limb* u, std::size_t un, const limb* v, std::size_t vn, limb* q, limb* r) {
    int s = std::countl_zero(v[vn - 1]);
    auto shift_left = [s](const limb* from, std::size_t n, limb* to) {
        if (s == 0) {
            std::copy_n(from, n, to);
            return limb{0};
        }
        limb carry = 0;
        for (std::size_t i = 0; i < n; ++i) {
            to[i] = (from[i] << s) | carry;
            carry = from[i] >> (64 - s);
        }
        return carry;
    };
    std::vector<limb> nv(vn), nu(un + 1);
    shift_left(v, vn, nv.data());
    nu[un] = shift_left(u, un, nu.data());
    limb top = nv[vn - 1], second = nv[vn - 2];
    for (std::size_t j = un - vn + 1; j-- > 0;) {
        // Estimate of the quotient digit, too large by at most 2
        limb qhat, rhat;
        bool rhat_overflow;
        if (nu[j + vn] >= top) {
            qhat = ~limb{0};
            rhat = nu[j + vn - 1] + top;
            rhat_overflow = rhat < top;
        } else {
            qhat = div_wide(nu[j + vn], nu[j + vn - 1], top, rhat);
            rhat_overflow = false;
        }
        while (!rhat_overflow) {
            auto w = mul_wide(qhat, second);
            if (w.hi < rhat || (w.hi == rhat && w.lo <= nu[j + vn - 2]))
                break;
            --qhat;
            rhat += top;
            rhat_overflow = rhat < top;
        }
        // nu[j .. j+vn] -= qhat * nv
        limb mul_carry = 0, borrow = 0;
        for (std::size_t i = 0; i < vn; ++i) {
            auto w = mul_wide(qhat, nv[i]);
            limb lo = w.lo + mul_carry;
            mul_carry = w.hi + (lo < w.lo);
            nu[i + j] = sub_borrow(nu[i + j], lo, borrow);
        }
        nu[j + vn] = sub_borrow(nu[j + vn], mul_carry, borrow);
        if (borrow != 0) {
            // The estimate was one too large : add back
            --qhat;
            limb carry = 0;
            for (std::size_t i = 0; i < vn; ++i)
                nu[i + j] = add_carry(nu[i + j], nv[i], carry);
            nu[j + vn] += carry;
        }
        q[j] = qhat;
    }
    for (std::size_t i = 0; i < vn; ++i)
        r[i] = s == 0 ? nu[i] : (nu[i] >> s) | (nu[i + 1] << (64 - s));
}

constexpr limb ten_19 = 10'000'000'000'000'000'000ULL;

int digit_value(char c) noexcept {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return 99;
}

}  // namespace

big_integer::big_integer(std::string_view decimal) : big_integer(parse(decimal, 10)) {}

big_integer big_integer::parse(std::string_view text, int base) {
    if (base < 2 || base > 36)
        throw std::invalid_argument("Invalid base: " + std::to_string(base));
    std::string_view digits = text;
    bool negative = false;
    if (!digits.empty() && (digits[0] == '+' || digits[0] == '-')) {
        negative = digits[0] == '-';
        digits.remove_prefix(1);
    }
    if (digits.empty())
        throw std::invalid_argument("Not an integer: \"" + std::string(text) + "\"");
    // Digits are read by chunks fitting in a limb
    int chunk_digits = 0;
    limb chunk_base = 1;
    while (chunk_base <= std::numeric_limits<limb>::max() / static_cast<limb>(base)) {
        chunk_base *= static_cast<limb>(base);
        ++chunk_digits;
    }
    big_integer x;
    limb chunk = 0, scale = 1;
    int in_chunk = 0;
    for (char c : digits) {
        int d = digit_value(c);
        if (d >= base)
            throw std::invalid_argument("Not an integer: \"" + std::string(text) + "\"");
        chunk = chunk * static_cast<limb>(base) + static_cast<limb>(d);
        scale *= static_cast<limb>(base);
        if (++in_chunk == chunk_digits) {
            access::multiply_add(x, scale, chunk);
            chunk = 0;
            scale = 1;
            in_chunk = 0;
        }
    }
    if (in_chunk > 0)
        access::multiply_add(x, scale, chunk);
    access::set_negative(x, negative);
    return x;
}

big_integer big_integer::from_magnitude(std::span<const limb> limbs, bool negative) {
    big_integer x;
    std::copy(limbs.begin(), limbs.end(), access::reserve(x, limbs.size()));
    access::normalize(x, limbs.size());
    access::set_negative(x, negative);
    return x;
}

limb* big_integer::reserve_limbs(std::size_t n) {
    return access::reserve(*this, n);
}

void big_integer::set_magnitude_size(std::size_t n) noexcept {
    access::normalize(*this, n);
}

big_integer big_integer::from_double(double value) {
    if (!std::isfinite(value))
        throw std::invalid_argument("Not a finite double");
    int exponent;
    double fraction = std::frexp(std::trunc(value), &exponent);  // |value| = fraction * 2^exponent, fraction in [0.5, 1)
    if (exponent <= 0)
        return {};
    auto mantissa = static_cast<limb>(std::ldexp(std::fabs(fraction), 53));
    big_integer x(mantissa);
    if (exponent >= 53)
        x <<= static_cast<std::size_t>(exponent - 53);
    else
        x >>= static_cast<std::size_t>(53 - exponent);
    access::set_negative(x, value < 0);
    return x;
}

void big_integer::copy_heap(const big_integer& other) {
    heap_ = new limb[other.size_];
    capacity_ = other.size_;
    std::copy_n(other.heap_, other.size_, heap_);
}

void big_integer::assign_heap(const big_integer& other) {
    if (other.size_ > capacity_) {
        big_integer copy(other);
        *this = std::move(copy);
        return;
    }
    std::copy_n(other.data(), other.size_, data());
    size_ = other.size_;
    negative_ = other.negative_;
}

void big_integer::free_heap() noexcept {
    delete[] heap_;
}

int big_integer::compare_magnitudes(const big_integer& a, const big_integer& b) noexcept {
    return access::compare(a.data(), a.size_, b.data(), b.size_);
}

std::size_t big_integer::bit_length() const noexcept {
    if (size_ == 0)
        return 0;
    return std::size_t{size_} * 64 - static_cast<std::size_t>(std::countl_zero(data()[size_ - 1]));
}

double big_integer::to_double() const noexcept {
    if (size_ == 0)
        return 0.0;
    double magnitude;
    if (size_ == 1) {
        magnitude = static_cast<double>(data()[0]);
    } else {
        // The 64 leading bits, the lowest one also standing for the bits below : rounding them to 53 bits then
        // gives the correctly rounded value
        const limb* m = data();
        std::size_t shift = bit_length() - 64;
        std::size_t i = shift / 64;
        auto offset = static_cast<int>(shift % 64);
        limb top = offset == 0 ? m[i] : (m[i] >> offset) | (m[i + 1] << (64 - offset));
        bool sticky = offset != 0 && (m[i] & ((limb{1} << offset) - 1)) != 0;
        for (std::size_t j = 0; j < i && !sticky; ++j)
            sticky = m[j] != 0;
        if (sticky)
            top |= 1;
        magnitude = shift > 2000 ? HUGE_VAL : std::ldexp(static_cast<double>(top), static_cast<int>(shift));
    }
    return negative_ ? -magnitude : magnitude;
}

std::string big_integer::to_string() const {
    if (size_ <= 1) {
        std::string s = std::to_string(small_value());
        return negative_ ? "-" + s : s;
    }
    // Chunks of 19 digits, the least significant first
    big_integer x = abs();
    std::vector<limb> chunks;
    chunks.reserve(size_ * 64 / 63 + 1);
    while (x.size_ > 0)
        chunks.push_back(access::divide_small(x, ten_19));
    std::string s = negative_ ? "-" : "";
    s += std::to_string(chunks.back());
    for (std::size_t i = chunks.size() - 1; i-- > 0;) {
        auto part = std::to_string(chunks[i]);
        s.append(19 - part.size(), '0');
        s += part;
    }
    return s;
}

std::ostream& operator<<(std::ostream& out, const big_integer& x) {
    return out << x.to_string();
}

std::size_t big_integer::hash_limbs() const noexcept {
    std::uint64_t h = negative_ ? 0x9e3779b97f4a7c15ULL : 0;
    for (limb x : magnitude()) {
        h ^= x + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    }
    return static_cast<std::size_t>(h);
}

big_integer& big_integer::add_slow(const big_integer& other, bool subtract) {
    if (this == &other) {
        if (subtract) {
            size_ = 0;
            negative_ = false;
            return *this;
        }
        return *this <<= 1;
    }
    access::add_signed(*this, other.data(), other.size_, other.negative_ != subtract);
    return *this;
}

big_integer& big_integer::add_product_slow(const big_integer& a, limb b) {
    if (a.size_ == 0 || b == 0)
        return *this;
    if (this == &a || (size_ != 0 && negative_ != a.negative_))
        return *this += a * big_integer(b);
    access::add_product(*this, a.data(), a.size_, b);
    negative_ = a.negative_;
    return *this;
}

big_integer& big_integer::operator*=(const big_integer& other) {
    bool negative = negative_ != other.negative_;
    if (size_ == 0 || other.size_ == 0) {
        size_ = 0;
        negative_ = false;
        return *this;
    }
    if (other.size_ == 1 && this != &other) {
        access::multiply_add(*this, other.data()[0], 0);
    } else if (size_ == 1) {
        limb m = data()[0];
        big_integer copy(other);
        access::multiply_add(copy, m, 0);
        *this = std::move(copy);
    } else {
        std::vector<limb> product(std::size_t{size_} + other.size_);
        multiply(product.data(), data(), size_, other.data(), other.size_);
        limb* r = access::reserve(*this, product.size());
        std::copy(product.begin(), product.end(), r);
        access::normalize(*this, product.size());
    }
    access::set_negative(*this, negative);
    return *this;
}

std::pair<big_integer, big_integer> big_integer::div_mod(const big_integer& a, const big_integer& b) {
    if (b.size_ == 0)
        throw std::domain_error("Division by zero");
    if (compare_magnitudes(a, b) < 0)
        return {big_integer{}, a};
    big_integer q, r;
    if (b.size_ == 1) {
        q = a;
        r = big_integer(access::divide_small(q, b.data()[0]));
    } else {
        std::size_t qn = a.size_ - b.size_ + 1;
        limb* qd = access::reserve(q, qn);
        limb* rd = access::reserve(r, b.size_);
        divide(a.data(), a.size_, b.data(), b.size_, qd, rd);
        access::normalize(q, qn);
        access::normalize(r, b.size_);
    }
    access::set_negative(q, a.negative_ != b.negative_);
    access::set_negative(r, a.negative_);
    return {std::move(q), std::move(r)};
}

big_integer& big_integer::operator/=(const big_integer& other) {
    if (other.size_ == 1 && this != &other) {
        bool negative = negative_ != other.negative_;
        access::divide_small(*this, other.data()[0]);
        access::set_negative(*this, negative);
        return *this;
    }
    return *this = div_mod(*this, other).first;
}

big_integer& big_integer::operator%=(const big_integer& other) {
    if (other.size_ == 1 && this != &other) {
        bool negative = negative_;
        limb rem = access::divide_small(*this, other.data()[0]);
        *this = rem;
        access::set_negative(*this, negative);
        return *this;
    }
    return *this = div_mod(*this, other).second;
}

big_integer& big_integer::operator<<=(std::size_t bits) {
    if (size_ == 0 || bits == 0)
        return *this;
    std::size_t limbs = bits / 64, n = size_;
    auto s = static_cast<int>(bits % 64);
    limb* r = access::reserve_zeroed(*this, n + limbs + 1);
    r[n + limbs] = 0;
    for (std::size_t i = n; i-- > 0;) {
        limb x = r[i];
        if (s != 0)
            r[i + limbs + 1] |= x >> (64 - s);
        r[i + limbs] = x << s;
    }
    std::fill(r, r + limbs, limb{0});
    access::normalize(*this, n + limbs + 1);
    return *this;
}

big_integer& big_integer::operator>>=(std::size_t bits) {
    if (size_ == 0 || bits == 0)
        return *this;
    std::size_t limbs = bits / 64, n = size_;
    auto s = static_cast<int>(bits % 64);
    limb* r = data();
    bool negative = negative_;
    // Rounding toward negative infinity : a negative value whose shifted out bits are not all zero gets 1 more
    bool inexact = false;
    if (negative_) {
        for (std::size_t i = 0; i < std::min(limbs, n) && !inexact; ++i)
            inexact = r[i] != 0;
        if (!inexact && limbs < n && s != 0)
            inexact = (r[limbs] & ((limb{1} << s) - 1)) != 0;
    }
    if (limbs >= n) {
        size_ = 0;
    } else {
        for (std::size_t i = limbs; i < n; ++i) {
            limb x = r[i] >> s;
            if (s != 0 && i + 1 < n)
                x |= r[i + 1] << (64 - s);
            r[i - limbs] = x;
        }
        size_ = static_cast<std::uint32_t>(n - limbs);
        access::normalize(*this, size_);
    }
    if (size_ == 0)
        negative_ = false;
    if (inexact) {
        limb one = 1;
        access::add_magnitude(*this, &one, 1);
        negative_ = negative;
    }
    return *this;
}

big_integer big_integer::pow(big_integer base, unsigned exponent) {
    big_integer result(1);
    while (exponent != 0) {
        if (exponent & 1U)
            result *= base;
        exponent >>= 1;
        if (exponent != 0)
            base *= base;
    }
    return result;
}

namespace literals {
big_integer operator""_big(const char* literal) {
    std::string digits;
    for (const char* c = literal; *c != '\0'; ++c)
        if (*c != '\'')
            digits += *c;
    std::string_view text = digits;
    int base = 10;
    if (text.size() > 1 && text[0] == '0') {
        if (text[1] == 'x' || text[1] == 'X')
            base = 16;
        else if (text[1] == 'b' || text[1] == 'B')
            base = 2;
        else
            base = 8;
        text.remove_prefix(base == 8 ? 1 : 2);
    }
    return big_integer::parse(text, base);
}
}  // namespace literals

}  // namespace autolib
