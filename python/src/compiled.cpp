// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Regular expressions, word samplers, compiled automata and searches.

#include <nanobind/stl/string_view.h>

#include "casters.hpp"

namespace autolib::python {

using release_gil = nb::call_guard<nb::gil_scoped_release>;

big_integer draw_below(const big_integer& bound, nb::handle rng);

void bind_regex(nb::module_& m) {
    // regex_error becomes RegexError, a ValueError telling the position of the error
    static nb::object error = nb::steal(PyErr_NewExceptionWithDoc("autolib.RegexError",
                                                                  "A malformed regular expression; position tells where.",
                                                                  PyExc_ValueError, nullptr));
    m.attr("RegexError") = error;
    nb::register_exception_translator([](const std::exception_ptr& p, void*) {
        try {
            std::rethrow_exception(p);
        } catch (const regex_error& e) {
            nb::object instance = error(nb::str(e.what()));
            instance.attr("position") = e.position();
            PyErr_SetObject(error.ptr(), instance.ptr());
        }
    });

    nb::class_<regex>(m, "Regex", R"(A parsed regular expression over integer symbols.

Symbols are the digits 0 to 9 or integers between angle brackets, ``<-3>``, ``<42>``; ``.`` is any symbol.
Operators : concatenation, ``|`` union, ``&`` intersection, ``~`` complement, ``*``, ``+``, ``?``, ``{n}``,
``{n,m}``, ``{n,}``, ``{,m}``, and ``()`` for the empty word::

    >>> Regex("(<10>|<20>)+.{2,}").to_dfa().shortest_word()
    [10, -2147483648, -2147483648]
)")
        .def(nb::init<std::string_view>(), nb::arg("expression"))
        .def("to_nfa", &regex::to_nfa, "The Thompson automaton, built in linear time.", release_gil())
        .def("to_dfa", &regex::to_dfa, "The minimal DFA.", release_gil())
        .def("__str__", &regex::to_string)
        .def("__repr__", [](const regex& r) { return "Regex(" + nb::cast<std::string>(nb::repr(nb::str(r.to_string().c_str()))) + ")"; });
}

void bind_compiled(nb::module_& m) {
    nb::class_<word_sampler>(m, "WordSampler", R"(The words of one length of a language, numbered in lexicographic order.

Counts once, then draws, ranks and unranks words quickly::

    >>> sampler = WordSampler(Automaton("(0|1)*0(0|1){3}"), 20)
    >>> sampler.count
    524288
    >>> w = sampler.draw()
    >>> sampler.unrank(sampler.rank(w)) == w
    True
)")
        .def(nb::init<const automaton&, int>(), nb::arg("automaton"), nb::arg("length"), release_gil())
        .def_prop_ro("length", &word_sampler::length)
        .def_prop_ro("count", &word_sampler::count, "The number of words, exactly.")
        .def("__len__", [](const word_sampler& s) {
            auto n = s.count().to<std::int64_t>();
            if (!n)
                throw std::overflow_error("more words than an index can hold : use count");
            return *n;
        })
        .def("__bool__", [](const word_sampler& s) { return !s.empty(); })
        .def("unrank", &word_sampler::unrank, nb::arg("rank"), "The word of a rank, from 0 to count - 1.")
        .def("__getitem__", [](const word_sampler& s, const big_integer& rank) {
            if (rank.is_negative())
                return s.unrank(rank + s.count());
            return s.unrank(rank);
        })
        .def("rank", [](const word_sampler& s, const word_view& w) { return s.rank(w); }, nb::arg("word"),
             "The rank of a word, or None if it is not one of the words.")
        .def("draw", [](const word_sampler& s, nb::handle rng) {
            if (s.empty())
                throw nb::index_error("no word of this length");
            return s.unrank(draw_below(s.count(), rng));
        }, nb::arg("rng") = nb::none(), "A word drawn uniformly, with a random.Random, or the random module.");

    nb::class_<compiled_dfa>(m, "CompiledDFA", R"(An immutable DFA compiled into flat arrays, to read many words quickly.

Accepts lists and numpy arrays of integers; arrays of int32 are read in place, and long words without the GIL.
)")
        .def(nb::init<const automaton&, bool>(), nb::arg("automaton"), nb::arg("lookup_table") = true, release_gil())
        .def_prop_ro("state_count", &compiled_dfa::state_count)
        .def_prop_ro("has_lookup_table", &compiled_dfa::has_lookup_table)
        .def("run", [](const compiled_dfa& d, const word_view& w) {
            nb::gil_scoped_release release;
            return d.run(w);
        }, nb::arg("word"))
        .def("__call__", [](const compiled_dfa& d, const word_view& w) {
            nb::gil_scoped_release release;
            return d.run(w);
        }, nb::arg("word"))
        .def("__contains__", [](const compiled_dfa& d, const word_view& w) { return d.run(w); });

    nb::class_<match>(m, "Match", "A factor text[start:end] of a text.")
        .def(nb::init<std::size_t, std::size_t>(), nb::arg("start"), nb::arg("end"))
        .def_ro("start", &match::start)
        .def_ro("end", &match::end)
        .def("__len__", &match::length)
        .def("__eq__", [](const match& a, const match& b) { return a == b; })
        .def("__iter__", [](const match& x) { return nb::iter(nb::make_tuple(x.start, x.end)); })
        .def("__repr__", [](const match& x) { return "Match(" + std::to_string(x.start) + ", " + std::to_string(x.end) + ")"; });

    nb::class_<searcher>(m, "Searcher", R"(Searches the leftmost-longest factors of sequences belonging to a language, in linear time.

    >>> s = Searcher(Automaton("12+"))
    >>> s.find([3, 1, 2, 2, 4])
    Match(1, 4)
)")
        .def(nb::init<const automaton&>(), nb::arg("language"), release_gil())
        .def("find", [](const searcher& s, const word_view& text, std::size_t start) {
            nb::gil_scoped_release release;
            return s.find(text, start);
        }, nb::arg("text"), nb::arg("start") = 0, "The leftmost-longest match from a position, or None.")
        .def("find_all", [](const searcher& s, const word_view& text) {
            nb::gil_scoped_release release;
            return s.find_all(text);
        }, nb::arg("text"), "The successive non-overlapping leftmost-longest matches.")
        .def("contains", [](const searcher& s, const word_view& text) {
            nb::gil_scoped_release release;
            return s.contains(text);
        }, nb::arg("text"), "Whether a factor of the text belongs to the language.");
}

}  // namespace autolib::python
