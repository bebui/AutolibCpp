# automaton-lib

**Finite automata over integer symbols, computed in C++, used from Python.**

```{code-block} sh
pip install automaton-lib
```

```python
>>> from autolib import Automaton
>>> a = Automaton("(0|1)*0(0|1){2}")    # the third symbol from the end is 0
>>> b = Automaton("(0|1)*11(0|1)*")     # contains 1 1
>>> [0, 1, 1] in a & b
True
>>> (a - b).shortest_word()
[0, 0, 0]
>>> a.count_words(100) == 2**99
True

```

- **Regular expressions over integers** compiled to minimal automata, and the operations of regular languages as
  operators : `|`, `&`, `-`, `~`, `+`, `*`, `==`, `<=`.
- **Questions on languages** : emptiness, finiteness, equality, inclusion, counter-examples, shortest words.
- **Words** : lazy enumeration, exact counting with Python integers, uniform random sampling, ranks.
- **Searches** in long sequences, lists or numpy arrays, in linear time.
- **Weighted automata** : costs of words, shortest paths, the cheapest words and paths, exact determinization
  and minimization, costs depending on the position.
- **Several costs at once** : Pareto fronts, lexicographic optima, scalarizations.

Every computation runs in C++ without the GIL : automata can be shared between threads. Symbols are 32-bit
integers; words are any sequence of integers, and numpy arrays are read in place.

```{toctree}
:maxdepth: 2
:caption: Guide

getting-started
examples
regular-expressions
languages
words
searching
weighted
multi-weighted
visualization
performance
```

```{toctree}
:maxdepth: 1
:caption: Reference

api
```
