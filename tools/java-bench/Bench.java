// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

import fr.menana.automaton.Automaton;
import fr.menana.automaton.CompiledDfa;
import fr.menana.automaton.Operation;
import fr.menana.automaton.Searcher;
import fr.menana.automaton.State;

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
    }
}
