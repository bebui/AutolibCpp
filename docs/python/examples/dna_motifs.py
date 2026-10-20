"""DNA motifs : search a pattern and its reverse complement in a random genome."""

import numpy as np

from autolib import Automaton

A, C, G, T = range(4)
complement = {A: T, C: G, G: C, T: A}

# TATA box : TATA, then A or T, then A, then A or T
motif = [T, A, T, A, (A, T), A, (A, T)]

def expression(pattern):
    parts = []
    for base in pattern:
        options = base if isinstance(base, tuple) else (base,)
        parts.append("(" + "|".join(str(b) for b in options) + ")")
    return "".join(parts)

forward = Automaton(expression(motif))
# The other strand reads the complement, backwards
backward = Automaton(expression([tuple(complement[b] for b in x) if isinstance(x, tuple) else complement[x]
                                 for x in reversed(motif)]))
both = forward | backward

genome = np.random.default_rng(3).integers(0, 4, 2_000_000, dtype=np.int32)
print("words of the motif :", forward.count_words(7))
print("on the forward strand :", len(forward.searcher().find_all(genome)))
print("on either strand      :", len(both.searcher().find_all(genome)))
