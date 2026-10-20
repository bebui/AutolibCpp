// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "autolib/automaton.hpp"

#include <algorithm>
#include <ostream>
#include <sstream>
#include <stdexcept>

#include "detail/access.hpp"
#include "detail/reachability.hpp"
#include "detail/algorithms.hpp"

namespace autolib {

namespace {
std::size_t u(state_id s) { return static_cast<std::size_t>(s); }
}  // namespace

// ---------------------------------------------------------------------- elementary languages

automaton automaton::empty_word() {
    automaton a;
    a.set_initial(a.add_state(true));
    return a;
}

automaton automaton::symbols(const interval_set& set) {
    automaton a;
    state_id start = a.add_state();
    a.set_initial(start);
    if (!set.empty())
        a.add_transition(start, a.add_state(true), set);
    return a;
}

automaton automaton::universal() {
    automaton a;
    state_id s = a.add_state(true);
    a.set_initial(s);
    a.add_transition(s, s, interval_set::all());
    return a;
}

automaton automaton::of_word(std::span<const symbol> w) {
    automaton a;
    detail::access::reserve(a, w.size() + 1);
    state_id current = a.add_state();
    a.set_initial(current);
    for (symbol s : w) {
        state_id next = a.add_state();
        detail::access::add(a, current, next, s);
        current = next;
    }
    a.set_accept(current);
    return a;
}

// ---------------------------------------------------------------------- structure

std::size_t automaton::checked(state_id s) const {
    if (s < 0 || u(s) >= states_.size())
        throw std::out_of_range("No state " + std::to_string(s) + " in an automaton of " + std::to_string(states_.size()) +
                                " states");
    return u(s);
}

state_id automaton::add_state(bool accept) {
    states_.push_back({{}, {}, accept});
    return static_cast<state_id>(states_.size() - 1);
}

std::size_t automaton::transition_count() const noexcept {
    std::size_t total = 0;
    for (const auto& s : states_)
        total += s.out.size() + s.eps.size();
    return total;
}

void automaton::set_initial(state_id s) {
    initial_ = static_cast<state_id>(checked(s));
}

void automaton::add_transition(state_id from, state_id to, interval_set labels) {
    auto& out = states_[checked(from)].out;
    (void)checked(to);
    if (labels.empty())
        return;
    if (deterministic_)
        deterministic_ = std::ranges::none_of(out, [&](const transition& t) { return t.labels.intersects(labels); });
    // Labels towards the same target are merged
    for (auto& t : out) {
        if (t.target == to) {
            t.labels.insert(labels);
            return;
        }
    }
    out.push_back({to, std::move(labels)});
}

void automaton::add_epsilon(state_id from, state_id to) {
    auto& eps = states_[checked(from)].eps;
    (void)checked(to);
    if (std::ranges::find(eps, to) == eps.end())
        eps.push_back(to);
    deterministic_ = false;
}

state_id automaton::step(state_id from, symbol s) const {
    for (const auto& t : states_[checked(from)].out)
        if (t.labels.contains(s))
            return t.target;
    return no_state;
}

state_id automaton::embed(const automaton& other) {
    auto offset = static_cast<state_id>(states_.size());
    states_.reserve(states_.size() + other.states_.size());
    for (const auto& s : other.states_) {
        state_data copy{s.out, s.eps, s.accept};
        for (auto& t : copy.out)
            t.target += offset;
        for (auto& e : copy.eps)
            e += offset;
        states_.push_back(std::move(copy));
    }
    return offset;
}

void automaton::trim() {
    if (initial_ == no_state) {
        states_.clear();
        return;
    }
    std::size_t n = states_.size();
    // Forward reachability from the initial state
    std::vector<char> reached(n, 0);
    std::vector<state_id> stack{initial_};
    reached[u(initial_)] = 1;
    while (!stack.empty()) {
        state_id s = stack.back();
        stack.pop_back();
        auto visit = [&](state_id t) {
            if (!reached[u(t)]) {
                reached[u(t)] = 1;
                stack.push_back(t);
            }
        };
        for (const auto& t : states_[u(s)].out)
            visit(t.target);
        for (state_id t : states_[u(s)].eps)
            visit(t);
    }
    // Backward reachability from the reached accepting states, on compressed predecessor rows
    detail::predecessors preds(n, [&](std::size_t s, auto&& f) {
        if (!reached[s])
            return;
        for (const auto& t : states_[s].out)
            f(t.target);
        for (state_id t : states_[s].eps)
            f(t);
    });
    std::vector<char> useful(n, 0);
    std::vector<state_id> queue(n);
    std::size_t tail = 0;
    for (std::size_t s = 0; s < n; ++s) {
        if (reached[s] && states_[s].accept) {
            useful[s] = 1;
            queue[tail++] = static_cast<state_id>(s);
        }
    }
    detail::propagate<std::uint32_t, state_id>(preds.start, {}, preds.sources, useful, queue, tail);
    if (!useful[u(initial_)]) {
        states_.assign(1, state_data{});
        initial_ = 0;
        deterministic_ = true;
        return;
    }
    // Breadth first numbering of the useful states
    std::vector<state_id> number(n, no_state);
    std::vector<state_id> order{initial_};
    number[u(initial_)] = 0;
    for (std::size_t i = 0; i < order.size(); ++i) {
        const auto& s = states_[u(order[i])];
        auto visit = [&](state_id t) {
            if (useful[u(t)] && number[u(t)] == no_state) {
                number[u(t)] = static_cast<state_id>(order.size());
                order.push_back(t);
            }
        };
        for (const auto& t : s.out)
            visit(t.target);
        for (state_id t : s.eps)
            visit(t);
    }
    std::vector<state_data> kept;
    kept.reserve(order.size());
    for (state_id old : order) {
        auto& s = states_[u(old)];
        state_data fresh{{}, {}, s.accept};
        for (auto& t : s.out)
            if (number[u(t.target)] != no_state)
                fresh.out.push_back({number[u(t.target)], std::move(t.labels)});
        for (state_id t : s.eps)
            if (number[u(t)] != no_state)
                fresh.eps.push_back(number[u(t)]);
        kept.push_back(std::move(fresh));
    }
    states_ = std::move(kept);
    initial_ = 0;
}

// ---------------------------------------------------------------------- membership

bool automaton::run(std::span<const symbol> w) const {
    if (initial_ == no_state)
        return false;
    if (deterministic_) {
        state_id s = initial_;
        for (symbol c : w) {
            s = step(s, c);
            if (s == no_state)
                return false;
        }
        return states_[u(s)].accept;
    }
    // Simulation on sets of states, stamped by generation to avoid clearing them
    std::vector<std::uint32_t> stamp(states_.size(), 0);
    std::uint32_t generation = 1;
    std::vector<state_id> current, next, stack;
    auto close = [&](std::vector<state_id>& set) {
        stack.assign(set.begin(), set.end());
        while (!stack.empty()) {
            state_id s = stack.back();
            stack.pop_back();
            for (state_id t : states_[u(s)].eps) {
                if (stamp[u(t)] != generation) {
                    stamp[u(t)] = generation;
                    set.push_back(t);
                    stack.push_back(t);
                }
            }
        }
    };
    stamp[u(initial_)] = generation;
    current.push_back(initial_);
    close(current);
    for (symbol c : w) {
        ++generation;
        next.clear();
        for (state_id s : current) {
            for (const auto& t : states_[u(s)].out) {
                if (stamp[u(t.target)] != generation && t.labels.contains(c)) {
                    stamp[u(t.target)] = generation;
                    next.push_back(t.target);
                }
            }
        }
        if (next.empty())
            return false;
        close(next);
        std::swap(current, next);
    }
    return std::ranges::any_of(current, [&](state_id s) { return states_[u(s)].accept; });
}

// ---------------------------------------------------------------------- regular operations

namespace {
/// Minimal DFA when the operands were deterministic, as documented on the operators.
automaton finish(automaton a, bool deterministic_operands) {
    return deterministic_operands ? a.minimize() : a;
}
}  // namespace

automaton automaton::reverse() const {
    automaton out;
    if (initial_ == no_state)
        return out;
    detail::access::reserve(out, states_.size() + 1);
    for (std::size_t s = 0; s < states_.size(); ++s)
        out.add_state(static_cast<state_id>(s) == initial_);
    for (std::size_t s = 0; s < states_.size(); ++s) {
        for (const auto& t : states_[s].out)
            detail::access::add(out, t.target, static_cast<state_id>(s), t.labels);
        for (state_id t : states_[s].eps)
            detail::access::add_epsilon(out, t, static_cast<state_id>(s));
    }
    state_id start = out.add_state();
    out.set_initial(start);
    for (std::size_t s = 0; s < states_.size(); ++s)
        if (states_[s].accept)
            detail::access::add_epsilon(out, start, static_cast<state_id>(s));
    detail::access::set_deterministic(out, false);
    return out;
}

automaton automaton::concatenate(const automaton& other) const {
    if (initial_ == no_state || other.initial_ == no_state)
        return {};
    automaton out = *this;
    state_id offset = out.embed(other);
    for (std::size_t s = 0; s < states_.size(); ++s) {
        if (out.states_[s].accept) {
            out.states_[s].accept = false;
            out.states_[s].eps.push_back(offset + other.initial_);
        }
    }
    out.deterministic_ = false;
    return finish(std::move(out), deterministic_ && other.deterministic_);
}

automaton automaton::union_with(const automaton& other) const {
    if (initial_ == no_state)
        return other;
    if (other.initial_ == no_state)
        return *this;
    if (deterministic_ && other.deterministic_)
        return detail::product(*this, other, detail::product_kind::union_of).minimize();
    automaton out = *this;
    state_id offset = out.embed(other);
    state_id start = out.add_state();
    out.states_[u(start)].eps = {initial_, offset + other.initial_};
    out.initial_ = start;
    out.deterministic_ = false;
    return out;
}

automaton automaton::star() const {
    automaton out = *this;
    state_id start = out.add_state(true);
    if (initial_ != no_state) {
        out.states_[u(start)].eps.push_back(initial_);
        for (std::size_t s = 0; s < states_.size(); ++s)
            if (states_[s].accept)
                out.states_[s].eps.push_back(start);
    }
    out.initial_ = start;
    out.deterministic_ = false;
    return finish(std::move(out), deterministic_);
}

automaton automaton::plus() const {
    if (initial_ == no_state)
        return {};
    automaton out = *this;
    // The loop goes through a fresh state, so that it adds no path entering the initial state from elsewhere
    state_id loop = out.add_state();
    out.states_[u(loop)].eps.push_back(initial_);
    for (std::size_t s = 0; s < states_.size(); ++s)
        if (states_[s].accept)
            out.states_[s].eps.push_back(loop);
    out.deterministic_ = false;
    return finish(std::move(out), deterministic_);
}

automaton automaton::optional() const {
    automaton out = *this;
    state_id start = out.add_state(true);
    if (initial_ != no_state)
        out.states_[u(start)].eps.push_back(initial_);
    out.initial_ = start;
    out.deterministic_ = false;
    return finish(std::move(out), deterministic_);
}

automaton automaton::repeat(int min, int max) const {
    if (min < 0 || (max >= 0 && max < min))
        throw std::invalid_argument("Invalid repetition {" + std::to_string(min) + "," + std::to_string(max) + "}");
    automaton out = empty_word();
    for (int i = 0; i < min; ++i)
        out = out.concatenate(*this);
    if (max < 0)
        return out.concatenate(star());
    if (max > min) {
        // (L (L (... L?)?)?) : max - min optional repetitions
        automaton tail = optional();
        for (int i = min + 1; i < max; ++i)
            tail = (*this).concatenate(tail).optional();
        out = out.concatenate(tail);
    }
    return out;
}

// ---------------------------------------------------------------------- output

std::ostream& automaton::print(std::ostream& out) const {
    for (std::size_t s = 0; s < states_.size(); ++s) {
        for (const auto& t : states_[s].out)
            out << s << " -" << t.labels << "-> " << t.target << '\n';
        for (state_id t : states_[s].eps)
            out << s << " -eps-> " << t << '\n';
    }
    return out;
}

std::string automaton::to_string() const {
    std::ostringstream out;
    print(out);
    return out.str();
}

std::string automaton::to_dot() const {
    std::ostringstream out;
    out << "digraph automaton {\n  rankdir = LR;\n";
    if (initial_ != no_state)
        out << "  start [shape=point];\n  start -> " << initial_ << ";\n";
    for (std::size_t s = 0; s < states_.size(); ++s) {
        out << "  " << s << " [shape=" << (states_[s].accept ? "doublecircle" : "circle") << "];\n";
        for (const auto& t : states_[s].out)
            out << "  " << s << " -> " << t.target << " [label=\"" << t.labels << "\"];\n";
        for (state_id t : states_[s].eps)
            out << "  " << s << " -> " << t << " [label=\"&epsilon;\"];\n";
    }
    out << "}\n";
    return out.str();
}

}  // namespace autolib
