# Weighted automata

A weighted automaton puts a cost on every transition, plus an initial cost and final costs. The cost of a word is
the **least** cost of its accepting paths : `inf` if the word is not accepted.

## Weighting an automaton

The quickest way : weight an existing automaton, with a number or a function of each transition.

```python
>>> from autolib import Automaton
>>> source = Automaton("12+")
>>> flat = source.weighted(2.0)                  # every symbol costs 2
>>> flat([1, 2, 2])
6.0
>>> priced = source.weighted(lambda edge: 0.5 if 2 in edge.labels else 3.0)
>>> priced([1, 2, 2, 2])
4.5
>>> priced([1])                                  # not accepted
inf

```

The function receives an `Edge` : its `source` and `target` states, its `labels`, whether it is an `epsilon`
transition, and its `index`.

## Building with costs

A builder creates the graph and its costs together. Parallel transitions stay distinct, each with its own cost :

```python
>>> from autolib import WeightedAutomaton
>>> b = WeightedAutomaton.Builder()
>>> home, work = b.add_state(), b.add_state()
>>> b.set_initial(home)
>>> b.set_accept(work, 0.5)                       # a final cost
>>> b.add_transition(home, work, 1, 30.0)         # 1 : by bus, 30 minutes
0
>>> b.add_transition(home, work, 1, 20.0)         # 1 again : by bike, 20 minutes
1
>>> b.add_transition(home, work, 2, 45.0)         # 2 : on foot
2
>>> b.add_transition(work, work, 0, 5.0)          # 0 : a coffee
3
>>> commute = b.build()
>>> commute([1]), commute([1, 0]), commute([2])
(20.5, 25.5, 45.5)

```

## Best paths and best words

```python
>>> commute.optimal_cost
20.5
>>> commute.best_path()
WeightedPath(symbols=[1], cost=20.5)
>>> commute.best_path().edges                     # the bike
[1]
>>> commute.k_best_words(3)
[WeightedWord(symbols=[1], cost=20.5), WeightedWord(symbols=[1, 0], cost=25.5), WeightedWord(symbols=[1, 0, 0], cost=30.5)]

```

`k_best_paths` lists paths, so the same word appears once per path; `k_best_words` lists distinct words, each
with its least cost. Both have a lazy form, to stop whenever you like :

```python
>>> for word, cost in commute.words_by_cost():
...     if cost > 40:
...         break
...     print(word, cost)
[1] 20.5
[1, 0] 25.5
[1, 0, 0] 30.5
[1, 0, 0, 0] 35.5

```

## Costs of lengths and prefixes

```python
>>> commute.cost_of_length(3)                     # the best word of three symbols
30.5
>>> commute.best_word_of_length(3)
[1, 0, 0]
>>> commute.optimal_cost_with_prefix([2])         # the best continuation of a word starting with 2
45.5

```

## Operations

They preserve the costs of the words :

| Python       | Cost of a word                                         |
|--------------|--------------------------------------------------------|
| `a \| b`     | the least of both costs                                |
| `a & b`      | the sum of both costs, for the words of both           |
| `a + b`      | the least sum over the ways of splitting the word      |
| `a.star()`   | the least sum over the splits into words of `a`        |
| `+a`         | the same, with at least one word                       |
| `a.reverse()`| the cost of the mirror word                            |

```python
>>> twice = commute + commute
>>> twice([1, 1])
41.0

```

## Exact transformations

`determinize()` and `minimize()` compute exactly, then check that every resulting weight is a double :

```python
>>> d = commute.determinize()
>>> d.graph.is_deterministic, d([1, 0])
(True, 25.5)
>>> d.minimize().graph.state_count
2
>>> fast = d.compile()                           # compiled evaluator of a deterministic automaton
>>> fast([1, 0, 0])
30.5

```

## Several weightings of one automaton

A snapshot captures the structure once. Its weightings share the same graph, edge numbers included, so that they
can be combined :

```python
>>> snapshot = Automaton("(1|2)*3").snapshot()
>>> time = snapshot.weighted(lambda e: 1.0 if 3 not in e.labels else 4.0)
>>> money = snapshot.weighted(lambda e: 2.0, final=1.0)
>>> time.graph.same_as(money.graph)
True
>>> time([1, 2, 3]), money([1, 2, 3])
(6.0, 7.0)
>>> time.run([1, 3]), [3, 3] in time            # membership, whatever the costs
(True, False)
>>> time.to_automaton() == Automaton("(1|2)*3")   # back to the language
True

```

## Costs depending on the position

A `LayeredAutomaton` unfolds a graph by the number of symbols read. Its weighting can depend on the position :
rush hour for the third trip, for example.

```python
>>> from autolib import LayeredAutomaton
>>> layers = LayeredAutomaton.exactly(commute.graph, 3)
>>> rush = layers.analyze(commute, lambda position, edge: 60.0 if position == 2 else commute.weight(edge))
>>> rush.optimal_cost                            # bike, coffee, then a coffee at rush hour
85.5
>>> rush.cost_at(0, 2)                           # the best way when the first symbol is 2
110.5

```

## Shortest paths

The functions of the module work on any weighting : `shortest_paths` picks Dijkstra, a pass over a DAG, or
Bellman-Ford, exact beyond the integers, as needed.

```python
>>> from autolib import shortest_paths, bellman_ford
>>> result = shortest_paths(commute)
>>> result.optimal_cost, result.distances()
(20.5, [0.0, 20.0])

```

Negative cycles make costs unbounded : `-inf`, and no best path.

```python
>>> b = WeightedAutomaton.Builder()
>>> s = b.add_state()
>>> b.set_initial(s)
>>> b.set_accept(s)
>>> b.add_epsilon(s, s, -1.0)
0
>>> b.build()([])
-inf

```
