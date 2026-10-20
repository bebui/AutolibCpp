// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Symbols, intervals of symbols and sets of symbols.

#pragma once

#include <algorithm>
#include <compare>
#include <cstdint>
#include <initializer_list>
#include <iosfwd>
#include <limits>
#include <span>
#include <string>

#include "autolib/export.hpp"

namespace autolib {

/// A symbol of the alphabet. Every 32-bit signed integer is a symbol.
using symbol = std::int32_t;

/// The smallest symbol.
inline constexpr symbol min_symbol = std::numeric_limits<symbol>::min();
/// The largest symbol.
inline constexpr symbol max_symbol = std::numeric_limits<symbol>::max();

/// A non-empty closed interval of symbols `[min, max]`.
struct interval {
    symbol min;
    symbol max;

    /// Checks whether `s` belongs to the interval.
    [[nodiscard]] constexpr bool contains(symbol s) const noexcept { return min <= s && s <= max; }
    /// Number of symbols in the interval (up to 2^32, hence 64 bits).
    [[nodiscard]] constexpr std::uint64_t size() const noexcept {
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(max) - min + 1);
    }

    friend constexpr bool operator==(interval, interval) noexcept = default;
    friend constexpr auto operator<=>(interval, interval) noexcept = default;
};

/// A set of symbols, stored as sorted, disjoint and non-adjacent intervals.
///
/// Sets holding a single interval, by far the most common case on automaton transitions,
/// are stored inline without any heap allocation.
///
/// ```cpp
/// auto digits = interval_set::range(0, 9);
/// auto odd = interval_set::of({1, 3, 5, 7, 9});
/// auto even = digits - odd;            // {0, 2, 4, 6, 8}
/// bool b = (digits & ~odd).contains(4);
/// ```
class AUTOLIB_EXPORT interval_set {
public:
    /// The empty set.
    interval_set() noexcept : inline_{0, 0} {}
    /// The set of the symbols of an interval; empty if `i.min > i.max`.
    interval_set(interval i) noexcept;  // NOLINT(google-explicit-constructor): a set of symbols is naturally built from an interval
    /// The singleton `{s}`.
    interval_set(symbol s) noexcept : interval_set(interval{s, s}) {}  // NOLINT(google-explicit-constructor)

    interval_set(const interval_set& other);
    interval_set(interval_set&& other) noexcept;
    interval_set& operator=(const interval_set& other);
    interval_set& operator=(interval_set&& other) noexcept;
    ~interval_set();

    /// The symbols from `min` to `max`; empty if `min > max`.
    [[nodiscard]] static interval_set range(symbol min, symbol max) noexcept { return interval{min, max}; }
    /// Every symbol.
    [[nodiscard]] static interval_set all() noexcept { return interval{min_symbol, max_symbol}; }
    /// The set of the given symbols, in any order, duplicates allowed.
    [[nodiscard]] static interval_set of(std::initializer_list<symbol> symbols);
    /// The set of the given symbols, in any order, duplicates allowed.
    [[nodiscard]] static interval_set of(std::span<const symbol> symbols);

    /// The intervals, in increasing order.
    [[nodiscard]] std::span<const interval> intervals() const noexcept { return {data(), size_}; }
    [[nodiscard]] const interval* begin() const noexcept { return data(); }
    [[nodiscard]] const interval* end() const noexcept { return data() + size_; }
    /// Number of intervals.
    [[nodiscard]] std::size_t interval_count() const noexcept { return size_; }
    /// Number of symbols (up to 2^32).
    [[nodiscard]] std::uint64_t cardinality() const noexcept;
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    /// Smallest symbol. Requires a non-empty set.
    [[nodiscard]] symbol min() const noexcept { return data()[0].min; }
    /// Largest symbol. Requires a non-empty set.
    [[nodiscard]] symbol max() const noexcept { return data()[size_ - 1].max; }

    /// Checks whether `s` belongs to the set, in O(log n).
    [[nodiscard]] bool contains(symbol s) const noexcept;
    /// Checks whether every symbol of `other` belongs to the set.
    [[nodiscard]] bool contains(const interval_set& other) const noexcept;
    /// Checks whether the sets share a symbol.
    [[nodiscard]] bool intersects(const interval_set& other) const noexcept;

    /// Adds the symbols of an interval; returns whether the set changed.
    bool insert(interval i);
    /// Adds a symbol; returns whether the set changed.
    bool insert(symbol s) { return insert(interval{s, s}); }
    /// Adds the symbols of another set; returns whether the set changed.
    bool insert(const interval_set& other);
    /// Adds an interval located after every symbol of the set, or adjacent to the last one, in O(1).
    void append(interval i);
    void clear() noexcept { size_ = 0; }

    interval_set& operator|=(const interval_set& other);
    interval_set& operator&=(const interval_set& other);
    interval_set& operator-=(const interval_set& other);

    /// Union.
    [[nodiscard]] friend interval_set operator|(interval_set a, const interval_set& b) { return a |= b; }
    /// Intersection.
    [[nodiscard]] friend interval_set operator&(const interval_set& a, const interval_set& b) { return a.intersection(b); }
    /// Difference.
    [[nodiscard]] friend interval_set operator-(const interval_set& a, const interval_set& b) { return a.difference(b); }
    /// Complement with respect to every symbol.
    [[nodiscard]] friend interval_set operator~(const interval_set& a) { return a.complement(); }

    [[nodiscard]] interval_set intersection(const interval_set& other) const;
    [[nodiscard]] interval_set difference(const interval_set& other) const;
    [[nodiscard]] interval_set complement() const;

    [[nodiscard]] friend bool operator==(const interval_set& a, const interval_set& b) noexcept {
        return std::ranges::equal(a.intervals(), b.intervals());
    }
    /// Lexicographic order on the sequences of intervals.
    [[nodiscard]] friend std::strong_ordering operator<=>(const interval_set& a, const interval_set& b) noexcept {
        auto n = std::min(a.size_, b.size_);
        for (std::uint32_t i = 0; i < n; ++i)
            if (auto c = a.data()[i] <=> b.data()[i]; c != 0)
                return c;
        return a.size_ <=> b.size_;
    }

    /// Hash consistent with equality.
    [[nodiscard]] std::size_t hash() const noexcept;
    /// Text such as `{0-9, 12}`.
    [[nodiscard]] std::string to_string() const;
    friend std::ostream& operator<<(std::ostream& out, const interval_set& set) { return print(out, set); }

private:
    static std::ostream& print(std::ostream& out, const interval_set& set);
    [[nodiscard]] interval* data() noexcept { return capacity_ <= 1 ? &inline_ : heap_; }
    [[nodiscard]] const interval* data() const noexcept { return capacity_ <= 1 ? &inline_ : heap_; }
    void reserve(std::uint32_t capacity);

    std::uint32_t size_ = 0;
    std::uint32_t capacity_ = 1;  ///< 1 : the single interval is stored inline
    union {
        interval inline_;
        interval* heap_;
    };
};

}  // namespace autolib

template <>
struct std::hash<autolib::interval_set> {
    std::size_t operator()(const autolib::interval_set& set) const noexcept { return set.hash(); }
};
