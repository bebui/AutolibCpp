// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Enumeration of the words of a language.

#pragma once

#include <iterator>
#include <memory>
#include <vector>

#include "autolib/automaton.hpp"
#include "autolib/compiled_dfa.hpp"
#include "autolib/export.hpp"

namespace autolib {

/// The words of a language up to a maximal length, in shortlex order : by increasing length, then
/// lexicographically. A lazy input range : words are computed one at a time, each in time proportional to its
/// length times the number of transitions per state, dead branches being pruned.
///
/// ```cpp
/// for (const word& w : words("(1|2)3?"_re, 2))
///     ...  // {1}, {2}, {1, 3}, {2, 3}
/// ```
class AUTOLIB_EXPORT word_range {
public:
    class AUTOLIB_EXPORT iterator {
    public:
        using value_type = word;
        using difference_type = std::ptrdiff_t;
        using reference = const word&;
        using pointer = const word*;
        using iterator_concept = std::input_iterator_tag;

        iterator() = default;
        [[nodiscard]] const word& operator*() const noexcept { return word_; }
        [[nodiscard]] const word* operator->() const noexcept { return &word_; }
        iterator& operator++() {
            advance(true);
            return *this;
        }
        void operator++(int) { advance(true); }
        [[nodiscard]] friend bool operator==(const iterator& it, std::default_sentinel_t) noexcept { return it.done_; }

    private:
        friend class word_range;
        struct shared;
        explicit iterator(std::shared_ptr<const shared> data);
        bool next_length();
        bool next_symbol();
        void advance(bool resume);

        std::shared_ptr<const shared> data_;
        bool done_ = true;
        int length_ = -1;
        int depth_ = 0;
        std::vector<state_id> states_;
        std::vector<std::size_t> edges_;  ///< position of the transition taken at each depth, or SIZE_MAX before the first
        word word_;
    };

    /// The words of the language of `a` whose length is at most `max_length`.
    word_range(const automaton& a, int max_length);

    [[nodiscard]] iterator begin() const { return iterator(data_); }
    [[nodiscard]] std::default_sentinel_t end() const noexcept { return {}; }

private:
    std::shared_ptr<const iterator::shared> data_;
};

/// The words of the language of `a` whose length is at most `max_length`, in shortlex order.
/// \throws std::invalid_argument if `max_length` is negative
[[nodiscard]] inline word_range words(const automaton& a, int max_length) {
    return {a, max_length};
}

}  // namespace autolib
