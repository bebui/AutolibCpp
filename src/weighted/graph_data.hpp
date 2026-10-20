// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "autolib/weighted_graph.hpp"

namespace autolib::detail {

/// The arrays of a graph, in compressed rows : the algorithms read them directly.
struct graph_data {
    state_id initial = no_state;
    std::vector<char> accept;
    std::vector<state_id> source, target;
    std::vector<char> epsilon;
    bool has_epsilon = false;
    // Labels of edge e : intervals [mins[i], maxs[i]] for i in [label_start[e], label_start[e+1])
    std::vector<std::uint32_t> label_start{0};
    std::vector<symbol> mins, maxs;
    std::vector<std::uint32_t> out_start, in_start;
    std::vector<edge_id> out_edges, in_edges;
    std::vector<state_id> order;  ///< a topological order, empty when the graph has a cycle (or no state)
    bool acyclic = true;
    bool deterministic = true;

    [[nodiscard]] std::size_t states() const noexcept { return accept.size(); }
    [[nodiscard]] std::size_t edges() const noexcept { return source.size(); }
    [[nodiscard]] std::span<const edge_id> out(state_id s) const noexcept {
        auto i = static_cast<std::size_t>(s);
        return {out_edges.data() + out_start[i], out_edges.data() + out_start[i + 1]};
    }
    [[nodiscard]] std::span<const edge_id> in(state_id s) const noexcept {
        auto i = static_cast<std::size_t>(s);
        return {in_edges.data() + in_start[i], in_edges.data() + in_start[i + 1]};
    }
    [[nodiscard]] bool accepts(edge_id e, symbol c) const noexcept {
        auto lo = label_start[static_cast<std::size_t>(e)], hi = label_start[static_cast<std::size_t>(e) + 1];
        while (lo < hi) {
            auto mid = lo + (hi - lo) / 2;
            if (maxs[mid] < c)
                lo = mid + 1;
            else if (mins[mid] > c)
                hi = mid;
            else
                return true;
        }
        return false;
    }
    /// The least symbol of a consuming edge : the symbol of a witness path.
    [[nodiscard]] symbol representative(edge_id e) const noexcept { return mins[label_start[static_cast<std::size_t>(e)]]; }
    [[nodiscard]] interval_set labels(edge_id e) const {
        interval_set set;
        for (auto i = label_start[static_cast<std::size_t>(e)]; i < label_start[static_cast<std::size_t>(e) + 1]; ++i)
            set.append({mins[i], maxs[i]});
        return set;
    }
    void check_state(state_id s) const {
        if (s < 0 || static_cast<std::size_t>(s) >= states())
            throw std::out_of_range("State " + std::to_string(s));
    }
    void check_edge(edge_id e) const {
        if (e < 0 || static_cast<std::size_t>(e) >= edges())
            throw std::out_of_range("Edge " + std::to_string(e));
    }
    void require_dag() const {
        if (!acyclic)
            throw std::invalid_argument("Acyclic graph required");
    }
    [[nodiscard]] std::vector<bool> useful() const;
};

inline std::size_t u(std::int64_t i) noexcept { return static_cast<std::size_t>(i); }

}  // namespace autolib::detail
