// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Graphviz representations of graphs and of their weightings.

#include <cmath>
#include <cstdio>
#include <string_view>
#include <cstdlib>
#include <functional>
#include <sstream>
#include <unordered_set>

#include "weighted/common.hpp"

namespace autolib {

using detail::u;

namespace {

constexpr const char* highlight_color = "#d6336c";

/// The shortest decimal representation of a double that reads back as it, without exponent unless the
/// magnitude is very large or very small.
std::string number(double x) {
    if (x == 0)
        return "0";
    bool plain = std::fabs(x) >= 1e-5 && std::fabs(x) < 1e15;
    char buffer[32];
    for (int precision = 1; precision <= 17; ++precision) {
        std::snprintf(buffer, sizeof buffer, "%.*g", precision, x);
        bool exponent = std::string_view(buffer).find('e') != std::string_view::npos;
        if (std::strtod(buffer, nullptr) == x && !(plain && exponent))
            break;
    }
    return buffer;
}

/// A vector of costs : (1, 2.5).
std::string vector_text(std::span<const double> v) {
    std::string out = "(";
    for (std::size_t i = 0; i < v.size(); ++i)
        out += (i ? ", " : "") + number(v[i]);
    return out + ")";
}

/// The Graphviz description of a graph : `edge_cost(e)` is the cost written after the symbols of an edge,
/// `final_cost(s)` the cost written under an accepting state, empty when there is none; the edges of `highlight`
/// and their states are drawn in colour.
std::string graph_dot(const detail::graph_data& g, const std::function<std::string(edge_id)>& edge_cost,
                      const std::function<std::string(state_id)>& final_cost, const std::string& initial_cost,
                      std::span<const edge_id> highlight) {
    std::unordered_set<edge_id> marked(highlight.begin(), highlight.end());
    std::unordered_set<state_id> visited;
    for (edge_id e : highlight) {
        g.check_edge(e);
        visited.insert(g.source[u(e)]);
        visited.insert(g.target[u(e)]);
    }
    if (!highlight.empty() || g.initial != no_state)
        visited.insert(g.initial);
    std::ostringstream out;
    out << "digraph automaton {\n  rankdir = LR;\n  node [fontname=\"Helvetica\"];\n  edge [fontname=\"Helvetica\"];\n";
    if (g.initial != no_state) {
        out << "  start [shape=point];\n  start -> " << g.initial;
        std::string label = initial_cost.empty() ? "" : "/ " + initial_cost;
        if (!label.empty() || !highlight.empty()) {
            out << " [";
            if (!label.empty())
                out << "label=\"" << label << "\"";
            if (!highlight.empty())
                out << (label.empty() ? "" : ", ") << "color=\"" << highlight_color << "\", penwidth=2.5";
            out << "]";
        }
        out << ";\n";
    }
    for (std::size_t s = 0; s < g.states(); ++s) {
        auto state = static_cast<state_id>(s);
        out << "  " << s << " [shape=" << (g.accept[s] ? "doublecircle" : "circle");
        std::string final_text = g.accept[s] ? final_cost(state) : "";
        if (!final_text.empty())
            out << ", label=\"" << s << "\\n/ " << final_text << "\"";
        if (!highlight.empty() && visited.contains(state))
            out << ", color=\"" << highlight_color << "\", penwidth=2";
        out << "];\n";
    }
    for (std::size_t e = 0; e < g.edges(); ++e) {
        auto id = static_cast<edge_id>(e);
        std::string label = g.epsilon[e] ? "&epsilon;" : g.labels(id).to_string();
        std::string cost = edge_cost(id);
        if (!cost.empty())
            label += " / " + cost;
        out << "  " << g.source[e] << " -> " << g.target[e] << " [label=\"" << label << "\"";
        if (g.epsilon[e])
            out << ", style=dashed";
        if (marked.contains(id))
            out << ", color=\"" << highlight_color << "\", fontcolor=\"" << highlight_color << "\", penwidth=2.5";
        out << "];\n";
    }
    out << "}\n";
    return out.str();
}

}  // namespace

std::string automaton_graph::to_dot(std::span<const edge_id> highlight) const {
    return graph_dot(
        *data_, [](edge_id) { return std::string(); }, [](state_id) { return std::string(); }, "", highlight);
}

std::string weighted_automaton::to_dot(std::span<const edge_id> highlight) const {
    const auto& w = *weights_;
    return graph_dot(
        detail::data(graph_), [&w](edge_id e) { return number(w.edges[u(e)]); },
        [&w](state_id s) { return w.finals[u(s)] == 0 ? std::string() : number(w.finals[u(s)]); },
        w.initial == 0 ? "" : number(w.initial), highlight);
}

std::string multi_weighted_automaton::to_dot(std::span<const edge_id> highlight) const {
    auto zero = [](std::span<const double> v) { return std::ranges::all_of(v, [](double x) { return x == 0; }); };
    return graph_dot(
        detail::data(graph_), [this](edge_id e) { return vector_text(weights(e)); },
        [&](state_id s) { return zero(final_weights(s)) ? std::string() : vector_text(final_weights(s)); },
        zero(initial_weights()) ? "" : vector_text(initial_weights()), highlight);
}

}  // namespace autolib
