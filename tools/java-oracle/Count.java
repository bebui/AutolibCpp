// Counts of words of the Java library fr.menana:automaton on a few expressions, up to huge counts.
import fr.menana.automaton.*;
public class Count {
    public static void main(String[] args) {
        String[] exprs = {"(0|1)*0(0|1){3}", ".*", "(<-5>|<1000000>)*.", "~((0|1)*11(0|1)*)&(0|1)*", "(0|12|345)*", "(.{3})*&~(.*7.*)", "0*1*2*", "()", "0&1"};
        int[] lengths = {0, 1, 5, 64, 65, 100, 257};
        for (String e : exprs) {
            Automaton a = Automaton.dfaFromString(e);
            StringBuilder sb = new StringBuilder(e);
            for (int l : lengths) sb.append('\t').append(a.countWords(l));
            System.out.println(sb);
        }
    }
}
