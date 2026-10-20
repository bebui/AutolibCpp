// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

import fr.menana.automaton.Automaton;
import fr.menana.automaton.CompiledDfa;
import fr.menana.automaton.Operation;
import fr.menana.automaton.Searcher;
import fr.menana.automaton.State;
import fr.menana.automaton.weighted.MultiWeightedAutomaton;
import fr.menana.automaton.weighted.WeightedAutomaton;
import fr.menana.automaton.weighted.WeightedPaths;
import fr.menana.automaton.weighted.compiled.CompiledWeightedDfa;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Random;
import java.util.Set;
import java.util.function.Supplier;

/**
 * The benchmarks of benchmarks/benchmarks.cpp on the Java library, with identical inputs.
 * Usage : java -Xms4g -Xmx4g Bench [filter]. Prints the median time of an operation after a warm up.
 */
public final class Bench {

    private static Object sink;

    private static void measure(String name, String filter, Supplier<Object> operation) {
        if (filter != null && !name.contains(filter))
            return;
        long warmup = System.nanoTime() + 3_000_000_000L;
        while (System.nanoTime() < warmup)
            sink = operation.get();
        List<Long> times = new ArrayList<>();
        long end = System.nanoTime() + 5_000_000_000L;
        while (System.nanoTime() < end || times.size() < 5) {
            long start = System.nanoTime();
            sink = operation.get();
            times.add(System.nanoTime() - start);
        }
        times.sort(null);
        System.out.printf("%-40s %12.3f ms   (%d runs)%n", name, times.get(times.size() / 2) / 1e6, times.size());
    }

    static String nthFromEnd(int k) {
        return "(0|1)*0(0|1){" + k + "}";
    }

    static String symbol(int v) {
        return v < 10 ? Integer.toString(v) : "<" + v + ">";
    }

    static String words(int n, long seed) {
        Random r = new Random(seed);
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < n; i++) {
            if (i > 0)
                b.append('|');
            for (int j = 0; j < 4; j++)
                b.append(symbol(r.nextInt(20)));
        }
        return b.toString();
    }

    static Automaton build(int n, List<int[]> edges, Random r) {
        Automaton a = new Automaton();
        for (int i = 0; i < n; i++)
            a.addState();
        List<State> st = a.getStates();
        a.setInitial(st.get(0));
        for (int i = 0; i < n; i++)
            a.setAccept(st.get(i), r.nextInt(3) == 0);
        for (int[] e : edges)
            a.addTransition(st.get(e[0]), st.get(e[2]), e[1]);
        return a;
    }

    static Automaton randomNfa(long seed, int n, int sigma, int perState) {
        Random r = new Random(seed);
        List<int[]> edges = new ArrayList<>();
        for (int s = 0; s < n; s++)
            for (int j = 0; j < perState; j++) {
                int v = r.nextInt(sigma);
                edges.add(new int[]{s, v, r.nextInt(n)});
            }
        return build(n, edges, r);
    }

    static Automaton randomDfa(long seed, int n, int sigma, int perState) {
        Random r = new Random(seed);
        List<int[]> edges = new ArrayList<>();
        for (int s = 0; s < n; s++) {
            Set<Integer> used = new HashSet<>();
            for (int j = 0; j < perState; j++) {
                int v = r.nextInt(sigma);
                if (used.add(v))
                    edges.add(new int[]{s, v, r.nextInt(n)});
            }
            edges.add(new int[]{s, sigma, (s + 1) % n});
        }
        return build(n, edges, r);
    }

    static int[] randomWord(long seed, int length, int sigma) {
        Random r = new Random(seed);
        int[] w = new int[length];
        for (int i = 0; i < length; i++)
            w[i] = r.nextInt(sigma);
        return w;
    }

    static WeightedAutomaton weighted(long seed, int n, int sigma, int perState, int epsilons, int low, boolean dag) {
        Random r = new Random(seed);
        WeightedAutomaton.Builder b = WeightedAutomaton.builder();
        for (int s = 0; s < n; s++) b.addState();
        b.setInitial(0);
        for (int s = 0; s < n; s++)
            if (r.nextInt(4) == 0 || (dag && s == n - 1)) b.setAccept(s, r.nextInt(8) / 4.0);
        for (int s = 0; s < (dag ? n - 1 : n); s++) {
            for (int j = 0; j < perState; j++) {
                int v = r.nextInt(sigma);
                int t = dag ? s + 1 + r.nextInt(Math.min(4, n - 1 - s)) : r.nextInt(n);
                b.addTransition(s, t, v, (r.nextInt(17) + low) / 4.0);
            }
            for (int j = 0; j < epsilons; j++) {
                int t = dag ? s + 1 + r.nextInt(Math.min(4, n - 1 - s)) : r.nextInt(n);
                b.addEpsilonTransition(s, t, r.nextInt(9) / 4.0);
            }
        }
        return b.build();
    }

    static WeightedAutomaton weightedDfa(long seed, int n, int sigma, int low) {
        Random r = new Random(seed);
        WeightedAutomaton.Builder b = WeightedAutomaton.builder();
        for (int s = 0; s < n; s++) b.addState();
        b.setInitial(0);
        for (int s = 0; s < n; s++)
            if (r.nextInt(4) == 0) b.setAccept(s, r.nextInt(8) / 4.0);
        for (int s = 0; s < n; s++)
            for (int c = 0; c < sigma; c++) {
                int t = r.nextInt(n);
                b.addTransition(s, t, c, (r.nextInt(17) + low) / 4.0);
            }
        return b.build();
    }

    static MultiWeightedAutomaton multiDag(long seed, int n, int perState) {
        Random r = new Random(seed);
        MultiWeightedAutomaton.Builder b = MultiWeightedAutomaton.builder(2);
        for (int s = 0; s < n; s++) b.addState();
        b.setInitial(0);
        b.setAccept(n - 1);
        for (int s = 0; s < n - 1; s++)
            for (int j = 0; j < perState; j++) {
                int t = s + 1 + r.nextInt(Math.min(4, n - 1 - s));
                double x = r.nextInt(9), y = r.nextInt(9);
                b.addTransition(s, t, 0, x, y);
            }
        return b.build();
    }

    public static void main(String[] args) {
        String f = args.length > 0 ? args[0] : null;
        for (int k : new int[]{10, 14})
            measure("regex_to_minimal_dfa/nth_from_end_" + k, f, () -> Automaton.dfaFromString(nthFromEnd(k)));
        String w2000 = words(2000, 1);
        measure("regex_to_minimal_dfa/words_2000", f, () -> Automaton.dfaFromString(w2000));
        for (int n : new int[]{40, 60}) {
            Automaton nfa = randomNfa(7, n, 4, 3);
            System.out.println("determinize/" + n + " states=" + nfa.determinize().getNbStates());
            measure("determinize/" + n, f, nfa::determinize);
        }
        Automaton nth14 = Automaton.nfaFromString(nthFromEnd(14)).determinize();
        measure("minimize_nth_from_end_14", f, () -> Operation.minimizeHopcroft(nth14));
        Automaton sparse = randomDfa(5, 20000, 2000, 3);
        measure("minimize_sparse", f, () -> Operation.minimizeHopcroft(sparse));
        Automaton a = Automaton.dfaFromString("(0|1|2)*0(0|1|2){7}");
        Automaton b = Automaton.dfaFromString("(0|1|2)*1(0|1|2){6}");
        Automaton same = Automaton.dfaFromString("(0|1|2)*0(0|1|2)(0|1|2){6}");
        Automaton sup = Automaton.dfaFromString("(0|1|2)*0(0|1|2)*");
        measure("intersection", f, () -> a.intersection(b));
        measure("same_language", f, () -> a.sameLanguage(same));
        measure("subset", f, () -> a.isSubsetOf(sup));
        measure("complement", f, a::complement);
        measure("shortest_word", f, a::shortestWord);
        Automaton dfa = Automaton.dfaFromString(nthFromEnd(8));
        CompiledDfa compiled = dfa.compile();
        int[] word = randomWord(3, 1_000_000, 2);
        measure("run_automaton", f, () -> dfa.run(word));
        measure("run_compiled", f, () -> compiled.run(word));
        Searcher searcher = Automaton.dfaFromString("01+2").searcher();
        int[] text = randomWord(4, 1_000_000, 4);
        measure("search_all", f, () -> searcher.findAll(text).size());
        measure("count_words/64_bits", f, () -> dfa.countWords(60));
        measure("count_words/2000_bits", f, () -> dfa.countWords(2000));
        WeightedAutomaton costs = weighted(11, 300, 4, 3, 1, 0, false);
        List<int[]> costWords = new ArrayList<>();
        for (int i = 0; i < 1000; i++) costWords.add(randomWord(100 + i, 30, 4));
        WeightedAutomaton.Evaluator evaluator = costs.evaluator();
        measure("weighted_cost", f, () -> { double total = 0; for (int[] w : costWords) total += evaluator.cost(w); return total; });
        WeightedAutomaton big = weighted(12, 200000, 4, 3, 0, 0, false);
        measure("weighted_dijkstra", f, () -> WeightedPaths.shortestPaths(big, false).optimalCost());
        WeightedAutomaton signed = weighted(13, 400, 4, 3, 0, -2, false);
        measure("weighted_bellman_exact", f, () -> WeightedPaths.bellmanFord(signed).optimalCost());
        WeightedAutomaton dagNfa = weighted(14, 40, 2, 3, 0, 0, true);
        System.out.println("weighted_determinize states=" + dagNfa.determinize(100000).graph().getNbStates());
        measure("weighted_determinize", f, () -> dagNfa.determinize(100000));
        WeightedAutomaton wdfa = weightedDfa(15, 2000, 4, 0);
        System.out.println("weighted_minimize states=" + wdfa.minimize().graph().getNbStates());
        measure("weighted_minimize", f, wdfa::minimize);
        WeightedAutomaton paths = weighted(16, 200, 4, 3, 0, 0, false);
        measure("weighted_k_best_paths", f, () -> paths.kBestPaths(2000));
        WeightedAutomaton wordsNfa = weighted(17, 50, 4, 3, 0, 0, false);
        measure("weighted_k_best_words", f, () -> wordsNfa.kBestWords(500));
        MultiWeightedAutomaton multi = multiDag(18, 60, 4);
        System.out.println("weighted_pareto front=" + multi.paretoFront().size());
        measure("weighted_pareto", f, multi::paretoFront);
        CompiledWeightedDfa compiledW = weightedDfa(19, 1000, 4, 0).compile();
        int[] longWord = randomWord(20, 1000000, 4);
        measure("weighted_compiled", f, () -> compiledW.cost(longWord));
        Random random = new Random(3);
        measure("random_word/1000", f, () -> dfa.randomWord(1000, random));
    }
}
