// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <vector>

#include "autolib/layered_automaton.hpp"

namespace autolib {

struct layered_automaton::structure {
    automaton_graph base, graph;
    int horizon = 0;
    std::vector<state_id> original_state;  ///< of every unfolded state
    std::vector<int> position;             ///< of every unfolded state
    std::vector<edge_id> original_edge;    ///< of every unfolded edge
    std::vector<int> edge_position;        ///< number of symbols read before every unfolded edge
};

}  // namespace autolib
