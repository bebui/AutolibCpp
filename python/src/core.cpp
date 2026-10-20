// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Sets of symbols, automata, regular expressions, words, compiled automata and searches.

#include <nanobind/make_iterator.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/tuple.h>

#include <memory>
#include <sstream>

#include "casters.hpp"

namespace autolib::python {

using release_gil = nb::call_guard<nb::gil_scoped_release>;

namespace {
bool is_range(nb::handle h) {
    static PyObject* range_type = nb::object(nb::module_::import_("builtins").attr("range")).release().ptr();
    return PyObject_IsInstance(h.ptr(), range_type) == 1;
}
}  // namespace

interval_set to_labels(nb::handle h) {
    if (nb::isinstance<interval_set>(h))
        return nb::cast<interval_set>(h);
    if (PyLong_Check(h.ptr()))
        return interval_set(to_symbol(h));
    auto range_of = [](nb::handle r) {
        auto start = nb::cast<long long>(r.attr("start")), stop = nb::cast<long long>(r.attr("stop"));
        if (nb::cast<long long>(r.attr("step")) != 1)
            throw nb::value_error("ranges of symbols have a step of 1");
        if (start >= stop)
            return interval_set();
        if (start < min_symbol || stop - 1 > max_symbol)
            throw std::overflow_error("range out of the 32-bit symbols");
        return interval_set::range(static_cast<symbol>(start), static_cast<symbol>(stop - 1));
    };
    if (is_range(h))
        return range_of(h);
    if (PyUnicode_Check(h.ptr()))
        throw nb::type_error("symbols are integers, not strings");
    interval_set result;
    for (nb::handle item : nb::iter(h)) {
        if (is_range(item))
            result |= range_of(item);
        else if (nb::isinstance<interval_set>(item))
            result |= nb::cast<interval_set>(item);
        else
            result.insert(to_symbol(item));
    }
    return result;
}

nb::object to_python(const big_integer& x) {
    if (auto small = x.to<std::int64_t>())
        return nb::steal(PyLong_FromLongLong(*small));
    return nb::steal(PyLong_FromString(x.to_string().c_str(), nullptr, 10));
}

big_integer to_big_integer(nb::handle h) {
    int overflow = 0;
    long long v = PyLong_AsLongLongAndOverflow(h.ptr(), &overflow);
    if (overflow == 0) {
        if (v == -1 && PyErr_Occurred())
            throw nb::python_error();
        return big_integer(v);
    }
    return big_integer(nb::cast<std::string>(nb::str(h)));
}

nb::object to_fraction(const dyadic& x) {
    static nb::object fraction = nb::module_::import_("fractions").attr("Fraction");
    if (x.exponent() >= 0)
        return fraction(to_python(x.mantissa() << static_cast<std::size_t>(x.exponent())));
    return fraction(to_python(x.mantissa()), to_python(big_integer(1) << static_cast<std::size_t>(-x.exponent())));
}

namespace {

/// The minimization algorithm named in Python.
minimization algorithm_of(std::string_view name) {
    if (name == "partition" || name == "hopcroft" || name == "valmari")
        return minimization::partition_refinement;
    if (name == "brzozowski")
        return minimization::brzozowski;
    throw nb::value_error("algorithm is 'partition' or 'brzozowski'");
}

std::string repr(const automaton& a) {
    std::ostringstream out;
    out << "<Automaton : " << a.state_count() << " states, " << a.transition_count() << " transitions"
        << (a.is_deterministic() ? ", deterministic" : "") << ">";
    return out.str();
}

/// A Python iterator over the words of a word_range.
struct word_iterator {
    word_range range;
    word_range::iterator it;
    bool started = false;
};

}  // namespace

/// A uniform random integer below a bound, drawn by Python's random module or a given generator.
big_integer draw_below(const big_integer& bound, nb::handle rng) {
    nb::object source = rng.is_none() ? nb::object(nb::module_::import_("random")) : nb::borrow(rng);
    return to_big_integer(source.attr("randrange")(to_python(bound)));
}

void bind_core(nb::module_& m) {
    // ------------------------------------------------------------------ IntervalSet

    nb::class_<interval_set>(m, "IntervalSet", R"(A set of 32-bit integer symbols, stored as sorted disjoint intervals.

Built from an integer, a ``range`` of step 1, or an iterable of integers and ranges::

    >>> IntervalSet(range(0, 10)) | IntervalSet([20, 21])
    IntervalSet({0..9, 20..21})
    >>> 5 in IntervalSet.range(0, 9)
    True
)")
        .def(nb::init<>(), "The empty set.")
        .def("__init__", [](interval_set* self, const labels_arg& symbols) { new (self) interval_set(symbols.value()); },
             nb::arg("symbols"), "The set of an integer, a range of step 1, or an iterable of integers and ranges.")
        .def_static("range", &interval_set::range, nb::arg("min"), nb::arg("max"), "The symbols from min to max, both included.")
        .def_static("all", &interval_set::all, "Every 32-bit symbol.")
        .def_prop_ro("intervals", [](const interval_set& s) {
                std::vector<std::pair<symbol, symbol>> out;
                for (interval i : s)
                    out.emplace_back(i.min, i.max);
                return out;
            }, "The intervals, as pairs (min, max) of included bounds.")
        .def_prop_ro("cardinality", &interval_set::cardinality, "The number of symbols.")
        .def_prop_ro("min", [](const interval_set& s) {
                if (s.empty())
                    throw nb::value_error("empty set");
                return s.min();
            }, "The least symbol.")
        .def_prop_ro("max", [](const interval_set& s) {
                if (s.empty())
                    throw nb::value_error("empty set");
                return s.max();
            }, "The greatest symbol.")
        .def("__len__", [](const interval_set& s) { return s.cardinality(); })
        .def("__bool__", [](const interval_set& s) { return !s.empty(); })
        .def("__contains__", [](const interval_set& s, symbol c) { return s.contains(c); })
        .def("__contains__", [](const interval_set& s, const interval_set& t) { return s.contains(t); })
        .def("__iter__", [](const interval_set& s) {
                // The symbols one after the other, lazily : a set may hold billions of them
                nb::object gen = nb::module_::import_("itertools").attr("chain").attr("from_iterable");
                nb::list ranges;
                for (interval i : s)
                    ranges.append(nb::module_::import_("builtins").attr("range")(i.min, static_cast<long long>(i.max) + 1));
                return gen(ranges);
            })
        .def("isdisjoint", [](const interval_set& s, const interval_set& t) { return !s.intersects(t); }, nb::arg("other"))
        .def("__or__", [](const interval_set& a, const interval_set& b) { return a | b; })
        .def("__and__", [](const interval_set& a, const interval_set& b) { return a & b; })
        .def("__sub__", [](const interval_set& a, const interval_set& b) { return a - b; })
        .def("__xor__", [](const interval_set& a, const interval_set& b) { return (a - b) | (b - a); })
        .def("__invert__", [](const interval_set& a) { return ~a; })
        .def("__eq__", [](const interval_set& a, const interval_set& b) { return a == b; })
        .def("__le__", [](const interval_set& a, const interval_set& b) { return b.contains(a); })
        .def("__lt__", [](const interval_set& a, const interval_set& b) { return b.contains(a) && a != b; })
        .def("__ge__", [](const interval_set& a, const interval_set& b) { return a.contains(b); })
        .def("__gt__", [](const interval_set& a, const interval_set& b) { return a.contains(b) && a != b; })
        .def("__hash__", &interval_set::hash)
        .def("__str__", &interval_set::to_string)
        .def("__repr__", [](const interval_set& s) { return "IntervalSet(" + s.to_string() + ")"; });

    nb::class_<word_iterator>(m, "WordIterator", "The words of a language, computed one at a time.")
        .def("__iter__", [](nb::handle self) { return nb::borrow(self); })
        .def("__next__", [](word_iterator& w) -> word {
            if (!w.started) {
                w.it = w.range.begin();
                w.started = true;
            } else {
                ++w.it;
            }
            if (w.it == std::default_sentinel)
                throw nb::stop_iteration();
            return *w.it;
        });

    // ------------------------------------------------------------------ Edge

    nb::class_<automaton_edge>(m, "Edge", "A transition as seen by the cost functions of a weighting.")
        .def_ro("source", &automaton_edge::from, "The state it leaves.")
        .def_ro("target", &automaton_edge::to, "The state it reaches.")
        .def_ro("epsilon", &automaton_edge::epsilon, "Whether it reads nothing.")
        .def_ro("labels", &automaton_edge::labels, "The symbols it reads, empty for an epsilon transition.")
        .def_ro("index", &automaton_edge::index, "Its rank among the transitions, or the epsilon transitions, of its source.")
        .def("__repr__", [](const automaton_edge& e) {
            return "Edge(" + std::to_string(e.from) + " -> " + std::to_string(e.to) + ", " +
                   (e.epsilon ? std::string("epsilon") : e.labels.to_string()) + ")";
        });

    // ------------------------------------------------------------------ Automaton

    nb::class_<automaton> cls(m, "Automaton", R"(A finite automaton over 32-bit integer symbols, deterministic or not, with epsilon transitions.

Operators follow the operations of regular languages::

    >>> a = Automaton("(0|1)*0")        # minimal DFA of a regular expression
    >>> b = Automaton("1(0|1)*")
    >>> c = (a | b) & ~Automaton("11")  # union, intersection, complement
    >>> [1, 1, 0] in a                   # membership
    True
    >>> (a - b) == (a & ~b)              # equality of languages
    True
    >>> a <= (a | b)                     # inclusion of languages
    True

Automata are values : operations return new automata.
)");
    cls.def(nb::init<>(), "The automaton without any state, recognizing nothing.")
        .def("__init__", [](automaton* self, std::string_view expression) { new (self) automaton(automaton::from_regex(expression)); },
             nb::arg("expression"), "The minimal DFA of a regular expression over integers.", release_gil())
        .def_static("from_regex", &automaton::from_regex, nb::arg("expression"), "The minimal DFA of a regular expression.", release_gil())
        .def_static("nfa", &automaton::nfa_from_regex, nb::arg("expression"),
                    "The Thompson automaton of a regular expression, built in linear time, with epsilon transitions.", release_gil())
        .def_static("empty_language", &automaton::empty_language, "Recognizes no word.")
        .def_static("empty_word", &automaton::empty_word, "Recognizes the empty word only.")
        .def_static("symbols", [](const labels_arg& symbols) { return automaton::symbols(symbols.value()); }, nb::arg("symbols"),
                    "Recognizes the words of one symbol among some symbols.")
        .def_static("any_symbol", &automaton::any_symbol, "Recognizes every word of one symbol.")
        .def_static("universal", &automaton::universal, "Recognizes every word.")
        .def_static("of_word", [](const word_view& w) { return automaton::of_word(w); }, nb::arg("word"), "Recognizes one word.")
        // Structure
        .def("add_state", &automaton::add_state, nb::arg("accept") = false, "Adds a state and returns its number.")
        .def_prop_ro("state_count", &automaton::state_count, "The number of states.")
        .def_prop_ro("transition_count", &automaton::transition_count, "The number of transitions, epsilon transitions included.")
        .def_prop_rw("initial", [](const automaton& a) -> std::optional<state_id> {
                return a.initial() == no_state ? std::nullopt : std::optional<state_id>(a.initial());
            }, [](automaton& a, state_id s) { a.set_initial(s); }, "The initial state, or None.")
        .def("set_initial", &automaton::set_initial, nb::arg("state"))
        .def("is_accept", &automaton::is_accept, nb::arg("state"))
        .def("set_accept", &automaton::set_accept, nb::arg("state"), nb::arg("accept") = true)
        .def("add_transition", [](automaton& a, state_id from, state_id to, const labels_arg& labels) { a.add_transition(from, to, labels.value()); },
             nb::arg("source"), nb::arg("target"), nb::arg("labels"),
             "Adds a transition reading an integer, a range, an IntervalSet or an iterable of them.")
        .def("add_epsilon", &automaton::add_epsilon, nb::arg("source"), nb::arg("target"), "Adds an epsilon transition.")
        .def("transitions", [](const automaton& a, state_id s) {
                std::vector<std::pair<state_id, interval_set>> out;
                for (const auto& t : a.transitions(s))
                    out.emplace_back(t.target, t.labels);
                return out;
            }, nb::arg("state"), "The transitions leaving a state, as pairs (target, labels).")
        .def("epsilons", [](const automaton& a, state_id s) {
                auto e = a.epsilons(s);
                return std::vector<state_id>(e.begin(), e.end());
            }, nb::arg("state"), "The targets of the epsilon transitions leaving a state.")
        .def_prop_ro("is_deterministic", &automaton::is_deterministic)
        .def("step", [](const automaton& a, state_id s, symbol c) -> std::optional<state_id> {
                auto t = a.step(s, c);
                return t == no_state ? std::nullopt : std::optional<state_id>(t);
            }, nb::arg("state"), nb::arg("symbol"), "The state reached from a state of a deterministic automaton, or None.")
        .def("trim", &automaton::trim, "Removes the useless states, in place.")
        // Membership
        .def("run", [](const automaton& a, const word_view& w) { return a.run(w); }, nb::arg("word"), "Whether a word is accepted.")
        .def("__contains__", [](const automaton& a, const word_view& w) { return a.run(w); })
        .def("__call__", [](const automaton& a, const word_view& w) { return a.run(w); }, nb::arg("word"))
        // Transformations
        .def("determinize", &automaton::determinize, release_gil())
        .def("minimize", [](const automaton& a, std::string_view algorithm) { return a.minimize(algorithm_of(algorithm)); },
             nb::arg("algorithm") = "partition", "The minimal DFA, by partition refinement or by Brzozowski's method.", release_gil())
        .def("remove_epsilon", &automaton::remove_epsilon, release_gil())
        .def("reduce", &automaton::reduce, "An equivalent automaton whose states are merged by bisimulation : never blows up.", release_gil())
        .def("reverse", &automaton::reverse, "The automaton of the mirror language.", release_gil())
        .def("star", &automaton::star, "Kleene star.", release_gil())
        .def("plus", &automaton::plus, "One or more repetitions.", release_gil())
        .def("optional", &automaton::optional, "Zero or one occurrence.", release_gil())
        .def("repeat", [](const automaton& a, int min, std::optional<int> max) { return a.repeat(min, max ? *max : -1); },
             nb::arg("min"), nb::arg("max") = nb::none(), "From min to max repetitions, None meaning unbounded.", release_gil())
        .def("__or__", [](const automaton& a, const automaton& b) { return a | b; }, release_gil())
        .def("__and__", [](const automaton& a, const automaton& b) { return a & b; }, release_gil())
        .def("__sub__", [](const automaton& a, const automaton& b) { return a - b; }, release_gil())
        .def("__xor__", [](const automaton& a, const automaton& b) { return a.symmetric_difference(b); }, release_gil())
        .def("__add__", [](const automaton& a, const automaton& b) { return a + b; }, release_gil())
        .def("__invert__", [](const automaton& a) { return ~a; }, release_gil())
        .def("__pos__", [](const automaton& a) { return +a; }, release_gil())
        .def("__mul__", [](const automaton& a, int n) { return a.repeat(n, n); }, nb::arg("count"), release_gil())
        .def("__pow__", [](const automaton& a, int n) { return a.repeat(n, n); }, nb::arg("count"), release_gil())
        .def("complement", &automaton::complement, release_gil())
        .def("concatenate", &automaton::concatenate, nb::arg("other"), release_gil())
        // Languages
        .def("is_empty", &automaton::is_empty, release_gil())
        .def("is_universal", &automaton::is_universal, release_gil())
        .def("is_finite", &automaton::is_finite, release_gil())
        .def("same_language", &automaton::same_language, nb::arg("other"), release_gil())
        .def("is_subset_of", &automaton::is_subset_of, nb::arg("other"), release_gil())
        .def("shortest_word", &automaton::shortest_word, "A shortest accepted word, or None.", release_gil())
        .def("counter_example", &automaton::counter_example, nb::arg("other"),
             "A shortest word accepted by one automaton only, or None if both languages are equal.", release_gil())
        .def("__eq__", [](const automaton& a, const automaton& b) { return a == b; }, release_gil())
        .def("__ne__", [](const automaton& a, const automaton& b) { return !(a == b); }, release_gil())
        .def("__le__", [](const automaton& a, const automaton& b) { return a <= b; }, release_gil())
        .def("__lt__", [](const automaton& a, const automaton& b) { return a < b; }, release_gil())
        .def("__ge__", [](const automaton& a, const automaton& b) { return a >= b; }, release_gil())
        .def("__gt__", [](const automaton& a, const automaton& b) { return a > b; }, release_gil())
        // Words
        .def("words", [](const automaton& a, int max_length) { return word_iterator{words(a, max_length), {}, false}; },
             nb::arg("max_length"), "The accepted words up to a length, lazily, in shortlex order : by length, then lexicographically.")
        .def("count_words", [](const automaton& a, int length) { return count_words(a, length); }, nb::arg("length"),
             "The number of accepted words of a length, exactly.", release_gil())
        .def("random_word", [](const automaton& a, int length, nb::handle rng) -> std::optional<word> {
                word_sampler sampler(a, length);
                if (sampler.empty())
                    return std::nullopt;
                return sampler.unrank(draw_below(sampler.count(), rng));
            }, nb::arg("length"), nb::arg("rng") = nb::none(),
             "A word of a length drawn uniformly among the accepted ones, or None; rng is a random.Random, or the random module.")
        .def("compile", [](const automaton& a, bool lookup_table) { return compiled_dfa(a, lookup_table); },
             nb::arg("lookup_table") = true, "A compiled DFA, to read many words quickly.", release_gil())
        .def("searcher", [](const automaton& a) { return searcher(a); }, "A searcher of the factors of texts in the language.",
             release_gil())
        // Output
        .def("to_dot", &automaton::to_dot, "The Graphviz representation.")
        .def("__str__", &automaton::to_string)
        .def("__repr__", &repr)
        .def("__copy__", [](const automaton& a) { return a; })
        .def("__deepcopy__", [](const automaton& a, nb::handle) { return a; }, nb::arg("memo"));
    cls.attr("__hash__") = nb::none();
}

}  // namespace autolib::python
