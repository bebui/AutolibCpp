// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "autolib/interval_set.hpp"

namespace autolib::detail {

/// Partition of the symbols used by a collection of sets into classes : two symbols are in the same class
/// when every set contains both or neither. Algorithms then work on class numbers instead of intervals.
/// Only the symbols belonging to at least one set get a class; classes are numbered in increasing order.
class symbol_classes {
public:
    symbol_classes() = default;

    /// Calls `each(f)` with a function `f` to call on every set.
    template <class ForEachSet>
    explicit symbol_classes(ForEachSet&& each) {
        // Distinct cuts gathered in a hash set first : collections repeat the same intervals a lot
        std::vector<std::int64_t> cuts;
        cut_set seen;
        each([&](const interval_set& set) {
            for (interval i : set) {
                if (seen.insert(i.min))
                    cuts.push_back(i.min);
                if (seen.insert(static_cast<std::int64_t>(i.max) + 1))
                    cuts.push_back(static_cast<std::int64_t>(i.max) + 1);
            }
        });
        std::ranges::sort(cuts);
        if (cuts.size() < 2)
            return;
        // Number of intervals starting minus ending at each cut, then covering depth of each segment
        std::vector<std::int32_t> cover(cuts.size(), 0);
        auto at = [&](std::int64_t v) { return std::ranges::lower_bound(cuts, v) - cuts.begin(); };
        each([&](const interval_set& set) {
            for (interval i : set) {
                ++cover[static_cast<std::size_t>(at(i.min))];
                --cover[static_cast<std::size_t>(at(static_cast<std::int64_t>(i.max) + 1))];
            }
        });
        std::int32_t depth = 0;
        for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
            depth += cover[i];
            if (depth > 0) {
                mins_.push_back(static_cast<symbol>(cuts[i]));
                maxs_.push_back(static_cast<symbol>(cuts[i + 1] - 1));
            }
        }
    }

    [[nodiscard]] std::int32_t size() const noexcept { return static_cast<std::int32_t>(mins_.size()); }
    [[nodiscard]] symbol min(std::int32_t c) const noexcept { return mins_[static_cast<std::size_t>(c)]; }
    [[nodiscard]] symbol max(std::int32_t c) const noexcept { return maxs_[static_cast<std::size_t>(c)]; }

    /// Calls `f(c)` for every class of a set, in increasing order. The set must be one of the collection.
    template <class F>
    void for_each_class(const interval_set& set, F&& f) const {
        for (interval i : set) {
            auto first = first_class_from(i.min);
            auto last = first_class_from(static_cast<std::int64_t>(i.max) + 1);
            for (auto c = first; c < last; ++c)
                f(c);
        }
    }

    /// Adds the symbols of a class at the end of a set.
    void append_to(interval_set& set, std::int32_t c) const {
        set.append({mins_[static_cast<std::size_t>(c)], maxs_[static_cast<std::size_t>(c)]});
    }

private:
    /// Open addressing set of 64-bit values
    class cut_set {
    public:
        bool insert(std::int64_t v) {
            if (2 * (size_ + 1) > slots_.size())
                grow();
            auto mask = slots_.size() - 1;
            for (auto i = mix(v) & mask;; i = (i + 1) & mask) {
                if (!used_[i]) {
                    used_[i] = 1;
                    slots_[i] = v;
                    ++size_;
                    return true;
                }
                if (slots_[i] == v)
                    return false;
            }
        }

    private:
        static std::size_t mix(std::int64_t v) noexcept {
            auto x = static_cast<std::uint64_t>(v) * 0x9e3779b97f4a7c15ULL;
            return static_cast<std::size_t>(x ^ (x >> 29));
        }
        void grow() {
            std::vector<std::int64_t> old_slots(std::max<std::size_t>(16, 2 * slots_.size()));
            std::vector<char> old_used(old_slots.size(), 0);
            old_slots.swap(slots_);
            old_used.swap(used_);
            size_ = 0;
            for (std::size_t i = 0; i < old_slots.size(); ++i)
                if (old_used[i])
                    insert(old_slots[i]);
        }
        std::vector<std::int64_t> slots_;
        std::vector<char> used_;
        std::size_t size_ = 0;
    };

    [[nodiscard]] std::int32_t first_class_from(std::int64_t v) const noexcept {
        return static_cast<std::int32_t>(std::ranges::partition_point(mins_, [v](symbol m) { return m < v; }) - mins_.begin());
    }

    std::vector<symbol> mins_;
    std::vector<symbol> maxs_;
};

}  // namespace autolib::detail
