// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// The module autolib._autolib : the C++ library for Python.

#include "casters.hpp"

namespace autolib::python {
void bind_core(nb::module_& m);
void bind_regex(nb::module_& m);
void bind_compiled(nb::module_& m);
void bind_weighted(nb::module_& m);
}  // namespace autolib::python

NB_MODULE(_autolib, m) {
    m.doc() = "Finite automata over integer symbols, in C++.";
    m.attr("__version__") = AUTOLIB_VERSION;
    m.attr("MIN_SYMBOL") = autolib::min_symbol;
    m.attr("MAX_SYMBOL") = autolib::max_symbol;
    autolib::python::bind_core(m);
    autolib::python::bind_regex(m);
    autolib::python::bind_compiled(m);
    autolib::python::bind_weighted(m);
}
