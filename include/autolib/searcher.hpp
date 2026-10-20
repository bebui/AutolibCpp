// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Search of the factors of a sequence of symbols that belong to a regular language.

#pragma once

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <span>
#include <vector>

#include "autolib/compiled_dfa.hpp"
#include "autolib/export.hpp"

namespace autolib {

/// A match : the symbols `text[start .. end-1]`.
struct match {
    std::size_t start;
    std::size_t end;

    [[nodiscard]] std::size_t length() const noexcept { return end - start; }
    friend bool operator==(const match&, const match&) = default;
};

/// Searches leftmost-longest matches of a language in sequences of symbols : the match starting first is chosen,
/// and among those the longest one. Finding the leftmost start takes a single backward pass with an automaton
/// recognizing the mirror of `L.*`, so a search is linear in the length of the text, plus the length of the match.
///
/// ```cpp
/// searcher s("12+"_re);
/// auto m = s.find({3, 1, 2, 2, 4});   // match{1, 4}
/// ```
///
/// Instances are immutable and can be shared between threads.
class AUTOLIB_EXPORT searcher {
public:
    /// Prepares the search of the words of a language.
    explicit searcher(const automaton& language);

    /// The leftmost-longest match starting at or after `from`.
    [[nodiscard]] std::optional<match> find(std::span<const symbol> text, std::size_t from = 0) const;
    [[nodiscard]] std::optional<match> find(std::initializer_list<symbol> text) const { return find(std::span(text.begin(), text.size())); }
    /// Checks whether a factor of the text belongs to the language.
    [[nodiscard]] bool contains(std::span<const symbol> text) const;
    [[nodiscard]] bool contains(std::initializer_list<symbol> text) const { return contains(std::span(text.begin(), text.size())); }
    /// The successive non-overlapping leftmost-longest matches. After an empty match, the search resumes one symbol further.
    [[nodiscard]] std::vector<match> find_all(std::span<const symbol> text) const;
    [[nodiscard]] std::vector<match> find_all(std::initializer_list<symbol> text) const { return find_all(std::span(text.begin(), text.size())); }

private:
    [[nodiscard]] std::optional<std::size_t> leftmost_start(std::span<const symbol> text, std::size_t from) const;
    [[nodiscard]] std::size_t longest_end(std::span<const symbol> text, std::size_t start) const;

    compiled_dfa forward_;  ///< recognizes L
    compiled_dfa starts_;   ///< recognizes the mirror of L.* : reading text[i..] backwards accepts iff a match starts at i
};

}  // namespace autolib
