// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <algorithm>

#include "autolib/automaton.hpp"
#include "detail/access.hpp"
#include "detail/hash_table.hpp"
#include "detail/symbol_classes.hpp"

namespace autolib {

namespace {
std::size_t u(std::int32_t i) { return static_cast<std::size_t>(i); }
}  // namespace

automaton automaton::determinize() const {
    if (deterministic_)
        return *this;
    if (initial_ == no_state)
        return {};
    auto n = static_cast<std::int32_t>(states_.size());
    detail::symbol_classes classes([&](auto&& f) {
        for (const auto& s : states_)
            for (const auto& t : s.out)
                f(t.labels);
    });
    auto k = classes.size();

    // Symbol edges (class, target) and epsilon edges of each state, stored contiguously
    std::vector<std::int32_t> edge_start(u(n) + 1, 0), edge_class, edge_target;
    for (std::int32_t s = 0; s < n; ++s) {
        for (const auto& t : states_[u(s)].out) {
            classes.for_each_class(t.labels, [&](std::int32_t c) {
                edge_class.push_back(c);
                edge_target.push_back(t.target);
            });
        }
        edge_start[u(s) + 1] = static_cast<std::int32_t>(edge_class.size());
    }
    // Only states having symbol transitions or accepting matter in a subset
    std::vector<char> relevant(u(n));
    for (std::int32_t s = 0; s < n; ++s)
        relevant[u(s)] = states_[u(s)].accept || edge_start[u(s) + 1] > edge_start[u(s)];

    // Epsilon closure of a set of states, keeping the relevant ones, sorted
    std::vector<std::uint32_t> seen(u(n), 0);
    std::uint32_t generation = 0;
    std::vector<std::int32_t> stack, closure;
    auto close = [&](std::span<const std::int32_t> from) {
        ++generation;
        closure.clear();
        for (auto s : from) {
            if (seen[u(s)] != generation) {
                seen[u(s)] = generation;
                stack.push_back(s);
            }
        }
        while (!stack.empty()) {
            auto s = stack.back();
            stack.pop_back();
            if (relevant[u(s)])
                closure.push_back(s);
            for (auto t : states_[u(s)].eps) {
                if (seen[u(t)] != generation) {
                    seen[u(t)] = generation;
                    stack.push_back(t);
                }
            }
        }
        std::ranges::sort(closure);
    };

    automaton dfa;
    detail::sequence_numbering subsets;
    bool added = false;
    close(std::span(&initial_, 1));
    subsets.insert(closure, added);
    dfa.set_initial(dfa.add_state());

    // Targets reached by each class from the current subset
    std::vector<std::vector<std::int32_t>> buckets(u(k));
    std::vector<std::int32_t> touched;
    // Labels gathered towards each DFA target, in increasing class order
    std::vector<std::int32_t> slot_of(16, -1);
    std::vector<std::pair<state_id, interval_set>> by_target;
    for (std::int32_t v = 0; v < static_cast<std::int32_t>(subsets.size()); ++v) {
        touched.clear();
        bool accept = false;
        for (auto s : subsets.at(v)) {
            accept = accept || states_[u(s)].accept;
            for (auto i = edge_start[u(s)]; i < edge_start[u(s) + 1]; ++i) {
                auto c = edge_class[u(i)];
                if (buckets[u(c)].empty())
                    touched.push_back(c);
                buckets[u(c)].push_back(edge_target[u(i)]);
            }
        }
        dfa.states_[u(v)].accept = accept;
        std::ranges::sort(touched);
        for (auto c : touched) {
            close(buckets[u(c)]);
            buckets[u(c)].clear();
            if (closure.empty())
                continue;
            auto target = subsets.insert(closure, added);
            if (added)
                dfa.add_state();
            if (slot_of.size() <= u(target))
                slot_of.resize(2 * u(target) + 1, -1);
            if (slot_of[u(target)] < 0) {
                slot_of[u(target)] = static_cast<std::int32_t>(by_target.size());
                by_target.emplace_back(target, interval_set{});
            }
            classes.append_to(by_target[u(slot_of[u(target)])].second, c);
        }
        auto& out = dfa.states_[u(v)].out;
        out.reserve(by_target.size());
        for (auto& [target, labels] : by_target) {
            slot_of[u(target)] = -1;
            out.push_back({target, std::move(labels)});
        }
        by_target.clear();
    }
    return dfa;
}

}  // namespace autolib
