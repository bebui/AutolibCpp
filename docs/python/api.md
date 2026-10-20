# API reference

```{eval-rst}
.. module:: autolib

Automata
--------

.. autoclass:: Automaton
.. autoclass:: IntervalSet
.. autoclass:: Regex
.. autoexception:: RegexError
.. autodata:: MIN_SYMBOL
.. autodata:: MAX_SYMBOL

Words
-----

.. autoclass:: WordSampler
.. autoclass:: WordIterator

Compiled automata and searches
------------------------------

.. autoclass:: CompiledDFA
.. autoclass:: Searcher
.. autoclass:: Match

Weighted automata
-----------------

.. autoclass:: WeightedAutomaton
.. autoclass:: WeightedPath
.. autoclass:: WeightedWord
.. autoclass:: AutomatonGraph
.. autoclass:: AutomatonSnapshot
.. autoclass:: Edge
.. autoclass:: LayeredAutomaton
.. autoclass:: PathIterator
.. autoclass:: WordCostIterator

Shortest paths
--------------

.. autoclass:: ShortestPaths
.. autoclass:: PathAnalysis
.. autofunction:: shortest_paths
.. autofunction:: shortest_paths_from
.. autofunction:: dag_shortest_paths
.. autofunction:: dag_longest_paths
.. autofunction:: dijkstra
.. autofunction:: bellman_ford
.. autofunction:: backward_distances
.. autofunction:: distances_to
.. autofunction:: cost_through_state
.. autofunction:: cost_through_edge
.. autofunction:: determinize_dag
.. autofunction:: minimize_dag
.. autofunction:: k_best_paths_of_length
.. autofunction:: k_best_words_of_length

Several costs
-------------

.. autoclass:: MultiWeightedAutomaton
.. autoclass:: MultiWeightedPath
.. autoclass:: ParetoFront
.. autofunction:: dominates

Compiled weighted automata
--------------------------

.. autoclass:: CompiledWeightedDFA
.. autoclass:: CompiledMultiWeightedDFA
.. autoclass:: WeightedDFAIndex
```
