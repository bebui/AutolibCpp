"""Text as code points : validate identifiers, count them, and draw random ones."""

import random

from autolib import Automaton, IntervalSet

# Symbols are integers : a string is the list of its code points
def codes(text):
    return [ord(c) for c in text]

letter = IntervalSet([range(ord("a"), ord("z") + 1), range(ord("A"), ord("Z") + 1), ord("_")])
digit = IntervalSet(range(ord("0"), ord("9") + 1))

# [A-Za-z_][A-Za-z0-9_]* : a letter, then letters and digits
identifier = Automaton.symbols(letter) + Automaton.symbols(letter | digit).star()

for name in ["x", "_tmp1", "2fast", "snake_case", "kebab-case"]:
    print(f"{name!r:14} {'valid' if codes(name) in identifier else 'invalid'}")

# How many identifiers of 8 characters ? 53 * 63**7, exactly
print("identifiers of 8 characters :", identifier.count_words(8))

# Random identifiers, uniformly among all of them
rng = random.Random(1)
print("random :", ["".join(map(chr, identifier.random_word(8, rng))) for _ in range(3)])
