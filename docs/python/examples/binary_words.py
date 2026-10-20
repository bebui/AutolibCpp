"""Combinatorics on words : counts, ranks and uniform draws, exactly."""

import random

from autolib import Automaton, WordSampler

no_11 = Automaton("~(.*11.*)&(0|1)*")           # binary words without two consecutive 1

print("counts :", [no_11.count_words(n) for n in range(12)])   # Fibonacci numbers
print("length 500 :", no_11.count_words(500))

# All the words of length 4, in lexicographic order, then by rank
print("length 4 :", ["".join(map(str, w)) for w in no_11.words(4) if len(w) == 4])
sampler = WordSampler(no_11, 4)
print("rank of 0101 :", sampler.rank([0, 1, 0, 1]), "- word of rank 5 :", sampler[5])

# Balanced parentheses up to a depth : 0 opens, 1 closes
depth_3 = Automaton("(0(0(01)*1)*1)*")
print("balanced, depth 3, length 10 :", depth_3.count_words(10))
print("one at random :", "".join("()"[c] for c in depth_3.random_word(10, random.Random(5))))
