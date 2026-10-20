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

}  // namespace
