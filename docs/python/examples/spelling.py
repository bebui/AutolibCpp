"""Spelling suggestions : an edit-distance automaton, and the cheapest words it accepts."""

from autolib import IntervalSet, WeightedAutomaton

ALPHABET = IntervalSet(range(ord("a"), ord("z") + 1))

def within_edits(target, max_edits):
    """Words within max_edits edits of target; the cost of a word is its edit distance."""
    b = WeightedAutomaton.Builder()
    n = len(target)
    # State (i, e) : i letters of the target read, e edits made
    state = {(i, e): b.add_state() for i in range(n + 1) for e in range(max_edits + 1)}
    b.set_initial(state[0, 0])
    for (i, e), s in state.items():
        if i == n:
            b.set_accept(s)
        if i < n:
            b.add_transition(s, state[i + 1, e], ord(target[i]), 0.0)          # match
        if e < max_edits:
            b.add_transition(s, state[i, e + 1], ALPHABET, 1.0)                # insertion
            if i < n:
                b.add_transition(s, state[i + 1, e + 1], ALPHABET, 1.0)        # substitution
                b.add_epsilon(s, state[i + 1, e + 1], 1.0)                     # deletion
    return b.build()

dictionary = ["cat", "cart", "chat", "coat", "dog", "act", "cast", "scat", "at", "car"]
typos = within_edits("cat", 1)
for word in dictionary:
    cost = typos([ord(c) for c in word])
    print(f"{word:5} {'distance ' + str(int(cost)) if cost < float('inf') else 'too far'}")

# The closest words, cheapest first : no dictionary needed
closest = typos.k_best_words(4)
print("closest :", [("".join(map(chr, w.symbols)), w.cost) for w in closest])
