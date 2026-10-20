// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "autolib/interval_set.hpp"

#include <algorithm>
#include <cstring>
#include <ostream>
#include <sstream>
#include <vector>

namespace autolib {

interval_set::interval_set(interval i) noexcept : inline_{i} {
    size_ = i.min <= i.max ? 1 : 0;
}

interval_set::interval_set(const interval_set& other) : inline_{0, 0} {
    reserve(other.size_);
    std::copy_n(other.data(), other.size_, data());
    size_ = other.size_;
}

interval_set::interval_set(interval_set&& other) noexcept : size_(other.size_), capacity_(other.capacity_), inline_{0, 0} {
    if (capacity_ <= 1) {
        inline_ = other.inline_;
    } else {
        heap_ = other.heap_;
        other.capacity_ = 1;
    }
    other.size_ = 0;
}

interval_set& interval_set::operator=(const interval_set& other) {
    if (this != &other) {
        size_ = 0;
        reserve(other.size_);
        std::copy_n(other.data(), other.size_, data());
        size_ = other.size_;
    }
    return *this;
}

interval_set& interval_set::operator=(interval_set&& other) noexcept {
    if (this != &other) {
        if (capacity_ > 1)
            delete[] heap_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        if (capacity_ <= 1) {
            inline_ = other.inline_;
        } else {
            heap_ = other.heap_;
            other.capacity_ = 1;
        }
        other.size_ = 0;
    }
    return *this;
}

interval_set::~interval_set() {
    if (capacity_ > 1)
        delete[] heap_;
}

void interval_set::reserve(std::uint32_t capacity) {
    if (capacity <= capacity_)
        return;
    std::uint32_t grown = std::max(capacity, 2 * capacity_);
    auto* fresh = new interval[grown];
    if (capacity_ > 1) {
        std::copy_n(heap_, size_, fresh);
        delete[] heap_;
    } else if (size_ == 1) {
        fresh[0] = inline_;
    }
    heap_ = fresh;
    capacity_ = grown;
}

interval_set interval_set::of(std::initializer_list<symbol> symbols) {
    return of(std::span<const symbol>(symbols.begin(), symbols.size()));
}

interval_set interval_set::of(std::span<const symbol> symbols) {
    std::vector<symbol> sorted(symbols.begin(), symbols.end());
    std::ranges::sort(sorted);
    interval_set out;
    for (symbol s : sorted)
        out.append({s, s});
    return out;
}

std::uint64_t interval_set::cardinality() const noexcept {
    std::uint64_t total = 0;
    for (interval i : *this)
        total += i.size();
    return total;
}

bool interval_set::contains(symbol s) const noexcept {
    // First interval whose max is >= s
    const interval* it = std::partition_point(begin(), end(), [s](interval i) { return i.max < s; });
    return it != end() && it->min <= s;
}

bool interval_set::contains(const interval_set& other) const noexcept {
    const interval* it = begin();
    for (interval i : other) {
        it = std::partition_point(it, end(), [&](interval j) { return j.max < i.min; });
        if (it == end() || it->min > i.min || it->max < i.max)
            return false;
    }
    return true;
}

bool interval_set::intersects(const interval_set& other) const noexcept {
    const interval *a = begin(), *b = other.begin();
    while (a != end() && b != other.end()) {
        if (a->max < b->min)
            ++a;
        else if (b->max < a->min)
            ++b;
        else
            return true;
    }
    return false;
}

void interval_set::append(interval i) {
    if (size_ > 0) {
        interval& last = data()[size_ - 1];
        if (static_cast<std::int64_t>(last.max) + 1 >= i.min) {
            last.max = std::max(last.max, i.max);
            return;
        }
    }
    reserve(size_ + 1);
    data()[size_++] = i;
}

bool interval_set::insert(interval i) {
    if (i.min > i.max)
        return false;
    interval* first = std::partition_point(data(), data() + size_,
                                           [&](interval j) { return static_cast<std::int64_t>(j.max) + 1 < i.min; });
    interval* last = std::partition_point(first, data() + size_,
                                          [&](interval j) { return j.min <= static_cast<std::int64_t>(i.max) + 1; });
    auto at = static_cast<std::uint32_t>(first - data());
    auto nb = static_cast<std::uint32_t>(last - first);
    if (nb == 0) {
        reserve(size_ + 1);
        interval* d = data();
        std::move_backward(d + at, d + size_, d + size_ + 1);
        d[at] = i;
        ++size_;
        return true;
    }
    interval merged{std::min(i.min, first->min), std::max(i.max, (last - 1)->max)};
    bool changed = nb > 1 || merged != *first;
    *first = merged;
    if (nb > 1) {
        interval* d = data();
        std::move(d + at + nb, d + size_, d + at + 1);
        size_ -= nb - 1;
    }
    return changed;
}

bool interval_set::insert(const interval_set& other) {
    if (other.size_ <= 1)
        return other.size_ == 1 && insert(other.data()[0]);
    interval_set merged;
    merged.reserve(size_ + other.size_);
    const interval *a = begin(), *b = other.begin();
    while (a != end() || b != other.end()) {
        if (b == other.end() || (a != end() && a->min <= b->min))
            merged.append(*a++);
        else
            merged.append(*b++);
    }
    bool changed = merged != *this;
    *this = std::move(merged);
    return changed;
}

interval_set& interval_set::operator|=(const interval_set& other) {
    insert(other);
    return *this;
}

interval_set& interval_set::operator&=(const interval_set& other) {
    *this = intersection(other);
    return *this;
}

interval_set& interval_set::operator-=(const interval_set& other) {
    *this = difference(other);
    return *this;
}

interval_set interval_set::intersection(const interval_set& other) const {
    interval_set out;
    const interval *a = begin(), *b = other.begin();
    while (a != end() && b != other.end()) {
        symbol lo = std::max(a->min, b->min), hi = std::min(a->max, b->max);
        if (lo <= hi)
            out.append({lo, hi});
        if (a->max < b->max)
            ++a;
        else
            ++b;
    }
    return out;
}

interval_set interval_set::difference(const interval_set& other) const {
    interval_set out;
    const interval* b = other.begin();
    for (interval a : *this) {
        std::int64_t from = a.min;
        while (b != other.end() && b->max < from)
            ++b;
        for (const interval* c = b; c != other.end() && c->min <= a.max; ++c) {
            if (c->min > from)
                out.append({static_cast<symbol>(from), c->min - 1});
            from = static_cast<std::int64_t>(c->max) + 1;
        }
        if (from <= a.max)
            out.append({static_cast<symbol>(from), a.max});
    }
    return out;
}

interval_set interval_set::complement() const {
    interval_set out;
    std::int64_t next = min_symbol;
    for (interval i : *this) {
        if (i.min > next)
            out.append({static_cast<symbol>(next), i.min - 1});
        next = static_cast<std::int64_t>(i.max) + 1;
    }
    if (next <= max_symbol)
        out.append({static_cast<symbol>(next), max_symbol});
    return out;
}

std::size_t interval_set::hash() const noexcept {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (interval i : *this) {
        h = (h ^ static_cast<std::uint32_t>(i.min)) * 0x100000001b3ULL;
        h = (h ^ static_cast<std::uint32_t>(i.max)) * 0x100000001b3ULL;
    }
    return static_cast<std::size_t>(h);
}

namespace {
void print_symbol(std::ostream& out, symbol s) {
    if (s == min_symbol)
        out << "-inf";
    else if (s == max_symbol)
        out << "+inf";
    else
        out << s;
}
}  // namespace

std::ostream& interval_set::print(std::ostream& out, const interval_set& set) {
    out << '{';
    bool first = true;
    for (interval i : set) {
        if (!first)
            out << ", ";
        first = false;
        print_symbol(out, i.min);
        if (i.max != i.min) {
            out << "..";
            print_symbol(out, i.max);
        }
    }
    return out << '}';
}

std::string interval_set::to_string() const {
    std::ostringstream out;
    out << *this;
    return out.str();
}

}  // namespace autolib
