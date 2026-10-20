// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Conversions between Python values and the types of the library : words from any sequence of integers or
// from numpy arrays without copy, big integers as Python ints, dyadics as fractions.Fraction, and sets of
// symbols from integers, ranges and iterables.

#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <autolib/autolib.hpp>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace nb = nanobind;

namespace autolib::python {

/// A word read from Python : a view of a numpy array of int32, or a copy of the integers of a sequence.
struct word_view {
    std::span<const symbol> symbols;
    std::vector<symbol> storage;
    nb::object keep;  ///< the array viewed

    operator std::span<const symbol>() const noexcept { return symbols; }  // NOLINT(google-explicit-constructor)
};

/// A symbol from a Python integer.
/// \throws nb::python_error if it is not an integer of 32 bits
inline symbol to_symbol(nb::handle h) {
    if (!PyLong_Check(h.ptr()))
        throw nb::type_error("symbols are integers");
    long long v = PyLong_AsLongLong(h.ptr());
    if (v == -1 && PyErr_Occurred())
        throw nb::python_error();
    if (v < std::numeric_limits<symbol>::min() || v > std::numeric_limits<symbol>::max())
        throw std::overflow_error("symbol out of the 32-bit range: " + std::to_string(v));
    return static_cast<symbol>(v);
}

/// A set of symbols from an integer, a range of step 1, an IntervalSet, or an iterable of integers and ranges.
interval_set to_labels(nb::handle h);

/// Labels given in any of the forms of to_labels, converted when used so that errors keep their message.
struct labels_arg {
    nb::object source;
    [[nodiscard]] interval_set value() const { return to_labels(source); }
};

/// A cost function given as a number, or a function of an Edge.
struct cost_arg {
    edge_cost value;
};

/// A cost function of the positions and edges of an unfolding.
struct position_cost_arg {
    layered_automaton::position_weights value;
};

/// Python int from a big integer, exactly.
nb::object to_python(const big_integer& x);
/// Big integer from a Python int.
big_integer to_big_integer(nb::handle h);
/// fractions.Fraction from a dyadic, exactly.
nb::object to_fraction(const dyadic& x);

}  // namespace autolib::python

namespace nanobind::detail {

template <>
struct type_caster<autolib::python::word_view> {
    NB_TYPE_CASTER(autolib::python::word_view, const_name("collections.abc.Sequence[int]"))

    bool from_python(handle src, uint8_t flags, cleanup_list* cleanup) noexcept {
        using autolib::symbol;
        if (PyUnicode_Check(src.ptr()) || PyBytes_Check(src.ptr()))
            return false;
        // numpy arrays and other DLPack or buffer objects of 32-bit integers : viewed in place
        make_caster<ndarray<const std::int32_t, ndim<1>, c_contig, device::cpu>> as_int32;
        if (as_int32.from_python(src, flags & ~static_cast<uint8_t>(cast_flags::convert), cleanup)) {
            auto& array = as_int32.value;
            value.symbols = {array.data(), array.shape(0)};
            value.keep = borrow(src);
            return true;
        }
        make_caster<ndarray<const std::int64_t, ndim<1>, device::cpu>> as_int64;
        if (as_int64.from_python(src, flags & ~static_cast<uint8_t>(cast_flags::convert), cleanup)) {
            auto view = as_int64.value.view();
            value.storage.resize(view.shape(0));
            for (std::size_t i = 0; i < value.storage.size(); ++i) {
                auto v = view(i);
                if (v < std::numeric_limits<symbol>::min() || v > std::numeric_limits<symbol>::max())
                    return false;
                value.storage[i] = static_cast<symbol>(v);
            }
            value.symbols = value.storage;
            return true;
        }
        // Any iterable of integers
        PyObject* iterator = PyObject_GetIter(src.ptr());
        if (iterator == nullptr) {
            PyErr_Clear();
            return false;
        }
        object it = steal(iterator);
        while (PyObject* item = PyIter_Next(iterator)) {
            object guard = steal(item);
            if (!PyLong_Check(item))
                return false;
            long long v = PyLong_AsLongLong(item);
            if ((v == -1 && PyErr_Occurred()) || v < std::numeric_limits<symbol>::min() || v > std::numeric_limits<symbol>::max()) {
                PyErr_Clear();
                return false;
            }
            value.storage.push_back(static_cast<symbol>(v));
        }
        if (PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        value.symbols = value.storage;
        return true;
    }

    static handle from_cpp(const autolib::python::word_view& w, rv_policy, cleanup_list*) noexcept {
        PyObject* list = PyList_New(static_cast<Py_ssize_t>(w.symbols.size()));
        if (list == nullptr)
            return {};
        for (std::size_t i = 0; i < w.symbols.size(); ++i)
            PyList_SetItem(list, static_cast<Py_ssize_t>(i), PyLong_FromLong(w.symbols[i]));
        return list;
    }
};

template <>
struct type_caster<autolib::python::labels_arg> {
    NB_TYPE_CASTER(autolib::python::labels_arg,
                   const_name("int | range | IntervalSet | collections.abc.Iterable[int | range | IntervalSet]"))

    bool from_python(handle src, uint8_t, cleanup_list*) noexcept {
        if (src.is_none())
            return false;
        value.source = borrow(src);
        return true;
    }
    static handle from_cpp(const autolib::python::labels_arg&, rv_policy, cleanup_list*) noexcept { return {}; }
};

template <>
struct type_caster<autolib::python::cost_arg> {
    NB_TYPE_CASTER(autolib::python::cost_arg, const_name("float | collections.abc.Callable[[Edge], float]"))

    bool from_python(handle src, uint8_t, cleanup_list*) noexcept {
        if (PyFloat_Check(src.ptr()) || PyLong_Check(src.ptr())) {
            double c = PyFloat_AsDouble(src.ptr());
            if (c == -1.0 && PyErr_Occurred()) {
                PyErr_Clear();
                return false;
            }
            value.value = [c](const autolib::automaton_edge&) { return c; };
            return true;
        }
        if (!PyCallable_Check(src.ptr()))
            return false;
        object f = borrow(src);
        value.value = [f](const autolib::automaton_edge& e) {
            gil_scoped_acquire acquire;
            return cast<double>(f(e));
        };
        return true;
    }
    static handle from_cpp(const autolib::python::cost_arg&, rv_policy, cleanup_list*) noexcept { return {}; }
};

template <>
struct type_caster<autolib::python::position_cost_arg> {
    NB_TYPE_CASTER(autolib::python::position_cost_arg, const_name("collections.abc.Callable[[int, int], float]"))

    bool from_python(handle src, uint8_t, cleanup_list*) noexcept {
        if (!PyCallable_Check(src.ptr()))
            return false;
        object f = borrow(src);
        value.value = [f](int position, autolib::edge_id e) {
            gil_scoped_acquire acquire;
            return cast<double>(f(position, e));
        };
        return true;
    }
    static handle from_cpp(const autolib::python::position_cost_arg&, rv_policy, cleanup_list*) noexcept { return {}; }
};

template <>
struct type_caster<autolib::big_integer> {
    NB_TYPE_CASTER(autolib::big_integer, const_name("int"))

    bool from_python(handle src, uint8_t, cleanup_list*) noexcept {
        if (!PyLong_Check(src.ptr()))
            return false;
        try {
            value = autolib::python::to_big_integer(src);
            return true;
        } catch (...) {
            PyErr_Clear();
            return false;
        }
    }
    static handle from_cpp(const autolib::big_integer& x, rv_policy, cleanup_list*) noexcept {
        try {
            return autolib::python::to_python(x).release();
        } catch (...) {
            return {};
        }
    }
};

template <>
struct type_caster<autolib::dyadic> {
    NB_TYPE_CASTER(autolib::dyadic, const_name("fractions.Fraction"))

    bool from_python(handle, uint8_t, cleanup_list*) noexcept { return false; }
    static handle from_cpp(const autolib::dyadic& x, rv_policy, cleanup_list*) noexcept {
        try {
            return autolib::python::to_fraction(x).release();
        } catch (...) {
            return {};
        }
    }
};

}  // namespace nanobind::detail
