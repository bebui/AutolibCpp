// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

#include <benchmark/benchmark.h>
#include <random>

#include "inputs.hpp"

using namespace autolib;

namespace {

void regex_to_minimal_dfa(benchmark::State& state, std::string expression) {
    for (auto _ : state)
        benchmark::DoNotOptimize(automaton::from_regex(expression));
}
BENCHMARK_CAPTURE(regex_to_minimal_dfa, nth_from_end_10, inputs::nth_from_end(10))->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(regex_to_minimal_dfa, nth_from_end_14, inputs::nth_from_end(14))->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(regex_to_minimal_dfa, words_2000, inputs::words(2000, 1))->Unit(benchmark::kMillisecond);

void determinize(benchmark::State& state) {
    auto nfa = inputs::random_nfa(7, static_cast<int>(state.range(0)), 4, 3);
    for (auto _ : state)
        benchmark::DoNotOptimize(nfa.determinize());
    state.counters["states"] = static_cast<double>(nfa.determinize().state_count());
}
BENCHMARK(determinize)->Arg(40)->Arg(60)->Unit(benchmark::kMillisecond);

void minimize_nth_from_end_14(benchmark::State& state) {
    auto dfa = automaton::nfa_from_regex(inputs::nth_from_end(14)).determinize();
    for (auto _ : state)
        benchmark::DoNotOptimize(dfa.minimize());
}
BENCHMARK(minimize_nth_from_end_14)->Unit(benchmark::kMillisecond);

void minimize_sparse(benchmark::State& state) {
    auto dfa = inputs::random_dfa(5, 20000, 2000, 3);
    for (auto _ : state)
        benchmark::DoNotOptimize(dfa.minimize());
}
BENCHMARK(minimize_sparse)->Unit(benchmark::kMillisecond);

struct pair_inputs {
    automaton a = automaton::from_regex("(0|1|2)*0(0|1|2){7}");
    automaton b = automaton::from_regex("(0|1|2)*1(0|1|2){6}");
    automaton same = automaton::from_regex("(0|1|2)*0(0|1|2)(0|1|2){6}");
    automaton super = automaton::from_regex("(0|1|2)*0(0|1|2)*");
};
const pair_inputs& pairs() {
    static const pair_inputs p;
    return p;
}

void intersection(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(pairs().a & pairs().b);
}
BENCHMARK(intersection)->Unit(benchmark::kMillisecond);

void same_language(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(pairs().a == pairs().same);
}
BENCHMARK(same_language)->Unit(benchmark::kMillisecond);

void subset(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(pairs().a <= pairs().super);
}
BENCHMARK(subset)->Unit(benchmark::kMillisecond);

void complement(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(~pairs().a);
}
BENCHMARK(complement)->Unit(benchmark::kMillisecond);

void shortest_word(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(pairs().a.shortest_word());
}
BENCHMARK(shortest_word)->Unit(benchmark::kMicrosecond);

struct text_inputs {
    automaton dfa = automaton::from_regex(inputs::nth_from_end(8));
    compiled_dfa compiled{dfa};
    word w = inputs::random_word(3, 1'000'000, 2);
    searcher search{automaton::from_regex("01+2")};
    word text = inputs::random_word(4, 1'000'000, 4);
};
const text_inputs& texts() {
    static const text_inputs t;
    return t;
}

void run_automaton(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(texts().dfa(texts().w));
}
BENCHMARK(run_automaton)->Unit(benchmark::kMillisecond);

void run_compiled(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(texts().compiled(texts().w));
}
BENCHMARK(run_compiled)->Unit(benchmark::kMillisecond);

void search_all(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(texts().search.find_all(texts().text).size());
}
BENCHMARK(search_all)->Unit(benchmark::kMillisecond);

void count_words_of_length(benchmark::State& state) {
    auto dfa = automaton::from_regex(inputs::nth_from_end(8));
    for (auto _ : state)
        benchmark::DoNotOptimize(count_words(dfa, static_cast<int>(state.range(0))));
}
BENCHMARK(count_words_of_length)->Arg(60)->Arg(2000)->Unit(benchmark::kMillisecond);

void random_word_1000(benchmark::State& state) {
    auto dfa = automaton::from_regex(inputs::nth_from_end(8));
    std::mt19937_64 rng(3);
    for (auto _ : state)
        benchmark::DoNotOptimize(random_word(dfa, 1000, rng));
}
BENCHMARK(random_word_1000)->Unit(benchmark::kMillisecond);

void sampler_draw_1000(benchmark::State& state) {
    word_sampler sampler(automaton::from_regex(inputs::nth_from_end(8)), 1000);
    std::mt19937_64 rng(3);
    for (auto _ : state)
        benchmark::DoNotOptimize(sampler(rng));
}
BENCHMARK(sampler_draw_1000)->Unit(benchmark::kMicrosecond);

// Weighted automata

void weighted_cost(benchmark::State& state) {
    auto a = inputs::weighted(11, 300, 4, 3, 1, 0, false);
    std::vector<word> words;
    for (int i = 0; i < 1000; ++i)
        words.push_back(inputs::random_word(100 + i, 30, 4));
    auto evaluator = a.evaluator();
    for (auto _ : state) {
        double total = 0;
        for (const auto& w : words)
            total += evaluator.cost(w);
        benchmark::DoNotOptimize(total);
    }
}
BENCHMARK(weighted_cost)->Unit(benchmark::kMillisecond);

void weighted_dijkstra(benchmark::State& state) {
    auto a = inputs::weighted(12, 200000, 4, 3, 0, 0, false);
    for (auto _ : state)
        benchmark::DoNotOptimize(shortest_paths(a, false).optimal_cost());
}
BENCHMARK(weighted_dijkstra)->Unit(benchmark::kMillisecond);

void weighted_bellman_exact(benchmark::State& state) {
    auto a = inputs::weighted(13, 400, 4, 3, 0, -2, false);
    for (auto _ : state)
        benchmark::DoNotOptimize(bellman_ford(a).optimal_cost());
}
BENCHMARK(weighted_bellman_exact)->Unit(benchmark::kMillisecond);

void weighted_determinize(benchmark::State& state) {
    auto a = inputs::weighted(14, 40, 2, 3, 0, 0, true);
    for (auto _ : state)
        benchmark::DoNotOptimize(a.determinize(100000));
    state.counters["states"] = static_cast<double>(a.determinize(100000).graph().state_count());
}
BENCHMARK(weighted_determinize)->Unit(benchmark::kMillisecond);

void weighted_minimize(benchmark::State& state) {
    auto a = inputs::weighted_dfa(15, 2000, 4, 0);
    for (auto _ : state)
        benchmark::DoNotOptimize(a.minimize());
    state.counters["states"] = static_cast<double>(a.minimize().graph().state_count());
}
BENCHMARK(weighted_minimize)->Unit(benchmark::kMillisecond);

void weighted_k_best_paths(benchmark::State& state) {
    auto a = inputs::weighted(16, 200, 4, 3, 0, 0, false);
    for (auto _ : state)
        benchmark::DoNotOptimize(a.k_best_paths(2000));
}
BENCHMARK(weighted_k_best_paths)->Unit(benchmark::kMillisecond);

void weighted_k_best_words(benchmark::State& state) {
    auto a = inputs::weighted(17, 50, 4, 3, 0, 0, false);
    for (auto _ : state)
        benchmark::DoNotOptimize(a.k_best_words(500));
}
BENCHMARK(weighted_k_best_words)->Unit(benchmark::kMillisecond);

void weighted_pareto(benchmark::State& state) {
    auto a = inputs::multi_dag(18, 60, 4);
    for (auto _ : state)
        benchmark::DoNotOptimize(a.pareto_front());
    state.counters["front"] = static_cast<double>(a.pareto_front().size());
}
BENCHMARK(weighted_pareto)->Unit(benchmark::kMillisecond);

void weighted_compiled(benchmark::State& state) {
    auto compiled = inputs::weighted_dfa(19, 1000, 4, 0).compile();
    auto w = inputs::random_word(20, 1000000, 4);
    for (auto _ : state)
        benchmark::DoNotOptimize(compiled.cost(w));
}
BENCHMARK(weighted_compiled)->Unit(benchmark::kMillisecond);

}  // namespace
