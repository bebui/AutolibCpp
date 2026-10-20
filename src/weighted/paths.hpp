// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#pragma once

#include <vector>

#include "weighted/common.hpp"

namespace autolib::detail {

/// Distances, with the edge reaching each state on a best path when requested.
struct relaxed {
    std::vector<double> dist;
    std::vector<edge_id> parent;
};

/// +infinity everywhere except at one state.
std::vector<double> seeds(std::size_t n, state_id s, double weight);
bool has_negative(const weighted_automaton& a, bool epsilon_only);
/// Distances from the finite seeds, forward or backward, along every edge or the epsilon edges only.
relaxed solve(const weighted_automaton& a, const std::vector<double>& seed, bool reverse, bool epsilon_only, bool track);
/// Shortest paths from a state of value `value`.
shortest_path_result shortest_from(const weighted_automaton& a, state_id start, double value, bool track, bool longest,
                                   bool force_dijkstra);
double fixed_length_cost(const weighted_automaton& a, int length, bool at_most);

/// An indexed binary heap of states ordered by a distance array, without duplicates.
class state_heap {
public:
    explicit state_heap(const std::vector<double>& distance) : d_(&distance), heap_(distance.size()), position_(distance.size(), -1) {}
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    void clear() noexcept {
        for (std::size_t i = 0; i < size_; ++i)
            position_[u(heap_[i])] = -1;
        size_ = 0;
    }
    /// Inserts a state, or moves it up after its distance decreased.
    void offer(state_id s) noexcept {
        const auto& d = *d_;
        auto i = position_[u(s)];
        if (i < 0) {
            i = static_cast<std::int32_t>(size_++);
            heap_[u(i)] = s;
            position_[u(s)] = i;
        }
        while (i > 0) {
            auto p = (i - 1) / 2;
            if (d[u(heap_[u(p)])] <= d[u(s)])
                break;
            heap_[u(i)] = heap_[u(p)];
            position_[u(heap_[u(i)])] = i;
            i = p;
        }
        heap_[u(i)] = s;
        position_[u(s)] = i;
    }
    state_id poll() noexcept {
        const auto& d = *d_;
        state_id result = heap_[0], last = heap_[--size_];
        position_[u(result)] = -1;
        if (size_ == 0)
            return result;
        std::size_t i = 0;
        while (2 * i + 1 < size_) {
            auto child = 2 * i + 1;
            if (child + 1 < size_ && d[u(heap_[child + 1])] < d[u(heap_[child])])
                ++child;
            if (d[u(last)] <= d[u(heap_[child])])
                break;
            heap_[i] = heap_[child];
            position_[u(heap_[i])] = static_cast<std::int32_t>(i);
            i = child;
        }
        heap_[i] = last;
        position_[u(last)] = static_cast<std::int32_t>(i);
        return result;
    }

private:
    const std::vector<double>* d_;
    std::vector<state_id> heap_;
    std::vector<std::int32_t> position_;
    std::size_t size_ = 0;
};

/// Closes distances under the epsilon edges, reusing its buffers when no epsilon edge is negative.
class epsilon_workspace {
public:
    explicit epsilon_workspace(const weighted_automaton& a);
    void close(std::vector<double>& distances);

private:
    const weighted_automaton* a_;
    bool signed_;
    // The epsilon edges leaving each state, in compressed rows : targets and weights
    std::vector<std::uint32_t> start_;
    std::vector<state_id> target_;
    std::vector<double> weight_;
    std::vector<double> work_;
    state_heap heap_;
};

}  // namespace autolib::detail
