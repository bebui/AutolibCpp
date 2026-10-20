// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Enumeration, counting, ranking and uniform sampling of the words of a language.

#pragma once

#include <iterator>
#include <memory>
#include <optional>
#include <random>
#include <vector>

#include "autolib/automaton.hpp"
#include "autolib/big_integer.hpp"
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

/// The number of words of a given length in the language of `a`, by dynamic programming on a deterministic
/// automaton : O(length x transitions) operations on integers.
/// \throws std::invalid_argument if `length` is negative
[[nodiscard]] AUTOLIB_EXPORT big_integer count_words(const automaton& a, int length);

/// The words of a given length of a language, numbered in lexicographic order : `count()` words, of ranks 0 to
/// `count() - 1`. Draws them uniformly, and converts between words and ranks, each in time proportional to the
/// length times the number of transitions per state.
///
/// The number of words from every state for every remaining length is computed once, by the constructor : a
/// sampler draws many words quickly.
///
/// ```cpp
/// word_sampler sampler("(0|1)*0(0|1){3}"_re, 20);
/// std::mt19937_64 rng(42);
/// word w = sampler(rng);                   // uniform among the sampler.count() = 2^19 words
/// auto r = sampler.rank(w);                // its rank, sampler.unrank(*r) == w
/// ```
///
/// Instances are immutable and can be shared between threads, each thread drawing with its own generator.
class AUTOLIB_EXPORT word_sampler {
public:
    /// Prepares the words of length `length` of the language of `a`.
    /// \throws std::invalid_argument if `length` is negative
    word_sampler(const automaton& a, int length);

    [[nodiscard]] int length() const noexcept { return length_; }
    /// The number of words.
    [[nodiscard]] const big_integer& count() const noexcept { return count_; }
    /// Whether the language has no word of that length.
    [[nodiscard]] bool empty() const noexcept { return count_.is_zero(); }

    /// The word of a given rank in lexicographic order.
    /// \throws std::out_of_range unless 0 <= rank < count()
    [[nodiscard]] word unrank(const big_integer& rank) const;
    /// The rank of a word in lexicographic order, if it is one of the words.
    [[nodiscard]] std::optional<big_integer> rank(std::span<const symbol> w) const;
    [[nodiscard]] std::optional<big_integer> rank(std::initializer_list<symbol> w) const {
        return rank(std::span(w.begin(), w.size()));
    }

    /// Draws a word uniformly.
    /// \throws std::out_of_range if there is no word
    template <std::uniform_random_bit_generator G>
    [[nodiscard]] word operator()(G& g) const {
        if (empty())
            throw std::out_of_range("No word of length " + std::to_string(length_));
        return unrank(big_integer::uniform(count_, g));
    }

private:
    /// The magnitude of the number of words of length `remaining` accepted from `s`.
    [[nodiscard]] std::span<const big_integer::limb> from(int remaining, state_id s) const noexcept {
        auto i = static_cast<std::size_t>(remaining) * states_ + static_cast<std::size_t>(s);
        return {limbs_.data() + offsets_[i], limbs_.data() + offsets_[i + 1]};
    }

    compiled_dfa dfa_;
    int length_;
    std::size_t states_;
    // The counts, for every length then every state, stored contiguously : one allocation instead of one per count
    std::vector<big_integer::limb> limbs_;
    std::vector<std::size_t> offsets_;
    big_integer count_;
};

/// Draws a word of a given length uniformly among the words of that length of the language of `a`, or returns
/// nothing if there is none. To draw many words, a `word_sampler` avoids counting again.
/// \throws std::invalid_argument if `length` is negative
template <class G>
    requires std::uniform_random_bit_generator<std::remove_reference_t<G>>
[[nodiscard]] std::optional<word> random_word(const automaton& a, int length, G&& g) {
    word_sampler sampler(a, length);
    if (sampler.empty())
        return std::nullopt;
    return sampler(g);
}

}  // namespace autolib
