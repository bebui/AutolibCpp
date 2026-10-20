// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace autolib::detail {

/// Accepts every state or edge.
struct any {
    constexpr bool operator()(std::size_t) const noexcept { return true; }
};

/// The breadth first propagation shared by the reachability questions : marks the states reachable from
/// queue[0 .. tail) along compressed rows. State s leads through the entries i of [start[s], start[s + 1]) to
/// destination[edge], where edge is edges[i], or i when edges is empty; an edge is followed when `follow(edge)`
/// holds, toward a state `enter(state)` accepts. The queue must hold room for every state.
template <class Index, class State, class Follow = any, class Enter = any>
void propagate(std::span<const Index> start, std::span<const std::int32_t> edges, std::span<const State> destination,
               std::vector<char>& reached, std::vector<State>& queue, std::size_t tail, Follow follow = {}, Enter enter = {}) {
    for (std::size_t head = 0; head < tail; ++head) {
        auto s = static_cast<std::size_t>(queue[head]);
        for (auto i = start[s]; i < start[s + 1]; ++i) {
            auto edge = edges.empty() ? static_cast<std::size_t>(i) : static_cast<std::size_t>(edges[static_cast<std::size_t>(i)]);
            if (!follow(edge))
                continue;
            auto next = destination[edge];
            if (!reached[static_cast<std::size_t>(next)] && enter(static_cast<std::size_t>(next))) {
                reached[static_cast<std::size_t>(next)] = 1;
                queue[tail++] = next;
            }
        }
    }
}

/// The predecessors of every state, in compressed rows : predecessors[start[t] .. start[t + 1]) lead to t.
struct predecessors {
    std::vector<std::uint32_t> start;
    std::vector<std::int32_t> sources;

    /// `each(s, f)` calls f(t) for every successor t of s.
    template <class Successors>
    predecessors(std::size_t n, Successors&& each) : start(n + 1, 0) {
        for (std::size_t s = 0; s < n; ++s)
            each(s, [&](std::int32_t t) { ++start[static_cast<std::size_t>(t) + 1]; });
        for (std::size_t s = 0; s < n; ++s)
            start[s + 1] += start[s];
        sources.resize(start[n]);
        std::vector<std::uint32_t> fill(start.begin(), start.end() - 1);
        for (std::size_t s = 0; s < n; ++s)
            each(s, [&](std::int32_t t) { sources[fill[static_cast<std::size_t>(t)]++] = static_cast<std::int32_t>(s); });
    }
};

}  // namespace autolib::detail
