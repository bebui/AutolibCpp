# automaton-lib

Finite automata over integer symbols, computed in C++ : regular expressions, the operations of regular languages
as operators, exact counting and uniform sampling of words, searches in sequences, and weighted automata with
shortest paths, best words, exact determinization and minimization, and Pareto fronts.

```sh
pip install automaton-lib
```

```python
from autolib import Automaton

a = Automaton("(0|1)*0(0|1){2}")   # minimal DFA : the third symbol from the end is 0
b = Automaton("(0|1)*11(0|1)*")    # contains 11

c = (a | b) & ~Automaton("11")      # union, intersection, complement
[0, 1, 1] in a                      # membership : True
(a - b) == (a & ~b)                 # equality of languages : True
a.count_words(100)                  # 2**99, exactly
a.random_word(20)                   # uniform among the accepted words of length 20
```

Weighted automata :

```python
from autolib import Automaton

# Each transition costs 2.5 if it reads 3, and 1 otherwise
routes = Automaton("(1|2)*3").weighted(lambda edge: 2.5 if 3 in edge.labels else 1.0)
routes([1, 2, 3])                   # 4.5, the cost of a word
routes.k_best_words(3)              # [3] for 2.5, then [1, 3] and [2, 3] for 3.5
```

Symbols are 32-bit integers. Words are any sequence of integers, or numpy arrays read in place.

Documentation : https://bebui.github.io/AutolibCpp/python/

License : LGPL 3.0 or later.
