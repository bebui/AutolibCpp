# Performance

The library is written in C++ and runs every computation outside the Python interpreter.

- **No GIL** : long computations release the global interpreter lock. Automata, compiled DFAs, searchers and
  weighted automata are immutable once built and can be shared between threads.
- **numpy without copies** : words and texts given as contiguous int32 arrays are read in place; int64 arrays are
  converted once.
- **Machine integers first** : counts, ranks and exact costs use 64-bit arithmetic while values fit, and switch to
  big integers only when needed.
- **Compiled forms** : `compile()` lays a DFA out in flat arrays with a transition table; reading a symbol costs
  two array accesses.

```python
>>> import threading
>>> import numpy as np
>>> from autolib import Automaton
>>> dfa = Automaton("(0|1)*0(0|1){8}").compile()
>>> texts = [np.random.default_rng(i).integers(0, 2, 1_000_000, dtype=np.int32) for i in range(4)]
>>> results = [None] * 4
>>> def read(i):
...     results[i] = dfa(texts[i])
>>> threads = [threading.Thread(target=read, args=(i,)) for i in range(4)]
>>> for t in threads: t.start()
>>> for t in threads: t.join()
>>> results == [dfa(t) for t in texts]
True

```

Measured in C++ against the Java library it comes from, the operations run 1.1 to 9.7 times faster; see the
[project page](https://github.com/bebui/AutolibCpp#performance).
