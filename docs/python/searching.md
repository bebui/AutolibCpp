# Searching sequences

A `Searcher` finds the factors of a sequence that belong to a language, leftmost first and longest among them,
in a single pass :

```python
>>> from autolib import Automaton
>>> s = Automaton("12+").searcher()
>>> s.find([3, 1, 2, 2, 4])
Match(1, 4)
>>> s.find_all([1, 2, 0, 1, 2, 2, 7, 1])
[Match(0, 2), Match(3, 6)]
>>> s.contains([2, 2, 1])
False

```

A `Match` unpacks as a pair, ready for slicing :

```python
>>> text = [9, 1, 2, 2, 2, 9]
>>> start, end = s.find(text)
>>> text[start:end]
[1, 2, 2, 2]

```

## Long sequences with numpy

Arrays of int32 are read in place, and the search runs without the GIL :

```python
>>> import numpy as np
>>> # A DNA strand, A C G T as 0 1 2 3, and the motif T A T A followed by A or T
>>> strand = np.random.default_rng(7).integers(0, 4, 1_000_000, dtype=np.int32)
>>> motif = Automaton("3030(0|3)").searcher()
>>> hits = motif.find_all(strand)
>>> 500 < len(hits) < 2500
True
>>> all(strand[h.start] == 3 for h in hits)
True

```

## Reading many words

`compile()` gives an immutable DFA laid out in flat arrays, with a transition table when it is small enough. It is
the fastest way to test many words, from several threads at once :

```python
>>> dfa = Automaton("(0|1)*0(0|1){8}").compile()
>>> dfa.has_lookup_table
True
>>> sum(dfa(w) for w in np.random.default_rng(1).integers(0, 2, (1000, 20), dtype=np.int32))
503

```
