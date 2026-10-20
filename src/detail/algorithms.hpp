// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "autolib/automaton.hpp"

namespace autolib::detail {

/// Boolean combinations computed by a product of deterministic automata.
enum class product_kind { union_of, intersection_of, difference_of, symmetric_difference_of };

/// Deterministic automaton of a boolean combination, by the product of the determinized operands
/// restricted to the reachable pairs.
automaton product(const automaton& left, const automaton& right, product_kind kind);

/// Coarsest bisimulation of the labelled transition system (src[t], lab[t], dst[t]) refining
/// accepting / non accepting, by Paige and Tarjan's algorithm. Returns the block of every state.
std::vector<std::int32_t> coarsest_bisimulation(std::int32_t n, std::span<const std::int32_t> src,
                                                std::span<const std::int32_t> lab, std::span<const std::int32_t> dst,
                                                std::int32_t labels, const std::vector<char>& accept);

}  // namespace autolib::detail
