// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <algorithm>
#include <limits>
#include <optional>
#include <vector>

#include "autolib/dyadic.hpp"
#include "weighted/common.hpp"

namespace autolib::detail {

/// Exact costs are integers counting units of 2^scale, where 2^scale is the finest quantum of the weights of
/// an automaton : every weight is such an integer, and so is every sum of weights. They are compared and added
/// as big integers, in machine arithmetic while they fit in 64 bits, without the alignments of dyadics.
class exact_scale {
public:
    /// Takes a value into account; the scale must cover every value converted.
    void include(double value) {
        if (value != 0)
            exponent_ = std::min(exponent_, dyadic(value).exponent());
    }
    [[nodiscard]] std::int64_t exponent() const noexcept { return exponent_ == unset ? 0 : exponent_; }
    /// A double as a number of units.
    [[nodiscard]] big_integer of(double value) const {
        dyadic d(value);
        return d.mantissa() << static_cast<std::size_t>(d.exponent() - exponent());
    }
    [[nodiscard]] dyadic value(const big_integer& units) const { return dyadic(units, exponent()); }
    /// The nearest double.
    /// \throws std::overflow_error beyond the doubles
    [[nodiscard]] double rounded(const big_integer& units) const {
        double result = value(units).to_double();
        if (!std::isfinite(result))
            throw std::overflow_error("Cost overflow");
        return result == 0 ? 0 : result;
    }
    /// The double equal to a number of units.
    /// \throws std::range_error if there is none
    [[nodiscard]] double representable(const big_integer& units) const {
        double result = rounded(units);
        if (of_checked(result) != units)
            throw std::range_error("Exact transformed weight is not representable as a double");
        return result;
    }

private:
    static constexpr std::int64_t unset = std::numeric_limits<std::int64_t>::max();
    /// A double as units, or an impossible value when it is finer than the scale.
    [[nodiscard]] big_integer of_checked(double value) const {
        dyadic d(value);
        if (!d.is_zero() && d.exponent() < exponent())
            return big_integer(1) << 4096;  // not a whole number of units, hence different
        return d.is_zero() ? big_integer() : d.mantissa() << static_cast<std::size_t>(d.exponent() - exponent());
    }
    std::int64_t exponent_ = unset;
};

/// An exact cost in units of the scale, absent for +infinity.
using exact = std::optional<big_integer>;

/// Whether a cost improves on a previous one, absent being +infinity.
inline bool improves(const big_integer& value, const exact& previous) { return !previous || value < *previous; }

/// The exact values of the weights of an automaton, for the algorithms comparing sums of costs.
class exact_costs {
public:
    explicit exact_costs(const weighted_automaton& a);

    [[nodiscard]] const graph_data& graph() const noexcept { return data(a_); }
    exact_scale scale;
    big_integer initial;
    std::vector<big_integer> edges;
    std::vector<exact> finals;  ///< absent for the non-accepting states

    /// The least exact cost from every useful state to acceptance, final costs included; absent if none.
    /// \throws std::invalid_argument if a useful negative cycle makes it unbounded
    [[nodiscard]] std::vector<exact> suffixes(const std::vector<bool>& useful) const;
    /// The least number of symbols read by a best suffix of every state, with the 0-1 BFS of the epsilon edges.
    [[nodiscard]] std::vector<int> symbol_steps(const std::vector<exact>& suffix) const;
    /// Closes distances under the epsilon edges, toward the allowed states only when given.
    /// \throws std::invalid_argument if a negative epsilon cycle is reachable
    void close_epsilon(std::vector<exact>& distances, const std::vector<bool>* allowed = nullptr) const;

private:
    weighted_automaton a_;
};

}  // namespace autolib::detail
