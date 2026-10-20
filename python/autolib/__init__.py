# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (c) 2025-2026 Julien Menana
"""Finite automata over integer symbols, computed in C++.

Regular expressions over integers, the operations of regular languages as operators, exact counting and uniform
sampling of words, searches in sequences, and weighted automata : shortest paths, best words, exact
determinization and minimization, Pareto fronts of several costs.

    >>> from autolib import Automaton
    >>> a = Automaton("(0|1)*0")
    >>> b = Automaton("1(0|1)*")
    >>> [1, 1, 0] in a & b
    True
    >>> (a | b).count_words(10)
    768
"""

from ._autolib import (
    MAX_SYMBOL,
    MIN_SYMBOL,
    Automaton,
    AutomatonGraph,
    AutomatonSnapshot,
    CompiledDFA,
    CompiledMultiWeightedDFA,
    CompiledWeightedDFA,
    Edge,
    IntervalSet,
    LayeredAutomaton,
    Match,
    MultiWeightedAutomaton,
    MultiWeightedPath,
    ParetoFront,
    PathAnalysis,
    PathIterator,
    Regex,
    RegexError,
    Searcher,
    ShortestPaths,
    WeightedAutomaton,
    WeightedDFAIndex,
    WeightedPath,
    WeightedWord,
    WordCostIterator,
    WordIterator,
    WordSampler,
    __version__,
    backward_distances,
    bellman_ford,
    cost_through_edge,
    cost_through_state,
    dag_longest_paths,
    dag_shortest_paths,
    determinize_dag,
    dijkstra,
    distances_to,
    dominates,
    k_best_paths_of_length,
    k_best_words_of_length,
    minimize_dag,
    shortest_paths,
    shortest_paths_from,
)

__all__ = [
    "MAX_SYMBOL",
    "MIN_SYMBOL",
    "Automaton",
    "AutomatonGraph",
    "AutomatonSnapshot",
    "CompiledDFA",
    "CompiledMultiWeightedDFA",
    "CompiledWeightedDFA",
    "Edge",
    "IntervalSet",
    "LayeredAutomaton",
    "Match",
    "MultiWeightedAutomaton",
    "MultiWeightedPath",
    "ParetoFront",
    "PathAnalysis",
    "PathIterator",
    "Regex",
    "RegexError",
    "Searcher",
    "ShortestPaths",
    "WeightedAutomaton",
    "WeightedDFAIndex",
    "WeightedPath",
    "WeightedWord",
    "WordCostIterator",
    "WordIterator",
    "WordSampler",
    "__version__",
    "backward_distances",
    "bellman_ford",
    "cost_through_edge",
    "cost_through_state",
    "dag_longest_paths",
    "dag_shortest_paths",
    "determinize_dag",
    "dijkstra",
    "distances_to",
    "dominates",
    "k_best_paths_of_length",
    "k_best_words_of_length",
    "minimize_dag",
    "shortest_paths",
    "shortest_paths_from",
]
