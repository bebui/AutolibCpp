// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Results of the weighted automata of the Java library fr.menana:automaton 4.0.0 on random automata, generated
// by tools/java-oracle/WeightedOracle.java : costs, k best paths and words, determinization and minimization.

#include <autolib/autolib.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <sstream>

using namespace autolib;

namespace {

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> out;
    std::string field;
    std::istringstream in(text);
    while (std::getline(in, field, separator))
        out.push_back(field);
    if (!text.empty() && text.back() == separator)
        out.emplace_back();
    return out;
}

double parse_double(const std::string& text) { return std::strtod(text.c_str(), nullptr); }

/// The name of the Java exception corresponding to the one thrown, or "ok".
std::string outcome(const std::function<void()>& f) {
    try {
        f();
        return "ok";
    } catch (const std::invalid_argument&) {
        return "E:IllegalArgumentException";
    } catch (const std::out_of_range&) {
        return "E:IndexOutOfBoundsException";
    } catch (const std::logic_error&) {
        return "E:IllegalStateException";
    } catch (const std::overflow_error&) {
        return "E:ArithmeticException";
    } catch (const std::range_error&) {
        return "E:ArithmeticException";
    }
}

weighted_automaton parse_automaton(const std::vector<std::string>& f) {
    weighted_automaton::builder b;
    int n = std::stoi(f[1]);
    for (int s = 0; s < n; ++s)
        b.add_state();
    b.set_initial(std::stoi(f[2]), parse_double(f[3]));
    for (const auto& final_state : split(f[4], ' ')) {
        auto parts = split(final_state, ':');
        b.set_accept(std::stoi(parts[0]), parse_double(parts[1]));
    }
    for (const auto& edge : split(f[5], ' ')) {
        auto arrow = edge.find('>'), colon = edge.find(':'), at = edge.find('@');
        int from = std::stoi(edge.substr(0, arrow)), to = std::stoi(edge.substr(arrow + 1, colon - arrow - 1));
        auto label = edge.substr(colon + 1, at - colon - 1);
        double w = parse_double(edge.substr(at + 1));
        if (label == "e") {
            b.add_epsilon(from, to, w);
            continue;
        }
        interval_set set;
        for (const auto& part : split(label, '+')) {
            auto dash = part.find('-');
            set |= interval_set::range(std::stoi(part.substr(0, dash)), std::stoi(part.substr(dash + 1)));
        }
        b.add_transition(from, to, set, w);
    }
    return b.build();
}

std::string join(std::span<const std::int32_t> values) {
    std::string out;
    for (auto v : values)
        out += (out.empty() ? "" : ",") + std::to_string(v);
    return out;
}


}  // namespace

TEST_CASE("the weighted automata of the Java library are reproduced", "[java][weighted]") {
    std::ifstream in(AUTOLIB_TEST_DATA "/java_weighted.tsv");
    REQUIRE(in.good());
    std::optional<weighted_automaton> a;
    std::string description;
    std::size_t automata = 0, checks = 0;
    for (std::string line; std::getline(in, line); ++checks) {
        auto f = split(line, '\t');
        if (f[0] == "A") {
            a = parse_automaton(f);
            description = line;
            ++automata;
            continue;
        }
        INFO(description);
        INFO(line);
        if (f[0] == "C") {
            word w;
            for (const auto& c : split(f[1], ' '))
                w.push_back(std::stoi(c));
            if (f[2].starts_with("E:"))
                REQUIRE(outcome([&] { (void)a->cost(w); }) == f[2]);
            else
                REQUIRE(a->cost(w) == parse_double(f[2]));
        } else if (f[0] == "O") {
            if (f[1].starts_with("E:"))
                REQUIRE(outcome([&] { (void)a->optimal_cost(); }) == f[1]);
            else
                REQUIRE(a->optimal_cost() == parse_double(f[1]));
        } else if (f[0] == "L") {
            auto expected = split(f[1], ' ');
            for (int l = 0; l <= 4; ++l) {
                const auto& e = expected[static_cast<std::size_t>(l)];
                if (e.starts_with("E:"))
                    REQUIRE(outcome([&] { (void)a->cost_of_length(l); }) == e);
                else
                    REQUIRE(a->cost_of_length(l) == parse_double(e));
            }
        } else if (f[0] == "P") {
            if (f[1].starts_with("E:")) {
                REQUIRE(outcome([&] { (void)a->k_best_paths(6); }) == f[1]);
                continue;
            }
            auto expected = f[1].empty() ? std::vector<std::string>{} : split(f[1], ' ');
            auto paths = a->k_best_paths(6);
            REQUIRE(paths.size() == expected.size());
            for (std::size_t i = 0; i < paths.size(); ++i) {
                auto parts = split(expected[i], '/');
                REQUIRE(paths[i].cost() == parse_double(parts[0]));
                REQUIRE(join(paths[i].edges()) == parts[1]);
                REQUIRE(join(paths[i].symbols()) == parts[2]);
            }
        } else if (f[0] == "W") {
            if (f[1].starts_with("E:")) {
                REQUIRE(outcome([&] { (void)a->k_best_words(6); }) == f[1]);
                continue;
            }
            auto expected = f[1].empty() ? std::vector<std::string>{} : split(f[1], ' ');
            auto words = a->k_best_words(6);
            REQUIRE(words.size() == expected.size());
            for (std::size_t i = 0; i < words.size(); ++i) {
                auto parts = split(expected[i], '/');
                REQUIRE(words[i].cost == parse_double(parts[0]));
                REQUIRE(join(words[i].symbols) == parts[1]);
            }
        } else if (f[0] == "D") {
            if (f[1].starts_with("E:")) {
                REQUIRE(outcome([&] { (void)a->determinize(64).minimize(); }) == f[1]);
                continue;
            }
            auto d = a->determinize(64);
            auto counts = split(f[1], ' ');
            REQUIRE(d.graph().state_count() == std::stoul(counts[0]));
            REQUIRE(d.graph().edge_count() == std::stoul(counts[1]));
            REQUIRE(d.minimize().graph().state_count() == std::stoul(counts[2]));
        } else if (f[0] == "R") {
            if (f[1].starts_with("E:"))
                REQUIRE(outcome([&] { (void)a->remove_epsilon(); }) == f[1]);
            else
                REQUIRE(a->remove_epsilon().graph().edge_count() == std::stoul(f[1]));
        }
    }
    REQUIRE(automata == 400);
    REQUIRE(checks > 18000);
}
