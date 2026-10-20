// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include "autolib/regex.hpp"

#include <charconv>
#include <limits>

#include "detail/access.hpp"

namespace autolib {

// ---------------------------------------------------------------------- parsing

class regex::parser {
public:
    parser(regex& r, std::string_view text) : r_(r), text_(text) {}

    int parse() {
        int root = alternation();
        if (more())
            fail(std::string("Unexpected '") + text_[pos_] + "'");
        return root;
    }

private:
    [[noreturn]] void fail(const std::string& message) const {
        throw regex_error(message + " at position " + std::to_string(pos_) + " in \"" + std::string(text_) + "\"", pos_);
    }
    [[nodiscard]] bool more() const noexcept { return pos_ < text_.size(); }
    [[nodiscard]] bool peek(char c) const noexcept { return more() && text_[pos_] == c; }
    void eat(char c) {
        if (!more())
            fail(std::string("Expected '") + c + "' but reached the end");
        if (text_[pos_] != c)
            fail(std::string("Expected '") + c + "' but found '" + text_[pos_] + "'");
        ++pos_;
    }

    int nary(kind type, std::vector<int> children) {
        if (children.size() == 1)
            return children[0];
        return r_.add({type, std::move(children), {}, 0, -1});
    }

    int alternation() {
        std::vector<int> parts{intersection()};
        while (peek('|')) {
            eat('|');
            parts.push_back(intersection());
        }
        return nary(kind::alternation, std::move(parts));
    }

    int intersection() {
        std::vector<int> parts{sequence()};
        while (peek('&')) {
            eat('&');
            parts.push_back(sequence());
        }
        return nary(kind::intersection, std::move(parts));
    }

    int sequence() {
        std::vector<int> parts;
        while (more() && !peek(')') && !peek('|') && !peek('&'))
            parts.push_back(complement());
        if (parts.empty())
            return r_.add({kind::empty_word, {}, {}, 0, -1});
        return nary(kind::sequence, std::move(parts));
    }

    int complement() {
        if (peek('~')) {
            eat('~');
            return r_.add({kind::complement, {complement()}, {}, 0, -1});
        }
        return postfix();
    }

    int postfix() {
        int base = atom();
        while (more()) {
            char c = text_[pos_];
            if (c == '*' || c == '+' || c == '?') {
                ++pos_;
                base = r_.add({kind::repeat, {base}, {}, c == '+' ? 1 : 0, c == '?' ? 1 : -1});
            } else if (c == '{') {
                auto [min, max] = range();
                base = r_.add({kind::repeat, {base}, {}, min, max});
            } else {
                break;
            }
        }
        return base;
    }

    /// Reads {n}, {n,m}, {n,} or {,m}
    std::pair<int, int> range() {
        auto end = text_.find('}', pos_);
        if (end == std::string_view::npos)
            fail("Missing '}'");
        auto inside = text_.substr(pos_ + 1, end - pos_ - 1);
        auto bad = [&] { fail("Invalid repetition {" + std::string(inside) + "}"); };
        auto number = [&](std::string_view s) {
            int v = 0;
            auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
            if (s.empty() || ec != std::errc{} || ptr != s.data() + s.size() || v < 0)
                bad();
            return v;
        };
        int min = 0, max = 0;
        auto comma = inside.find(',');
        if (comma == std::string_view::npos) {
            min = max = number(inside);
        } else {
            auto low = inside.substr(0, comma), high = inside.substr(comma + 1);
            if (high.find(',') != std::string_view::npos || (low.empty() && high.empty()))
                bad();
            min = low.empty() ? 0 : number(low);
            max = high.empty() ? -1 : number(high);
            if (max >= 0 && max < min)
                fail("Invalid repetition {" + std::string(inside) + "}: " + std::to_string(max) + " < " + std::to_string(min));
        }
        pos_ = end + 1;
        return {min, max};
    }

    int atom() {
        if (!more())
            fail("Unexpected end of expression");
        char c = text_[pos_];
        if (c == '(') {
            eat('(');
            int inner = alternation();
            eat(')');
            return inner;
        }
        if (c == '.') {
            ++pos_;
            return r_.add({kind::symbols, {}, interval_set::all(), 0, -1});
        }
        if (c == '<') {
            auto end = text_.find('>', pos_);
            if (end == std::string_view::npos)
                fail("Missing '>'");
            auto digits = text_.substr(pos_ + 1, end - pos_ - 1);
            symbol value = 0;
            auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
            if (digits.empty() || ptr != digits.data() + digits.size() || (ec != std::errc{} && ec != std::errc::result_out_of_range))
                fail("Expected an integer between '<' and '>' but found \"" + std::string(digits) + "\"");
            if (ec == std::errc::result_out_of_range)
                fail("Integer out of range: " + std::string(digits));
            pos_ = end + 1;
            return r_.add({kind::symbols, {}, value, 0, -1});
        }
        if (c >= '0' && c <= '9') {
            ++pos_;
            return r_.add({kind::symbols, {}, symbol{c - '0'}, 0, -1});
        }
        fail(std::string("Unexpected character '") + c + "', allowed symbols are digits, <integer> and .");
    }

    regex& r_;
    std::string_view text_;
    std::size_t pos_ = 0;
};

regex::regex(std::string_view expression) {
    root_ = parser(*this, expression).parse();
}

int regex::add(node n) {
    nodes_.push_back(std::move(n));
    return static_cast<int>(nodes_.size()) - 1;
}

// ---------------------------------------------------------------------- Thompson construction

namespace {
/// Copies an automaton between start and a new end state; returns the end state.
state_id embed(automaton& a, state_id start, const automaton& copied) {
    state_id end = a.add_state();
    if (copied.initial() == no_state)
        return end;
    state_id offset = detail::access::embed(a, copied);
    for (state_id s = 0; s < static_cast<state_id>(copied.state_count()); ++s) {
        if (copied.is_accept(s)) {
            a.set_accept(offset + s, false);
            detail::access::add_epsilon(a, offset + s, end);
        }
    }
    detail::access::add_epsilon(a, start, offset + copied.initial());
    return end;
}
}  // namespace

automaton regex::nfa_of(int n) const {
    automaton a;
    state_id start = a.add_state();
    a.set_initial(start);
    a.set_accept(build(a, n, start));
    detail::access::update_determinism(a);
    return a;
}

automaton regex::to_nfa() const {
    return nfa_of(root_);
}

state_id regex::build(automaton& a, int index, state_id start) const {
    const node& n = nodes_[static_cast<std::size_t>(index)];
    switch (n.type) {
    case kind::empty_word:
        return start;
    case kind::symbols: {
        state_id end = a.add_state();
        detail::access::add(a, start, end, n.labels);
        return end;
    }
    case kind::sequence: {
        state_id end = start;
        for (int child : n.children)
            end = build(a, child, end);
        return end;
    }
    case kind::alternation: {
        state_id end = a.add_state();
        for (int child : n.children)
            detail::access::add_epsilon(a, build(a, child, start), end);
        return end;
    }
    case kind::intersection: {
        automaton product = nfa_of(n.children[0]);
        for (std::size_t i = 1; i < n.children.size(); ++i)
            product = product & nfa_of(n.children[i]);
        return embed(a, start, product);
    }
    case kind::complement:
        return embed(a, start, (~nfa_of(n.children[0])).minimize());
    case kind::repeat: {
        int child = n.children[0];
        auto star = [&](state_id from) {
            // The loop goes through a fresh state, so that it never adds paths to other expressions sharing `from`
            state_id loop = a.add_state();
            detail::access::add_epsilon(a, from, loop);
            detail::access::add_epsilon(a, build(a, child, loop), loop);
            return loop;
        };
        state_id current = start;
        for (int i = 0; i < n.min; ++i)
            current = build(a, child, current);
        if (n.max < 0)
            return star(current);
        if (n.max == n.min)
            return current;
        // Every state reached after min to max repetitions leads to the end
        state_id end = a.add_state();
        for (int i = n.min; i < n.max; ++i) {
            detail::access::add_epsilon(a, current, end);
            current = build(a, child, current);
        }
        detail::access::add_epsilon(a, current, end);
        return end;
    }
    }
    return start;
}

// ---------------------------------------------------------------------- printing

void regex::print(std::string& out, int index) const {
    const node& n = nodes_[static_cast<std::size_t>(index)];
    auto children = [&](const char* separator) {
        out += '(';
        for (std::size_t i = 0; i < n.children.size(); ++i) {
            if (i > 0)
                out += separator;
            print(out, n.children[i]);
        }
        out += ')';
    };
    switch (n.type) {
    case kind::empty_word:
        out += "()";
        break;
    case kind::symbols:
        if (n.labels == interval_set::all())
            out += '.';
        else if (symbol s = n.labels.min(); s >= 0 && s <= 9)
            out += static_cast<char>('0' + s);
        else
            out += '<' + std::to_string(s) + '>';
        break;
    case kind::sequence:
        for (int child : n.children)
            print(out, child);
        break;
    case kind::alternation: children("|"); break;
    case kind::intersection: children("&"); break;
    case kind::complement:
        out += "~(";
        print(out, n.children[0]);
        out += ')';
        break;
    case kind::repeat:
        out += '(';
        print(out, n.children[0]);
        out += ')';
        if (n.min == 0 && n.max < 0)
            out += '*';
        else if (n.min == 1 && n.max < 0)
            out += '+';
        else if (n.min == 0 && n.max == 1)
            out += '?';
        else if (n.max < 0)
            out += '{' + std::to_string(n.min) + ",}";
        else if (n.max == n.min)
            out += '{' + std::to_string(n.min) + '}';
        else
            out += '{' + std::to_string(n.min) + ',' + std::to_string(n.max) + '}';
        break;
    }
}

std::string regex::to_string() const {
    std::string out;
    print(out, root_);
    return out;
}

// ---------------------------------------------------------------------- automaton factories

automaton automaton::nfa_from_regex(std::string_view expression) {
    return regex(expression).to_nfa();
}

automaton automaton::from_regex(std::string_view expression) {
    return regex(expression).to_dfa();
}

}  // namespace autolib
