// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Regular expressions over integer symbols.
///
/// | Syntax       | Language                                               |
/// |--------------|--------------------------------------------------------|
/// | `0` … `9`    | the one-symbol word made of that digit                 |
/// | `<n>`        | the one-symbol word `n`, any 32-bit integer : `<-3>`, `<42>` |
/// | `.`          | every one-symbol word                                  |
/// | `rs`         | concatenation                                          |
/// | `r\|s`       | union                                                  |
/// | `r&s`        | intersection                                           |
/// | `~r`         | complement                                             |
/// | `r*`         | zero or more repetitions                               |
/// | `r+`         | one or more repetitions                                |
/// | `r?`         | zero or one occurrence                                 |
/// | `r{n}`       | exactly n repetitions                                  |
/// | `r{n,m}`     | from n to m repetitions                                |
/// | `r{n,}`      | at least n repetitions                                 |
/// | `r{,m}`      | at most m repetitions                                  |
/// | `(r)`        | grouping; `()` is the empty word                       |
///
/// Precedence, from the loosest : `|`, `&`, concatenation, `~`, postfix operators.

#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "autolib/automaton.hpp"
#include "autolib/export.hpp"

namespace autolib {

/// Error raised on a malformed regular expression.
class AUTOLIB_EXPORT regex_error : public std::invalid_argument {
public:
    regex_error(const std::string& message, std::size_t position)
        : std::invalid_argument(message), position_(position) {}
    /// Position of the error in the expression.
    [[nodiscard]] std::size_t position() const noexcept { return position_; }

private:
    std::size_t position_;
};

/// A parsed regular expression over integer symbols.
///
/// ```cpp
/// regex r("(0|1)*<42>{2,}");
/// automaton nfa = r.to_nfa();   // Thompson construction, linear in the size of the expression
/// automaton dfa = r.to_dfa();   // minimal DFA
/// ```
class AUTOLIB_EXPORT regex {
public:
    /// Parses an expression.
    /// \throws regex_error if the expression is malformed
    explicit regex(std::string_view expression);

    /// Non-deterministic automaton with epsilon transitions, built in linear time
    /// (except for `&` and `~`, which determinize their operands).
    [[nodiscard]] automaton to_nfa() const;
    /// Minimal deterministic automaton.
    [[nodiscard]] automaton to_dfa() const { return to_nfa().minimize(); }
    /// Fully parenthesized text of the expression.
    [[nodiscard]] std::string to_string() const;

private:
    enum class kind { empty_word, symbols, sequence, alternation, intersection, complement, repeat };
    struct node {
        kind type;
        std::vector<int> children;
        interval_set labels;  ///< symbols
        int min = 0;          ///< repeat
        int max = -1;         ///< repeat, -1 when unbounded
    };
    class parser;

    int add(node n);
    state_id build(automaton& a, int n, state_id start) const;
    [[nodiscard]] automaton nfa_of(int n) const;
    void print(std::string& out, int n) const;

    std::vector<node> nodes_;
    int root_ = 0;
};

}  // namespace autolib
