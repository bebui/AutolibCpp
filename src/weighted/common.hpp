// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "autolib/multi_weighted_automaton.hpp"
#include "autolib/weighted_automaton.hpp"
#include "weighted/graph_data.hpp"

namespace autolib {

struct weighted_automaton::weights_data {
    std::vector<double> edges;
    double initial = 0;
    std::vector<double> finals;
};

/// The vectors of an automaton, dimension after dimension for each edge and each state.
struct multi_weighted_automaton::vectors {
    std::size_t dimensions = 1;
    std::vector<double> edges, initial, finals;
};

namespace detail {

constexpr double inf = std::numeric_limits<double>::infinity();

/// A finite weight, -0 becoming 0.
inline double finite(double value) {
    if (!std::isfinite(value))
        throw std::invalid_argument("Finite weight required: " + std::to_string(value));
    return value == 0 ? 0 : value;
}

/// A sum of costs, checked against overflow.
inline double add(double a, double b) {
    double sum = a + b;
    if (!std::isfinite(sum))
        throw std::overflow_error("Cost overflow");
    return sum == 0 ? 0 : sum;
}

inline double multiply(double a, double b) {
    double product = a * b;
    if (!std::isfinite(product))
        throw std::overflow_error("Cost overflow");
    return product == 0 ? 0 : product;
}

/// Checked copy of weights, of an expected count.
inline std::vector<double> copy_weights(std::vector<double> values, std::size_t size) {
    if (values.size() != size)
        throw std::invalid_argument("Expected " + std::to_string(size) + " weights, got " + std::to_string(values.size()));
    for (double& v : values)
        v = finite(v);
    return values;
}

/// The sum of a path, without overflow : +infinity and -infinity are absorbing.
inline double join(double left, double weight, double right) {
    if (left == inf || right == inf)
        return inf;
    if (left == -inf || right == -inf)
        return -inf;
    return add(add(left, weight), right);
}

/// Certifies that sums of a number of terms among some doubles are exact in doubles : the largest magnitude,
/// divided by the finest quantum of the values, must stay below 2^53 divided by the number of terms.
class exactness {
public:
    void include(double value) noexcept {
        if (value == 0)
            return;
        largest_ = std::max(largest_, std::fabs(value));
        std::uint64_t bits;
        double magnitude = std::fabs(value);
        std::memcpy(&bits, &magnitude, sizeof bits);
        auto e = static_cast<int>(bits >> 52);
        std::uint64_t mantissa = bits & ((std::uint64_t{1} << 52) - 1);
        if (e != 0)
            mantissa |= std::uint64_t{1} << 52;
        int quantum = (e == 0 ? -1074 : e - 1023 - 52) + std::countr_zero(mantissa);
        exponent_ = std::min(exponent_, quantum);
    }
    [[nodiscard]] bool safe(double terms) const noexcept {
        return largest_ == 0 ||
               (largest_ <= std::numeric_limits<double>::max() / terms && std::scalbn(largest_, -exponent_) <= 0x1.0p53 / terms);
    }

private:
    int exponent_ = std::numeric_limits<int>::max();
    double largest_ = 0;
};

/// Access to the internals of the weighted types, for the algorithms.
struct weighted_access {
    using weights = weighted_automaton::weights_data;

    static const graph_data& data(const automaton_graph& g) noexcept { return *g.data_; }
    static const weights& weights_of(const weighted_automaton& a) noexcept { return *a.weights_; }
    static weighted_automaton make(automaton_graph g, weights w) {
        return {std::move(g), std::make_shared<const weights>(std::move(w))};
    }
    using vectors = multi_weighted_automaton::vectors;
    static const vectors& vectors_of(const multi_weighted_automaton& a) noexcept { return *a.vectors_; }
    static multi_weighted_automaton make(automaton_graph g, vectors v) {
        return {std::move(g), std::make_shared<const vectors>(std::move(v))};
    }
    static shortest_path_result result(const weighted_automaton& a, std::vector<double> distances,
                                       std::vector<edge_id> parent, state_id start, bool longest) {
        return {a, std::move(distances), std::move(parent), start, longest};
    }
};

/// The graph arrays of an automaton.
inline const graph_data& data(const automaton_graph& g) noexcept { return weighted_access::data(g); }
inline const graph_data& data(const weighted_automaton& a) noexcept { return weighted_access::data(a.graph()); }
/// The weights of an automaton.
inline const auto& weights(const weighted_automaton& a) noexcept { return weighted_access::weights_of(a); }

}  // namespace detail
}  // namespace autolib
