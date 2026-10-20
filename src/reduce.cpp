// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <algorithm>
#include <numeric>

#include "autolib/automaton.hpp"
#include "detail/access.hpp"
#include "detail/algorithms.hpp"
#include "detail/partition.hpp"
#include "detail/symbol_classes.hpp"

namespace autolib {

namespace {

std::size_t u(std::int32_t i) { return static_cast<std::size_t>(i); }

/// Gathers the labels of the transitions of a state by target, then adds one transition per target.
class transition_merger {
public:
    explicit transition_merger(std::size_t states) : slot_of_(states, -1) {}

    void add(state_id target, const interval_set& labels) {
        auto& slot = slot_of_[u(target)];
        if (slot < 0) {
            slot = static_cast<std::int32_t>(pending_.size());
            pending_.emplace_back(target, labels);
        } else {
            pending_[u(slot)].second.insert(labels);
        }
    }
    void flush(automaton& out, state_id from) {
        for (auto& [target, labels] : pending_) {
            slot_of_[u(target)] = -1;
            detail::access::add(out, from, target, std::move(labels));
        }
        pending_.clear();
    }

private:
    std::vector<std::int32_t> slot_of_;
    std::vector<std::pair<state_id, interval_set>> pending_;
};

}  // namespace

automaton automaton::remove_epsilon() const {
    automaton out;
    if (initial_ == no_state)
        return out;
    auto n = static_cast<std::int32_t>(states_.size());
    // A state for the initial state and for every target of a symbol transition
    std::vector<state_id> copy(u(n), no_state);
    copy[u(initial_)] = out.add_state();
    out.set_initial(copy[u(initial_)]);
    for (const auto& s : states_)
        for (const auto& t : s.out)
            if (copy[u(t.target)] == no_state)
                copy[u(t.target)] = out.add_state();
    std::vector<std::uint32_t> seen(u(n), 0);
    std::uint32_t generation = 0;
    std::vector<std::int32_t> stack;
    transition_merger merger(out.state_count());
    for (std::int32_t i = 0; i < n; ++i) {
        if (copy[u(i)] == no_state)
            continue;
        state_id from = copy[u(i)];
        // Every symbol transition of the epsilon closure of the state leaves its copy
        ++generation;
        seen[u(i)] = generation;
        stack.push_back(i);
        while (!stack.empty()) {
            const auto& s = states_[u(stack.back())];
            stack.pop_back();
            if (s.accept)
                out.set_accept(from);
            for (const auto& t : s.out)
                merger.add(copy[u(t.target)], t.labels);
            for (auto t : s.eps) {
                if (seen[u(t)] != generation) {
                    seen[u(t)] = generation;
                    stack.push_back(t);
                }
            }
        }
        merger.flush(out, from);
    }
    detail::access::update_determinism(out);
    out.trim();
    return out;
}

automaton automaton::reduce() const {
    automaton a = remove_epsilon();
    auto n = static_cast<std::int32_t>(a.state_count());
    if (n == 0)
        return a;
    detail::symbol_classes classes([&](auto&& f) {
        for (state_id s = 0; s < n; ++s)
            for (const auto& t : a.transitions(s))
                f(t.labels);
    });
    // Transitions (source, class, target)
    std::vector<std::int32_t> src, lab, dst;
    std::vector<char> accept(u(n));
    for (state_id s = 0; s < n; ++s) {
        accept[u(s)] = a.is_accept(s);
        for (const auto& t : a.transitions(s)) {
            classes.for_each_class(t.labels, [&](std::int32_t c) {
                src.push_back(s);
                lab.push_back(c);
                dst.push_back(t.target);
            });
        }
    }
    auto block = detail::coarsest_bisimulation(n, src, lab, dst, classes.size(), accept);
    auto blocks = *std::ranges::max_element(block) + 1;
    if (blocks == n)
        return a;
    // Quotient automaton
    automaton out;
    for (std::int32_t b = 0; b < blocks; ++b)
        out.add_state();
    std::vector<char> done(u(blocks), 0);
    transition_merger merger(u(blocks));
    for (state_id s = 0; s < n; ++s) {
        auto b = block[u(s)];
        if (done[u(b)])
            continue;
        done[u(b)] = 1;
        out.set_accept(b, a.is_accept(s));
        for (const auto& t : a.transitions(s))
            merger.add(block[u(t.target)], t.labels);
        merger.flush(out, b);
    }
    out.set_initial(block[u(a.initial())]);
    detail::access::update_determinism(out);
    out.trim();
    return out;
}

// ---------------------------------------------------------------------- coarsest bisimulation

namespace detail {

namespace {

/// Compound blocks of Paige and Tarjan : unions of blocks with respect to which the partition is stable.
class compounds {
public:
    explicit compounds(std::int32_t n) : compound_of_(u(n) + 1, 0), position_(u(n) + 1, 0), members_(1), queued_(1, 0) {}

    void add_block(std::int32_t compound, std::int32_t b) {
        compound_of_[u(b)] = compound;
        auto& list = members_[u(compound)];
        position_[u(b)] = static_cast<std::int32_t>(list.size());
        list.push_back(b);
        if (list.size() >= 2 && !queued_[u(compound)]) {
            queued_[u(compound)] = 1;
            non_trivial_.push_back(compound);
        }
    }
    [[nodiscard]] bool has_non_trivial() const noexcept { return !non_trivial_.empty(); }
    [[nodiscard]] std::int32_t compound_of(std::int32_t b) const noexcept { return compound_of_[u(b)]; }

    /// Takes the smaller of the first two blocks out of a non-trivial compound block, into its own compound block.
    std::int32_t extract_small_block(const refinable_partition& blocks) {
        auto compound = non_trivial_.back();
        auto& list = members_[u(compound)];
        auto b0 = list[0], b1 = list[1];
        auto b = blocks.size(b0) <= blocks.size(b1) ? b0 : b1;
        auto last = list.back();
        list.pop_back();
        if (last != b) {
            list[u(position_[u(b)])] = last;
            position_[u(last)] = position_[u(b)];
        }
        if (list.size() < 2) {
            queued_[u(compound)] = 0;
            non_trivial_.pop_back();
        }
        members_.push_back({b});
        queued_.push_back(0);
        compound_of_[u(b)] = static_cast<std::int32_t>(members_.size()) - 1;
        position_[u(b)] = 0;
        return b;
    }

private:
    std::vector<std::int32_t> compound_of_, position_;
    std::vector<std::vector<std::int32_t>> members_;
    std::vector<char> queued_;
    std::vector<std::int32_t> non_trivial_;
};

void split_and_record(refinable_partition& blocks, compounds& comp) {
    auto before = blocks.count();
    blocks.split();
    for (auto b = before; b < blocks.count(); ++b)
        comp.add_block(comp.compound_of(blocks.parent(b)), b);
}

}  // namespace

std::vector<std::int32_t> coarsest_bisimulation(std::int32_t n, std::span<const std::int32_t> src,
                                                std::span<const std::int32_t> lab, std::span<const std::int32_t> dst,
                                                std::int32_t labels, const std::vector<char>& accept) {
    auto m = static_cast<std::int32_t>(src.size());
    refinable_partition blocks(n);
    for (std::int32_t s = 0; s < n; ++s)
        if (accept[u(s)])
            blocks.mark(s);
    blocks.split();
    // Counters (source, label, compound block) : count[counter_of[t]] transitions of the source of t, with the label
    // of t, go into the compound block containing the target of t. Initially a single compound block
    std::vector<std::int32_t> counter_of(u(m)), count, by_source_label(u(m));
    std::iota(by_source_label.begin(), by_source_label.end(), 0);
    std::ranges::sort(by_source_label, [&](auto x, auto y) {
        return src[u(x)] != src[u(y)] ? src[u(x)] < src[u(y)] : lab[u(x)] < lab[u(y)];
    });
    for (std::int32_t i = 0; i < m; ++i) {
        auto t = by_source_label[u(i)];
        if (i == 0 || src[u(t)] != src[u(by_source_label[u(i) - 1])] || lab[u(t)] != lab[u(by_source_label[u(i) - 1])])
            count.push_back(0);
        counter_of[u(t)] = static_cast<std::int32_t>(count.size()) - 1;
        ++count.back();
    }
    // Incoming transitions of each state
    std::vector<std::int32_t> in_start(u(n) + 1, 0), into(u(m));
    for (auto t : dst)
        ++in_start[u(t) + 1];
    for (std::int32_t q = 0; q < n; ++q)
        in_start[u(q) + 1] += in_start[u(q)];
    {
        std::vector<std::int32_t> fill(in_start.begin(), in_start.end() - 1);
        for (std::int32_t t = 0; t < m; ++t)
            into[u(fill[u(dst[u(t)])]++)] = t;
    }
    // The initial blocks must be stable with respect to the single compound block : split by "has an a-transition"
    std::vector<std::uint32_t> stamp(u(n), 0);
    std::uint32_t generation = 0;
    std::vector<std::vector<std::int32_t>> sources_by_label(u(labels));
    for (std::int32_t t = 0; t < m; ++t)
        sources_by_label[u(lab[u(t)])].push_back(src[u(t)]);
    for (const auto& sources : sources_by_label) {
        ++generation;
        for (auto x : sources) {
            if (stamp[u(x)] != generation) {
                stamp[u(x)] = generation;
                blocks.mark(x);
            }
        }
        blocks.split();
    }
    sources_by_label.clear();
    compounds comp(n);
    for (std::int32_t b = 0; b < blocks.count(); ++b)
        comp.add_block(0, b);
    std::vector<std::int32_t> new_count(u(n)), new_counter(u(n)), group, sorted, label_count(u(labels) + 1);
    while (comp.has_non_trivial()) {
        auto b = comp.extract_small_block(blocks);
        // Snapshot of the transitions into B, grouped by label
        group.clear();
        for (auto i = blocks.first(b); i < blocks.past(b); ++i) {
            auto y = blocks.element(i);
            for (auto j = in_start[u(y)]; j < in_start[u(y) + 1]; ++j)
                group.push_back(into[u(j)]);
        }
        if (group.size() > 1) {
            std::ranges::fill(label_count, 0);
            for (auto t : group)
                ++label_count[u(lab[u(t)]) + 1];
            for (std::int32_t a = 0; a < labels; ++a)
                label_count[u(a) + 1] += label_count[u(a)];
            sorted.resize(group.size());
            for (auto t : group)
                sorted[u(label_count[u(lab[u(t)])]++)] = t;
            std::swap(group, sorted);
        }
        std::size_t from = 0;
        while (from < group.size()) {
            auto a = lab[u(group[from])];
            auto to = from;
            while (to < group.size() && lab[u(group[to])] == a)
                ++to;
            // Split by the predecessors of B with label a, counting their transitions into B
            ++generation;
            for (auto i = from; i < to; ++i) {
                auto x = src[u(group[i])];
                if (stamp[u(x)] != generation) {
                    stamp[u(x)] = generation;
                    new_count[u(x)] = 0;
                    blocks.mark(x);
                }
                ++new_count[u(x)];
            }
            split_and_record(blocks, comp);
            // Split by the predecessors whose a-transitions into the compound block all go into B
            ++generation;
            for (auto i = from; i < to; ++i) {
                auto x = src[u(group[i])];
                if (stamp[u(x)] != generation) {
                    stamp[u(x)] = generation;
                    if (new_count[u(x)] == count[u(counter_of[u(group[i])])])
                        blocks.mark(x);
                }
            }
            split_and_record(blocks, comp);
            // The old counters now count the transitions into the rest of the compound block
            ++generation;
            for (auto i = from; i < to; ++i) {
                auto t = group[i];
                auto x = src[u(t)];
                --count[u(counter_of[u(t)])];
                if (stamp[u(x)] != generation) {
                    stamp[u(x)] = generation;
                    new_counter[u(x)] = static_cast<std::int32_t>(count.size());
                    count.push_back(new_count[u(x)]);
                }
                counter_of[u(t)] = new_counter[u(x)];
            }
            from = to;
        }
    }
    return blocks.sets();
}

}  // namespace detail

}  // namespace autolib
