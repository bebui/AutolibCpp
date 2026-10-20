// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace autolib::detail {

/// Refinable partition of {0, ..., n-1} (Valmari and Lehtinen) : elements are marked, then split()
/// separates the marked elements of every touched set, the smaller part getting a new set number.
/// The data read together are stored together, the refinement being bound by memory accesses.
class refinable_partition {
public:
    explicit refinable_partition(std::int32_t n)
        : elems_(u(n)), info_(u(n)), sets_(u(n) + 1, set{0, 0, 0, 0}) {
        for (std::int32_t i = 0; i < n; ++i) {
            elems_[u(i)] = i;
            info_[u(i)] = {i, 0};
        }
        count_ = n > 0 ? 1 : 0;
        sets_[0].past = n;
        touched_.reserve(u(n) + 1);
    }

    /// Partition into the groups order[bounds[g] .. bounds[g+1]-1], empty groups being skipped.
    refinable_partition(std::span<const std::int32_t> order, std::span<const std::int32_t> bounds)
        : refinable_partition(static_cast<std::int32_t>(order.size())) {
        std::ranges::copy(order, elems_.begin());
        count_ = 0;
        for (std::size_t g = 0; g + 1 < bounds.size(); ++g) {
            if (bounds[g] == bounds[g + 1])
                continue;
            sets_[u(count_)] = {bounds[g], bounds[g + 1], 0, 0};
            for (auto i = bounds[g]; i < bounds[g + 1]; ++i)
                info_[u(elems_[u(i)])] = {i, count_};
            ++count_;
        }
    }

    void mark(std::int32_t e) {
        auto [i, s] = info_[u(e)];
        set& x = sets_[u(s)];
        auto j = x.first + x.marked;
        if (i < j)
            return;
        auto other = elems_[u(j)];
        elems_[u(i)] = other;
        info_[u(other)].loc = i;
        elems_[u(j)] = e;
        info_[u(e)].loc = j;
        if (x.marked++ == 0)
            touched_.push_back(s);
    }

    void split() {
        while (!touched_.empty()) {
            auto s = touched_.back();
            touched_.pop_back();
            set& x = sets_[u(s)];
            auto j = x.first + x.marked;
            if (j == x.past) {
                x.marked = 0;
                continue;
            }
            set& fresh = sets_[u(count_)];
            if (x.marked <= x.past - j) {
                fresh.first = x.first;
                fresh.past = x.first = j;
            } else {
                fresh.past = x.past;
                fresh.first = x.past = j;
            }
            for (auto i = fresh.first; i < fresh.past; ++i)
                info_[u(elems_[u(i)])].set = count_;
            fresh.parent = s;
            x.marked = fresh.marked = 0;
            ++count_;
        }
    }

    [[nodiscard]] std::int32_t count() const noexcept { return count_; }
    [[nodiscard]] std::int32_t set_of(std::int32_t e) const noexcept { return info_[u(e)].set; }
    [[nodiscard]] std::int32_t first(std::int32_t s) const noexcept { return sets_[u(s)].first; }
    [[nodiscard]] std::int32_t past(std::int32_t s) const noexcept { return sets_[u(s)].past; }
    [[nodiscard]] std::int32_t size(std::int32_t s) const noexcept { return sets_[u(s)].past - sets_[u(s)].first; }
    [[nodiscard]] std::int32_t element(std::int32_t i) const noexcept { return elems_[u(i)]; }
    /// The set that a set was split from.
    [[nodiscard]] std::int32_t parent(std::int32_t s) const noexcept { return sets_[u(s)].parent; }
    /// The set of every element.
    [[nodiscard]] std::vector<std::int32_t> sets() const {
        std::vector<std::int32_t> out(info_.size());
        for (std::size_t e = 0; e < info_.size(); ++e)
            out[e] = info_[e].set;
        return out;
    }

private:
    static std::size_t u(std::int32_t i) noexcept { return static_cast<std::size_t>(i); }

    struct element_info {
        std::int32_t loc;  ///< position in elems_
        std::int32_t set;
    };
    struct set {
        std::int32_t first;   ///< first position in elems_
        std::int32_t past;    ///< position following the last one
        std::int32_t marked;  ///< number of marked elements, stored first
        std::int32_t parent;
    };

    std::int32_t count_ = 0;
    std::vector<std::int32_t> elems_;
    std::vector<element_info> info_;
    std::vector<set> sets_;
    std::vector<std::int32_t> touched_;
};

}  // namespace autolib::detail
