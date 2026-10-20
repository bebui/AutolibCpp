// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <algorithm>

#include "autolib/automaton.hpp"
#include "detail/access.hpp"
#include "detail/partition.hpp"
#include "detail/reachability.hpp"
#include "detail/symbol_classes.hpp"

namespace autolib {

namespace {

std::size_t u(std::int32_t i) { return static_cast<std::size_t>(i); }

/// Minimization of a deterministic automaton by the partition refinement of Valmari and Lehtinen,
/// in O(m log n) where m counts the transitions once split into symbol classes.
automaton minimize_partition(const automaton& dfa) {
    auto n_all = static_cast<std::int32_t>(dfa.state_count());
    state_id init = dfa.initial();

    // Transitions as flat arrays, read once in state order, then everything works on these arrays
    std::vector<std::int32_t> out_start(u(n_all) + 1, 0), out_target;
    std::vector<const interval_set*> out_labels;
    for (std::int32_t s = 0; s < n_all; ++s) {
        for (const auto& t : dfa.transitions(s)) {
            out_target.push_back(t.target);
            out_labels.push_back(&t.labels);
        }
        out_start[u(s) + 1] = static_cast<std::int32_t>(out_target.size());
    }

    // Live states : reachable from the initial state and leading to an accepting state, numbered from 0 to n-1
    std::vector<char> reached(u(n_all), 0);
    std::vector<std::int32_t> work(u(n_all));
    reached[u(init)] = 1;
    work[0] = init;
    detail::propagate<std::int32_t, std::int32_t>(out_start, {}, out_target, reached, work, 1);
    detail::predecessors preds(u(n_all), [&](std::size_t s, auto&& f) {
        if (reached[s])
            for (auto e = out_start[s]; e < out_start[s + 1]; ++e)
                f(out_target[u(e)]);
    });
    std::vector<char> useful(u(n_all), 0);
    std::size_t accepting = 0;
    for (std::int32_t s = 0; s < n_all; ++s) {
        if (reached[u(s)] && dfa.is_accept(s)) {
            useful[u(s)] = 1;
            work[accepting++] = s;
        }
    }
    detail::propagate<std::uint32_t, std::int32_t>(preds.start, {}, preds.sources, useful, work, accepting);
    if (!useful[u(init)]) {
        automaton out;
        out.set_initial(out.add_state());
        return out;
    }
    std::vector<std::int32_t> live(u(n_all), -1), states;
    for (std::int32_t s = 0; s < n_all; ++s) {
        if (useful[u(s)]) {
            live[u(s)] = static_cast<std::int32_t>(states.size());
            states.push_back(s);
        }
    }
    auto n = static_cast<std::int32_t>(states.size());

    // Transitions (tail, label, head) between live states, one per symbol class
    detail::symbol_classes classes([&](auto&& f) {
        for (auto s : states)
            for (auto e = out_start[u(s)]; e < out_start[u(s) + 1]; ++e)
                if (live[u(out_target[u(e)])] >= 0)
                    f(*out_labels[u(e)]);
    });
    auto k = classes.size();
    std::vector<std::int32_t> tail, label, head;
    tail.reserve(out_target.size());
    label.reserve(out_target.size());
    head.reserve(out_target.size());
    for (auto s : states) {
        for (auto e = out_start[u(s)]; e < out_start[u(s) + 1]; ++e) {
            auto target = live[u(out_target[u(e)])];
            if (target < 0)
                continue;
            classes.for_each_class(*out_labels[u(e)], [&](std::int32_t c) {
                tail.push_back(live[u(s)]);
                label.push_back(c);
                head.push_back(target);
            });
        }
    }
    auto m = static_cast<std::int32_t>(tail.size());

    // Blocks : partition of the states, initially accepting / non accepting
    detail::refinable_partition blocks(n);
    for (auto s : states)
        if (dfa.is_accept(s))
            blocks.mark(live[u(s)]);
    blocks.split();
    // Cords : partition of the transitions by label
    std::vector<std::int32_t> by_label(u(k) + 1, 0), order(u(m));
    for (auto c : label)
        ++by_label[u(c) + 1];
    for (std::int32_t c = 0; c < k; ++c)
        by_label[u(c) + 1] += by_label[u(c)];
    {
        std::vector<std::int32_t> fill(by_label.begin(), by_label.end() - 1);
        for (std::int32_t i = 0; i < m; ++i)
            order[u(fill[u(label[u(i)])]++)] = i;
    }
    detail::refinable_partition cords(order, by_label);
    // Transitions grouped by head
    std::vector<std::int32_t> into_start(u(n) + 1, 0), into(u(m));
    for (auto h : head)
        ++into_start[u(h) + 1];
    for (std::int32_t q = 0; q < n; ++q)
        into_start[u(q) + 1] += into_start[u(q)];
    {
        std::vector<std::int32_t> fill(into_start.begin(), into_start.end() - 1);
        for (std::int32_t i = 0; i < m; ++i)
            into[u(fill[u(head[u(i)])]++)] = i;
    }
    // Every cord splits the blocks, every block except the first one splits the cords
    std::int32_t b = 1, c = 0;
    while (c < cords.count()) {
        for (auto i = cords.first(c); i < cords.past(c); ++i)
            blocks.mark(tail[u(cords.element(i))]);
        blocks.split();
        ++c;
        for (; b < blocks.count(); ++b) {
            for (auto i = blocks.first(b); i < blocks.past(b); ++i) {
                auto q = blocks.element(i);
                for (auto j = into_start[u(q)]; j < into_start[u(q) + 1]; ++j)
                    cords.mark(into[u(j)]);
            }
            cords.split();
        }
    }

    // Quotient automaton, blocks numbered in breadth first order
    automaton out;
    detail::access::reserve(out, u(blocks.count()));
    std::vector<state_id> block_state(u(blocks.count()), no_state);
    std::vector<std::int32_t> queue;
    std::vector<std::int32_t> slot_of(u(blocks.count()), -1);
    std::vector<std::pair<state_id, interval_set>> by_target;
    auto init_block = blocks.set_of(live[u(init)]);
    block_state[u(init_block)] = out.add_state();
    out.set_initial(block_state[u(init_block)]);
    queue.push_back(init_block);
    for (std::size_t qi = 0; qi < queue.size(); ++qi) {
        auto block = queue[qi];
        auto representative = states[u(blocks.element(blocks.first(block)))];
        state_id from = block_state[u(block)];
        out.set_accept(from, dfa.is_accept(representative));
        for (const auto& t : dfa.transitions(representative)) {
            if (live[u(t.target)] < 0)
                continue;
            auto target = blocks.set_of(live[u(t.target)]);
            if (block_state[u(target)] == no_state) {
                block_state[u(target)] = out.add_state();
                queue.push_back(target);
            }
            if (slot_of[u(target)] < 0) {
                slot_of[u(target)] = static_cast<std::int32_t>(by_target.size());
                by_target.emplace_back(block_state[u(target)], t.labels);
            } else {
                by_target[u(slot_of[u(target)])].second.insert(t.labels);
            }
        }
        for (auto& [target, labels] : by_target)
            detail::access::add(out, from, target, std::move(labels));
        for (const auto& t : dfa.transitions(representative))
            if (live[u(t.target)] >= 0)
                slot_of[u(blocks.set_of(live[u(t.target)]))] = -1;
        by_target.clear();
    }
    return out;
}

automaton minimize_brzozowski(const automaton& a) {
    automaton out = a.reverse().determinize().reverse().determinize();
    out.trim();
    return out;
}

}  // namespace

automaton automaton::minimize(minimization algorithm) const {
    if (initial_ == no_state)
        return {};
    if (algorithm == minimization::brzozowski)
        return minimize_brzozowski(*this);
    return minimize_partition(deterministic_ ? *this : determinize());
}

}  // namespace autolib
