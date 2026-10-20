// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

import fr.menana.automaton.Automaton;
import fr.menana.automaton.Searcher;

import java.math.BigInteger;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;
import java.util.StringJoiner;

/**
 * Writes the results of the Java library fr.menana:automaton on random regular expressions, as tab separated lines,
 * so that the C++ library can be checked against them. Usage : java Oracle seed count > java_oracle.tsv
 * <pre>
 * R  expression  minimal-states  finite  empty  shortest-length  word-counts(0..6)
 * W  word  accepted                                  (words of the previous expression)
 * P  i  j  same  i-subset-j  j-subset-i  states(i&j)  states(i|j)  states(i-j)  states(ij)
 * S  i  text  matches                               (leftmost longest, start-end)
 * </pre>
 */
public final class Oracle {

    private final Random random;

    private Oracle(long seed) {
        this.random = new Random(seed);
    }

    private String symbol() {
        switch (random.nextInt(12)) {
            case 0: return ".";
            case 1: return "<-5>";
            case 2: return "<100>";
            default: return Integer.toString(random.nextInt(4));
        }
    }

    private String expression(int depth, boolean complement) {
        if (depth == 0)
            return symbol();
        switch (random.nextInt(complement ? 10 : 9)) {
            case 0:
            case 1:
            case 2: return expression(depth - 1, complement) + expression(depth - 1, complement);
            case 3:
            case 4: return "(" + expression(depth - 1, complement) + "|" + expression(depth - 1, complement) + ")";
            case 5: return "(" + expression(depth - 1, complement) + ")" + "*+?".charAt(random.nextInt(3));
            case 6: {
                int min = random.nextInt(3);
                String[] forms = {"{" + min + "}", "{" + min + "," + (min + random.nextInt(3)) + "}", "{" + min + ",}", "{," + (1 + random.nextInt(3)) + "}"};
                return "(" + expression(depth - 1, complement) + ")" + forms[random.nextInt(forms.length)];
            }
            case 7: return "(" + expression(depth - 1, complement) + "&" + expression(depth - 1, complement) + ")";
            case 8: return symbol();
            default: return "~(" + expression(depth - 1, false) + ")";
        }
    }

    private int[] word(int maxLength) {
        int[] w = new int[random.nextInt(maxLength + 1)];
        for (int i = 0; i < w.length; i++) {
            int r = random.nextInt(10);
            w[i] = r == 0 ? -5 : r == 1 ? 100 : r == 2 ? 7 : random.nextInt(4);
        }
        return w;
    }

    private static String text(int[] w) {
        StringJoiner j = new StringJoiner(" ");
        for (int v : w)
            j.add(Integer.toString(v));
        return j.toString();
    }

    private static int bit(boolean b) {
        return b ? 1 : 0;
    }

    private void run(int count) {
        List<Automaton> dfas = new ArrayList<>();
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < count; i++) {
            String e = expression(1 + random.nextInt(4), random.nextInt(4) == 0);
            Automaton dfa = Automaton.dfaFromString(e);
            dfas.add(dfa);
            int[] shortest = dfa.shortestWord();
            StringJoiner counts = new StringJoiner(",");
            for (int n = 0; n <= 6; n++) {
                BigInteger c = dfa.countWords(n);
                counts.add(c.toString());
            }
            out.append("R\t").append(e).append('\t').append(dfa.getNbStates()).append('\t').append(bit(dfa.isFinite()))
               .append('\t').append(bit(dfa.isEmpty())).append('\t').append(shortest == null ? -1 : shortest.length)
               .append('\t').append(counts).append('\n');
            for (int k = 0; k < 12; k++) {
                int[] w = word(8);
                out.append("W\t").append(text(w)).append('\t').append(bit(dfa.run(w))).append('\n');
            }
        }
        for (int i = 0; i + 1 < count; i++) {
            int j = random.nextInt(4) == 0 ? i : i + 1 + random.nextInt(Math.min(3, count - i - 1));
            Automaton a = dfas.get(i), b = dfas.get(j);
            out.append("P\t").append(i).append('\t').append(j).append('\t').append(bit(a.sameLanguage(b)))
               .append('\t').append(bit(a.isSubsetOf(b))).append('\t').append(bit(b.isSubsetOf(a)))
               .append('\t').append(a.intersection(b).getNbStates()).append('\t').append(a.union(b).getNbStates())
               .append('\t').append(a.difference(b).getNbStates()).append('\t').append(a.concatenate(b).getNbStates())
               .append('\n');
        }
        for (int i = 0; i < count; i += 3) {
            Searcher searcher = dfas.get(i).searcher();
            int[] t = word(30);
            StringJoiner matches = new StringJoiner(",");
            for (Searcher.Match m : searcher.findAll(t))
                matches.add(m.start() + "-" + m.end());
            out.append("S\t").append(i).append('\t').append(text(t)).append('\t').append(matches).append('\n');
        }
        System.out.print(out);
    }

    public static void main(String[] args) {
        new Oracle(Long.parseLong(args[0])).run(Integer.parseInt(args[1]));
    }
}
