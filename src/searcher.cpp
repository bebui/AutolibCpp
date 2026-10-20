// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "autolib/searcher.hpp"

#include <memory>

namespace autolib {

searcher::searcher(const automaton& language)
    : forward_(language.minimize()), starts_((automaton::universal() + language.reverse()).minimize()) {}

std::optional<std::size_t> searcher::leftmost_start(std::span<const symbol> text, std::size_t from) const {
    state_id s = starts_.initial();
    std::optional<std::size_t> start;
    if (starts_.is_accept(s))
        start = text.size();
    for (std::size_t i = text.size(); i > from && s != no_state;) {
        --i;
        s = starts_.step(s, text[i]);
        if (starts_.is_accept(s))
            start = i;
    }
    return start;
}

std::size_t searcher::longest_end(std::span<const symbol> text, std::size_t start) const {
    state_id s = forward_.initial();
    std::size_t end = start;
    for (std::size_t i = start; i < text.size() && s != no_state; ++i) {
        s = forward_.step(s, text[i]);
        if (forward_.is_accept(s))
            end = i + 1;
    }
    return end;
}

std::optional<match> searcher::find(std::span<const symbol> text, std::size_t from) const {
    auto start = leftmost_start(text, from);
    if (!start)
        return std::nullopt;
    return match{*start, longest_end(text, *start)};
}

bool searcher::contains(std::span<const symbol> text) const {
    return leftmost_start(text, 0).has_value();
}

std::vector<match> searcher::find_all(std::span<const symbol> text) const {
    // Every possible start, computed in a single backward pass
    std::size_t n = text.size();
    auto starts = std::make_unique<bool[]>(n + 1);
    state_id s = starts_.initial();
    starts[n] = starts_.is_accept(s);
    for (std::size_t i = n; i > 0 && s != no_state;) {
        --i;
        s = starts_.step(s, text[i]);
        starts[i] = starts_.is_accept(s);
    }
    std::vector<match> matches;
    for (std::size_t i = 0; i <= text.size();) {
        if (!starts[i]) {
            ++i;
            continue;
        }
        std::size_t end = longest_end(text, i);
        matches.push_back({i, end});
        i = end > i ? end : i + 1;
    }
    return matches;
}

}  // namespace autolib
