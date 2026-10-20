# Drawing automata

Every automaton, weighted automaton and graph describes itself in the language of
[Graphviz](https://graphviz.org) with `to_dot()`. Two functions draw it, with the Graphviz program `dot` :

- `autolib.to_svg(automaton, highlight=None)` returns the drawing as SVG text;
- `autolib.render(automaton, path, highlight=None)` writes it to a file, whose extension gives the format :
  `.svg`, `.png`, `.pdf`...

`highlight` is a path, `WeightedPath` or `MultiWeightedPath`, or a list of edges : it is drawn in colour, with
its states.

Graphviz installs apart from Python : `apt install graphviz`, `brew install graphviz`, `conda install graphviz`,
`winget install graphviz`, or from [graphviz.org](https://graphviz.org/download/).

## In Jupyter

An automaton alone in a cell shows its drawing when Graphviz is installed, and its text otherwise :

```python
from autolib import Automaton
Automaton("(0|1)*0(0|1)")
```

## An automaton

Double circles are the accepting states, the point leads to the initial state, and the labels are sets of
symbols.

```{literalinclude} figures/automaton.py
:language: python
```

![The automaton of (0|1)*0(0|1)](figures/automaton.svg)

## A weighted automaton and its best path

Edges show `symbols / cost`, accepting states their final cost. Parallel edges stay distinct : the bus and the
bike both read 1.

```{literalinclude} figures/best_path.py
:language: python
```

![The best path of length two](figures/best_path.svg)

## Epsilon edges

Epsilon edges, reading nothing, are dashed. Here the automaton of the words at most one edit away from `ab`,
the path of `xab` highlighted : an insertion, then two matches.

```{literalinclude} figures/edit_distance.py
:language: python
```

![Words at most one edit from ab](figures/edit_distance.svg)

## Several costs

Edges show their vectors of costs. Here the fastest journey of the Pareto front :

```{literalinclude} figures/pareto.py
:language: python
```

![The fastest journey of the Pareto front](figures/pareto.svg)

## Other tools

`to_dot()` is plain text : the Python package `graphviz`, NetworkX (`nx.nx_pydot.read_dot`) or any Graphviz viewer
read it.

```python
from autolib import Automaton
print(Automaton("12*").to_dot())
```
