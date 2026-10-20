// Results of the weighted automata of the Java library fr.menana:automaton on random automata, for
// tests/data/java_weighted.tsv. Each automaton is described by a line A, followed by its results.
//
// A  states  initial  initial-weight  finals (state:weight ...)  edges (from>to:label@weight ..., label "e" or lo-hi+lo-hi)
// C  word  cost                 cost of a word
// O  cost                       optimal cost
// L  costs of lengths 0 to 4
// P  k best paths : cost/edges/word ...  or E:exception if an exception is thrown
// W  k best words : cost/word ...     or E
// D  states and edges of determinize(64), states of its minimization, or E
// R  edges of remove_epsilon, or E
// Doubles are written in hexadecimal.

import fr.menana.automaton.Interval;
import fr.menana.automaton.IntervalSet;
import fr.menana.automaton.weighted.*;

import java.util.*;

public class WeightedOracle {
    static String hex(double d) { return Double.toHexString(d); }

    static String join(int[] values, String sep) {
        StringJoiner j = new StringJoiner(sep);
        for (int v : values) j.add(Integer.toString(v));
        return j.toString();
    }

    public static void main(String[] args) {
        Random r = new Random(Long.parseLong(args[0]));
        int count = Integer.parseInt(args[1]);
        double[] palette = {-2, -1.5, -1, -0.75, -0.5, -0.25, 0, 0.25, 0.5, 1, 1.25, 2, 3, 0.1, 0.2, 1e16, -1e16};
        StringBuilder out = new StringBuilder();
        for (int round = 0; round < count; round++) {
            int n = 2 + r.nextInt(5);
            boolean nonnegative = r.nextBoolean();
            // A third of the automata also use weights whose sums are not exact in doubles
            int colors = r.nextInt(3) == 0 ? palette.length : palette.length - 4;
            WeightedAutomaton.Builder b = WeightedAutomaton.builder();
            for (int s = 0; s < n; s++) b.addState();
            double initialWeight = palette[r.nextInt(colors)];
            if (nonnegative) initialWeight = Math.abs(initialWeight);
            b.setInitial(0, initialWeight);
            StringJoiner finals = new StringJoiner(" ");
            for (int s = 0; s < n; s++) if (r.nextInt(3) == 0 || s == n - 1) {
                double w = palette[r.nextInt(colors)];
                if (nonnegative) w = Math.abs(w);
                b.setAccept(s, w); finals.add(s + ":" + hex(w));
            }
            StringJoiner edges = new StringJoiner(" ");
            int m = 2 * n + r.nextInt(n + 1);
            for (int i = 0; i < m; i++) {
                int from = r.nextInt(n), to = r.nextInt(n);
                double w = palette[r.nextInt(colors)];
                if (nonnegative) w = Math.abs(w);
                if (r.nextInt(5) == 0) {
                    b.addEpsilonTransition(from, to, w); edges.add(from + ">" + to + ":e@" + hex(w));
                } else {
                    int lo = r.nextInt(3), hi = lo + r.nextInt(3 - lo);
                    IntervalSet label = IntervalSet.fromInterval(new Interval(lo, hi));
                    String text = lo + "-" + hi;
                    if (r.nextInt(6) == 0) { label.add(new Interval(5, 6)); text += "+5-6"; }
                    b.addTransition(from, to, label, w); edges.add(from + ">" + to + ":" + text + "@" + hex(w));
                }
            }
            WeightedAutomaton a = b.build();
            out.append("A\t").append(n).append("\t0\t").append(hex(initialWeight)).append('\t').append(finals).append('\t').append(edges).append('\n');
            List<int[]> words = new ArrayList<>(); words.add(new int[0]);
            for (int from = 0, length = 1; length <= 3; length++) {
                int to = words.size();
                for (int i = from; i < to; i++) for (int c = 0; c < 3; c++) { int[] w = Arrays.copyOf(words.get(i), length); w[length - 1] = c; words.add(w); }
                from = to;
            }
            for (int[] w : words) {
                String cost;
                try { cost = hex(a.cost(w)); } catch (RuntimeException e) { cost = "E:" + e.getClass().getSimpleName(); }
                out.append("C\t").append(join(w, " ")).append('\t').append(cost).append('\n');
            }
            String optimal;
            try { optimal = hex(a.optimalCost()); } catch (RuntimeException e) { optimal = "E:" + e.getClass().getSimpleName(); }
            out.append("O\t").append(optimal).append('\n');
            StringJoiner lengths = new StringJoiner(" ");
            for (int l = 0; l <= 4; l++) {
                try { lengths.add(hex(a.costOfLength(l))); } catch (RuntimeException e) { lengths.add("E:" + e.getClass().getSimpleName()); }
            }
            out.append("L\t").append(lengths).append('\n');
            try {
                StringJoiner paths = new StringJoiner(" ");
                for (WeightedPath p : a.kBestPaths(6)) paths.add(hex(p.cost()) + "/" + join(p.edges(), ",") + "/" + join(p.word(), ","));
                out.append("P\t").append(paths).append('\n');
            } catch (RuntimeException e) { out.append("P\tE:").append(e.getClass().getSimpleName()).append("\n"); }
            try {
                StringJoiner list = new StringJoiner(" ");
                for (WeightedWord w : a.kBestWords(6)) list.add(hex(w.cost()) + "/" + join(w.word(), ","));
                out.append("W\t").append(list).append('\n');
            } catch (RuntimeException e) { out.append("W\tE:").append(e.getClass().getSimpleName()).append("\n"); }
            try {
                WeightedAutomaton d = a.determinize(64);
                int minimal = d.minimize().graph().getNbStates();
                out.append("D\t").append(d.graph().getNbStates()).append(' ').append(d.graph().getNbTransitions())
                   .append(' ').append(minimal).append('\n');
            } catch (RuntimeException e) { out.append("D\tE:").append(e.getClass().getSimpleName()).append("\n"); }
            try {
                int remaining = a.removeEpsilon().graph().getNbTransitions();
                out.append("R\t").append(remaining).append('\n');
            } catch (RuntimeException e) { out.append("R\tE:").append(e.getClass().getSimpleName()).append("\n"); }
        }
        System.out.print(out);
    }
}
