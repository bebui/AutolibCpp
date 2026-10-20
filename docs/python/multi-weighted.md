# Several costs at once

A `MultiWeightedAutomaton` carries a vector of costs on each transition. An ambiguous word has one vector per
accepting path : the library never mixes the costs of different paths.

## A journey : time against money

```python
>>> from autolib import MultiWeightedAutomaton
>>> b = MultiWeightedAutomaton.Builder(2)          # minutes, euros
>>> home, station, office = b.add_state(), b.add_state(), b.add_state()
>>> b.set_initial(home)
>>> b.set_accept(office)
>>> b.add_transition(home, station, 0, [15, 0])     # walk
0
>>> b.add_transition(home, station, 0, [5, 8])      # taxi
1
>>> b.add_transition(station, office, 0, [40, 4])   # slow train
2
>>> b.add_transition(station, office, 0, [20, 15])  # fast train
3
>>> b.add_transition(home, office, 0, [50, 30])     # taxi all the way : dominated
4
>>> journey = b.build()

```

The **Pareto front** keeps the vectors that no other vector beats on every cost at once :

```python
>>> for path in sorted(journey.pareto_front(), key=lambda p: p.costs):
...     print(path.costs, path.edges)
[25.0, 23.0] [1, 3]
[35.0, 15.0] [0, 3]
[45.0, 12.0] [1, 2]
[55.0, 4.0] [0, 2]

```

The taxi all the way, at 50 minutes and 30 euros, is beaten by the taxi and the fast train.

## Choosing one

Lexicographically : the fastest, then the cheapest among them; or the cheapest first.

```python
>>> journey.lexicographic_best_path().costs
[25.0, 23.0]
>>> journey.lexicographic_best_path([1, 0]).costs
[55.0, 4.0]

```

With a price of time : a scalarization is an ordinary weighted automaton.

```python
>>> per_euro = journey.scalarize([0.5, 1.0])     # half a euro per minute
>>> per_euro.optimal_cost, per_euro.best_path().edges
(31.5, [0, 2])

```

## From scalar weightings

Weightings of one snapshot become the dimensions of a vector, their costs staying on the same paths :

```python
>>> from autolib import Automaton
>>> snapshot = Automaton("(0|1)*1").snapshot()
>>> both = snapshot.multi_weighted(lambda e: 1.0, lambda e: 3.0 if 1 in e.labels else 0.0)
>>> both.dimensions, both.project(1)([0, 0, 1])
(2, 3.0)

```

## Exact sums

When doubles would round, the front is computed exactly; `exact_cost` gives the exact sum as a fraction :

```python
>>> b = MultiWeightedAutomaton.Builder(1)
>>> p, q, r = b.add_state(), b.add_state(), b.add_state()
>>> b.set_initial(p)
>>> b.set_accept(r)
>>> b.add_transition(p, q, 0, [0.1])
0
>>> b.add_transition(q, r, 0, [0.2])
1
>>> path = b.build().pareto_front()[0]
>>> path.cost(0)
0.30000000000000004
>>> path.exact_cost(0)
Fraction(10808639105689191, 36028797018963968)

```
