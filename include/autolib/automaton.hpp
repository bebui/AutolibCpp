// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

/// \file
/// Finite automata over integer symbols and the operations of regular languages.

#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "autolib/detail/small_vector.hpp"
#include "autolib/export.hpp"
#include "autolib/interval_set.hpp"

namespace autolib {

namespace detail {
struct access;
}

/// A state, numbered from 0 in its automaton.
using state_id = std::int32_t;
/// No state : the absent initial state, or the dead state reached when no transition applies.
inline constexpr state_id no_state = -1;
/// A word : a sequence of symbols.
using word = std::vector<symbol>;

/// A transition towards `target`, taken by reading any symbol of `labels`.
struct transition {
    state_id target;
    interval_set labels;

    friend bool operator==(const transition&, const transition&) = default;
};

/// A transition of an automaton as the cost functions of its weightings see it : a transition reading
/// symbols, or an epsilon transition.
struct automaton_edge {
    state_id from;
    state_id to;
    bool epsilon;         ///< an epsilon transition, reading nothing
    interval_set labels;  ///< the symbols read, empty for an epsilon transition
    std::size_t index;    ///< its rank among the transitions(from), or among the epsilons(from)
};

/// The cost of a transition, for weighting an automaton.
using edge_cost = std::function<double(const automaton_edge&)>;

class automaton_snapshot;
class multi_weighted_automaton;
class weighted_automaton;

/// Algorithms computing the minimal deterministic automaton.
enum class minimization {
    /// Partition refinement of Valmari and Lehtinen, in O(m log n) : the default.
    partition_refinement,
    /// Double reversal of Brzozowski, exponential in the worst case.
    brzozowski,
};

/// A finite automaton, possibly non-deterministic and with epsilon transitions, whose transitions
/// read sets of 32-bit integer symbols.
///
/// Automata are regular values : copying an automaton copies its states, and operations return new automata.
/// The operators follow the operations of regular languages :
///
/// ```cpp
/// using namespace autolib::literals;
/// auto a = "(0|1)*0"_re;          // minimal DFA of a regular expression
/// auto b = "1(0|1)*"_re;
/// auto c = (a | b) & ~"11"_re;    // union, intersection, complement
/// auto d = a + b;                 // concatenation
/// auto e = *a;                    // Kleene star, also a.star()
/// auto f = +(a | b);              // one or more repetitions, also (a | b).plus()
/// bool same = (a - b) == (a & ~b); // equality of languages
/// bool included = a <= (a | b);   // inclusion of languages
/// bool accepted = a({1, 1, 0});   // membership
/// ```
///
/// Const member functions may be called concurrently.
class AUTOLIB_EXPORT automaton {
public:
    /// The automaton without any state, which recognizes the empty language.
    automaton() = default;

    // ------------------------------------------------------------------ elementary languages

    /// The empty language.
    [[nodiscard]] static automaton empty_language() { return {}; }
    /// The language `{ε}`.
    [[nodiscard]] static automaton empty_word();
    /// The words of one symbol taken from `symbols`.
    [[nodiscard]] static automaton symbols(const interval_set& symbols);
    /// Every word of one symbol.
    [[nodiscard]] static automaton any_symbol() { return symbols(interval_set::all()); }
    /// Every word.
    [[nodiscard]] static automaton universal();
    /// The single word `w`.
    [[nodiscard]] static automaton of_word(std::span<const symbol> w);
    [[nodiscard]] static automaton of_word(std::initializer_list<symbol> w) { return of_word(std::span(w.begin(), w.size())); }
    /// Non-deterministic automaton of a regular expression, built in linear time. See regex.hpp for the syntax.
    /// \throws regex_error if the expression is malformed
    [[nodiscard]] static automaton nfa_from_regex(std::string_view expression);
    /// Minimal deterministic automaton of a regular expression.
    /// \throws regex_error if the expression is malformed
    [[nodiscard]] static automaton from_regex(std::string_view expression);

    // ------------------------------------------------------------------ structure

    /// Adds a state and returns its number.
    state_id add_state(bool accept = false);
    /// Number of states.
    [[nodiscard]] std::size_t state_count() const noexcept { return states_.size(); }
    /// Number of symbol and epsilon transitions.
    [[nodiscard]] std::size_t transition_count() const noexcept;
    /// The initial state, or `no_state`.
    [[nodiscard]] state_id initial() const noexcept { return initial_; }
    void set_initial(state_id s);
    [[nodiscard]] bool is_accept(state_id s) const { return states_[checked(s)].accept; }
    void set_accept(state_id s, bool accept = true) { states_[checked(s)].accept = accept; }

    /// Adds a transition reading the symbols of `labels`; an empty set adds nothing.
    void add_transition(state_id from, state_id to, interval_set labels);
    /// Adds an epsilon transition.
    void add_epsilon(state_id from, state_id to);
    /// The symbol transitions leaving a state.
    [[nodiscard]] std::span<const transition> transitions(state_id s) const { return states_[checked(s)].out; }
    /// The targets of the epsilon transitions leaving a state.
    [[nodiscard]] std::span<const state_id> epsilons(state_id s) const { return states_[checked(s)].eps; }
    /// True when no state has two transitions sharing a symbol and there is no epsilon transition.
    /// A deterministic automaton need not be complete.
    [[nodiscard]] bool is_deterministic() const noexcept { return deterministic_; }
    /// The target of the transition reading `s` from a state of a deterministic automaton, or `no_state`.
    [[nodiscard]] state_id step(state_id from, symbol s) const;
    /// Removes the states that are unreachable or lead to no accepting state, and renumbers the others
    /// in breadth first order from the initial state.
    void trim();

    // ------------------------------------------------------------------ membership

    /// Checks whether the word belongs to the language.
    [[nodiscard]] bool run(std::span<const symbol> w) const;
    [[nodiscard]] bool run(std::initializer_list<symbol> w) const { return run(std::span(w.begin(), w.size())); }
    [[nodiscard]] bool operator()(std::span<const symbol> w) const { return run(w); }
    [[nodiscard]] bool operator()(std::initializer_list<symbol> w) const { return run(w); }

    // ------------------------------------------------------------------ transformations

    /// Equivalent deterministic automaton, by the subset construction restricted to reachable subsets.
    [[nodiscard]] automaton determinize() const;
    /// Minimal deterministic automaton of the language, without dead state.
    [[nodiscard]] automaton minimize(minimization algorithm = minimization::partition_refinement) const;
    /// Equivalent automaton without epsilon transitions, with at most as many states.
    [[nodiscard]] automaton remove_epsilon() const;
    /// Equivalent automaton without epsilon transitions, whose states are merged by the coarsest bisimulation.
    /// Unlike minimize(), never blows up, but the result may be non-deterministic and not minimal.
    [[nodiscard]] automaton reduce() const;
    /// Automaton of the mirror language.
    [[nodiscard]] automaton reverse() const;
    /// Kleene star.
    [[nodiscard]] automaton star() const;
    /// One or more repetitions.
    [[nodiscard]] automaton plus() const;
    /// Zero or one occurrence.
    [[nodiscard]] automaton optional() const;
    /// From `min` to `max` repetitions; `max < 0` means unbounded.
    [[nodiscard]] automaton repeat(int min, int max) const;

    /// Union : minimal DFA when both operands are deterministic.
    [[nodiscard]] friend automaton operator|(const automaton& a, const automaton& b) { return a.union_with(b); }
    /// Intersection, as a minimal DFA.
    [[nodiscard]] friend automaton operator&(const automaton& a, const automaton& b) { return a.intersect_with(b); }
    /// Difference, as a minimal DFA.
    [[nodiscard]] friend automaton operator-(const automaton& a, const automaton& b) { return a.minus(b); }
    /// Complement with respect to every word, as a complete DFA.
    [[nodiscard]] friend automaton operator~(const automaton& a) { return a.complement(); }
    /// Concatenation : minimal DFA when both operands are deterministic.
    [[nodiscard]] friend automaton operator+(const automaton& a, const automaton& b) { return a.concatenate(b); }
    /// Kleene star, as star() : the prefix form of `r*`. Binding tighter than the binary operators, `*a + b`
    /// is the concatenation of `a*` and `b`.
    [[nodiscard]] friend automaton operator*(const automaton& a) { return a.star(); }
    /// One or more repetitions, as plus() : the prefix form of `r+`.
    [[nodiscard]] friend automaton operator+(const automaton& a) { return a.plus(); }
    automaton& operator|=(const automaton& b) { return *this = *this | b; }
    automaton& operator&=(const automaton& b) { return *this = *this & b; }
    automaton& operator-=(const automaton& b) { return *this = *this - b; }
    automaton& operator+=(const automaton& b) { return *this = *this + b; }

    [[nodiscard]] automaton union_with(const automaton& other) const;
    [[nodiscard]] automaton intersect_with(const automaton& other) const;
    [[nodiscard]] automaton minus(const automaton& other) const;
    [[nodiscard]] automaton symmetric_difference(const automaton& other) const;
    [[nodiscard]] automaton complement() const;
    [[nodiscard]] automaton concatenate(const automaton& other) const;

    // ------------------------------------------------------------------ language properties

    /// Checks whether the language is empty.
    [[nodiscard]] bool is_empty() const;
    /// Checks whether the language contains every word.
    [[nodiscard]] bool is_universal() const;
    /// Checks whether the language is finite.
    [[nodiscard]] bool is_finite() const;
    /// Equality of languages, by Hopcroft and Karp's union-find on the determinized operands.
    [[nodiscard]] bool same_language(const automaton& other) const;
    /// Inclusion of languages, by the antichain algorithm, without determinizing `other` first.
    [[nodiscard]] bool is_subset_of(const automaton& other) const;
    /// A shortest word of the language, the smallest symbol being chosen on each transition.
    [[nodiscard]] std::optional<word> shortest_word() const;
    /// A shortest word belonging to exactly one of the two languages, or nothing if they are equal.
    [[nodiscard]] std::optional<word> counter_example(const automaton& other) const;

    /// Equality of languages.
    [[nodiscard]] friend bool operator==(const automaton& a, const automaton& b) { return a.same_language(b); }
    /// Inclusion of languages.
    [[nodiscard]] friend bool operator<=(const automaton& a, const automaton& b) { return a.is_subset_of(b); }
    [[nodiscard]] friend bool operator>=(const automaton& a, const automaton& b) { return b.is_subset_of(a); }
    /// Strict inclusion of languages.
    [[nodiscard]] friend bool operator<(const automaton& a, const automaton& b) { return a.is_subset_of(b) && !b.is_subset_of(a); }
    [[nodiscard]] friend bool operator>(const automaton& a, const automaton& b) { return b < a; }
    /// Partial order of inclusion : `unordered` when neither language contains the other.
    [[nodiscard]] friend std::partial_ordering operator<=>(const automaton& a, const automaton& b) { return a.compare(b); }
    [[nodiscard]] std::partial_ordering compare(const automaton& other) const;

    // ------------------------------------------------------------------ weightings

    // Defined with the weighted automata : include autolib/automaton_snapshot.hpp, or autolib/autolib.hpp.

    /// The structure of this automaton captured once, to weight it several times on the same graph. The
    /// snapshot does not follow the later changes of the automaton.
    [[nodiscard]] automaton_snapshot snapshot() const;
    /// This automaton weighted by a cost per transition, its initial and final costs being 0.
    /// \throws std::invalid_argument if a cost is not finite
    [[nodiscard]] weighted_automaton weighted(const edge_cost& cost) const;
    /// This automaton weighted by vectors, one cost function per dimension : the costs of a path stay together.
    /// \throws std::invalid_argument without dimension, or if a cost is not finite
    [[nodiscard]] multi_weighted_automaton multi_weighted(std::span<const edge_cost> dimensions) const;
    [[nodiscard]] multi_weighted_automaton multi_weighted(std::initializer_list<edge_cost> dimensions) const;

    // ------------------------------------------------------------------ output

    /// Graphviz representation.
    [[nodiscard]] std::string to_dot() const;
    /// One line per transition, such as `0 -{1..3}-> 2`.
    [[nodiscard]] std::string to_string() const;
    friend std::ostream& operator<<(std::ostream& out, const automaton& a) { return a.print(out); }

private:
    // Transitions stored inline up to a few per state : traversals read the states contiguously
    struct state_data {
        detail::small_vector<transition, 2> out;
        detail::small_vector<state_id, 1> eps;
        bool accept = false;
    };

    [[nodiscard]] std::size_t checked(state_id s) const;
    std::ostream& print(std::ostream& out) const;
    /// Appends a copy of the states of `other`, returning the number of its first state here.
    state_id embed(const automaton& other);

    std::vector<state_data> states_;
    state_id initial_ = no_state;
    bool deterministic_ = true;

    friend struct detail::access;
};

/// User-defined literals for automata.
namespace literals {
/// Minimal DFA of a regular expression : `"(0|1)*1"_re`.
[[nodiscard]] inline automaton operator""_re(const char* expression, std::size_t length) {
    return automaton::from_regex({expression, length});
}
/// Non-deterministic automaton of a regular expression : `"(0|1)*1"_nfa`.
[[nodiscard]] inline automaton operator""_nfa(const char* expression, std::size_t length) {
    return automaton::nfa_from_regex({expression, length});
}
}  // namespace literals

}  // namespace autolib
