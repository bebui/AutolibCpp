// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "autolib/words.hpp"

#include <limits>
#include <stdexcept>

namespace autolib {

namespace {
std::size_t u(std::int64_t i) { return static_cast<std::size_t>(i); }
constexpr std::size_t before_first = std::numeric_limits<std::size_t>::max();
}  // namespace

struct word_range::iterator::shared {
    compiled_dfa dfa;
    int max_length;
    /// can_accept[l][s] : a word of length exactly l is accepted from s
    std::vector<std::vector<char>> can_accept;

    [[nodiscard]] bool accepts_from(int length, state_id s) const {
        return s != no_state && can_accept[u(length)][u(s)];
    }
};

word_range::word_range(const automaton& a, int max_length) {
    if (max_length < 0)
        throw std::invalid_argument("Negative length: " + std::to_string(max_length));
    auto data = std::make_shared<iterator::shared>(iterator::shared{compiled_dfa(a, false), max_length, {}});
    const auto& dfa = data->dfa;
    auto n = dfa.state_count();
    data->can_accept.assign(u(max_length) + 1, std::vector<char>(n, 0));
    for (std::size_t s = 0; s < n; ++s)
        data->can_accept[0][s] = dfa.is_accept(static_cast<state_id>(s));
    for (std::size_t l = 1; l <= u(max_length); ++l)
        for (std::size_t s = 0; s < n; ++s)
            for (const auto& e : dfa.edges(static_cast<state_id>(s)))
                if (data->can_accept[l - 1][u(e.target)]) {
                    data->can_accept[l][s] = 1;
                    break;
                }
    data_ = std::move(data);
}

word_range::iterator::iterator(std::shared_ptr<const shared> data) : data_(std::move(data)), done_(false) {
    if (next_length())
        advance(false);
    else
        done_ = true;
}

bool word_range::iterator::next_length() {
    state_id init = data_->dfa.initial();
    do {
        ++length_;
    } while (length_ <= data_->max_length && !data_->accepts_from(length_, init));
    if (length_ > data_->max_length)
        return false;
    states_.assign(u(length_) + 1, no_state);
    edges_.assign(u(length_), before_first);
    word_.assign(u(length_), 0);
    states_[0] = init;
    depth_ = 0;
    return true;
}

bool word_range::iterator::next_symbol() {
    auto edges = data_->dfa.edges(states_[u(depth_)]);
    auto& e = edges_[u(depth_)];
    auto& s = word_[u(depth_)];
    if (e != before_first && s < edges[e].max) {
        ++s;
        return true;
    }
    int remaining = length_ - depth_ - 1;
    for (auto next = e == before_first ? 0 : e + 1; next < edges.size(); ++next) {
        if (data_->accepts_from(remaining, edges[next].target)) {
            e = next;
            s = edges[next].min;
            return true;
        }
    }
    return false;
}

void word_range::iterator::advance(bool resume) {
    bool descending = !resume;
    while (true) {
        if (descending) {
            if (depth_ == length_)
                return;  // word_ holds the next word
            edges_[u(depth_)] = before_first;
        }
        // Next symbol at the current depth, or backtracking
        if (depth_ == length_ || !next_symbol()) {
            if (depth_ == 0) {
                if (!next_length()) {
                    done_ = true;
                    return;
                }
                descending = true;
                continue;
            }
            --depth_;
            descending = false;
            continue;
        }
        states_[u(depth_) + 1] = data_->dfa.edges(states_[u(depth_)])[edges_[u(depth_)]].target;
        ++depth_;
        descending = true;
    }
}

}  // namespace autolib
