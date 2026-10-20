"""Weighted automata, snapshots, unfoldings and compiled evaluators."""

import itertools
import math
from fractions import Fraction

import pytest

from autolib import (
    Automaton,
    AutomatonGraph,
    LayeredAutomaton,
    PathAnalysis,
    ShortestPaths,
    WeightedAutomaton,
    bellman_ford,
    dag_longest_paths,
    dijkstra,
    k_best_paths_of_length,
    k_best_words_of_length,
    shortest_paths,
)

inf = math.inf


def example():
    b = WeightedAutomaton.Builder()
    s, t = b.add_state(), b.add_state()
    b.set_initial(s)
    b.set_accept(t, 0.5)
    b.add_transition(s, t, 1, 2.0)
    b.add_transition(s, t, range(0, 10), 3.0)
    b.add_transition(t, t, 0, 0.25)
    return b.build()


def test_costs_and_best_paths():
    a = example()
    assert a([1]) == 2.5 and a([2]) == 3.5 and a([1, 0]) == 2.75 and a([11]) == inf
    assert a.cost((1, 0, 0)) == 3.0
    assert a.optimal_cost == 2.5
    best = a.best_path()
    assert best.symbols == [1] and best.cost == 2.5 and best.edges == [0] and len(best) == 1
    assert a.best_path([2, 0]).cost == 3.75
    assert a.best_path([11]) is None
    assert a.cost_of_length(3) == 3.0 and a.cost_of_length_at_most(3) == 2.5
    assert a.best_word_of_length(2) == [1, 0]
    evaluator = a.evaluator()
    assert [evaluator(w) for w in ([1], [1, 0], [])] == [2.5, 2.75, inf]
    assert a.optimal_cost_with_prefix([3]) == 3.5
    assert a.run([7, 0]) and [7] in a and [] not in a
    assert a.to_automaton() == Automaton("<0>|<1>|<2>|<3>|<4>|<5>|<6>|<7>|<8>|<9>").concatenate(Automaton("0*"))


def test_enumerations_by_cost():
    a = example()
    words = a.k_best_words(4)
    assert [(w.symbols, w.cost) for w in words] == [([1], 2.5), ([1, 0], 2.75), ([1, 0, 0], 3.0), ([1, 0, 0, 0], 3.25)]
    symbols, cost = words[0]
    assert symbols == [1] and cost == 2.5
    costs = []
    for path in a.paths_by_cost():
        if path.cost > 3:
            break
        costs.append(path.cost)
    assert costs == [2.5, 2.75, 3.0]
    lazy = a.words_by_cost()
    assert next(lazy).symbols == [1] and next(lazy).symbols == [1, 0]
    assert [p.cost for p in a.k_best_paths(3)] == [2.5, 2.75, 3.0]


def test_operations_preserve_costs():
    a = example()
    b = Automaton("0*").weighted(1.0)
    words = [list(w) for n in range(4) for w in itertools.product([0, 1, 2], repeat=n)]
    union, product, concatenation = a | b, a & b, a + b
    for w in words:
        assert union(w) == min(a(w), b(w))
        assert product(w) == (a(w) + b(w) if a(w) < inf and b(w) < inf else inf)
        assert concatenation(w) == min(a(w[:i]) + b(w[i:]) for i in range(len(w) + 1))
        assert a.reverse()(w[::-1]) == a(w)
        assert a.simplify()(w) == a(w)
    assert (+a)([1, 1]) == 5.0 and a.star()([]) == 0
    assert a.repeat(2)([1, 1, 1]) == 7.5


def test_shortest_paths():
    b = WeightedAutomaton.Builder()
    for _ in range(4):
        b.add_state()
    b.set_initial(0, 2)
    b.set_accept(2, 3)
    b.add_transition(0, 1, 4, -5)
    b.add_transition(1, 2, 5, 1)
    b.add_transition(0, 2, 9, 4)
    a = b.build()
    assert dag_longest_paths(a).optimal_cost == 9
    assert shortest_paths(a).optimal_cost == 1
    assert shortest_paths(a).status_to(3) == ShortestPaths.Status.UNREACHABLE
    assert PathAnalysis(a).cost_through_edge(2) == 9
    assert bellman_ford(a).distances() == shortest_paths(a).distances()
    with pytest.raises(ValueError):
        dijkstra(a)


def test_negative_cycles_and_errors():
    b = WeightedAutomaton.Builder()
    b.add_state()
    b.set_initial(0)
    b.set_accept(0)
    b.add_epsilon(0, 0, -1)
    a = b.build()
    assert a([]) == -inf and a.run([])
    with pytest.raises(RuntimeError):
        a.best_path()
    with pytest.raises(ValueError):
        a.remove_epsilon()
    with pytest.raises(ValueError):
        WeightedAutomaton.of(AutomatonGraph(), [1.0])
    b = WeightedAutomaton.Builder()
    b.add_state()
    b.set_initial(0)
    b.set_accept(0)
    b.add_transition(0, 0, 1, 1e308)
    with pytest.raises(OverflowError):
        b.build()([1, 1])


def test_determinization_and_minimization():
    b = WeightedAutomaton.Builder()
    for _ in range(5):
        b.add_state()
    b.set_initial(0)
    b.set_accept(3, 4)
    b.set_accept(4, 1)
    b.add_transition(0, 1, 0, 2)
    b.add_transition(0, 2, 1, 3)
    b.add_transition(1, 3, 2, 1)
    b.add_transition(2, 4, 2, 4)
    minimal = b.build().minimize()
    assert minimal.graph.state_count == 3 and minimal([0, 2]) == 7 and minimal([1, 2]) == 8
    a = example()
    d = a.determinize()
    assert d.graph.is_deterministic and all(d(w) == a(w) for w in ([1], [2, 0], [1, 0, 0], [5]))
    assert d.compile()([1, 0, 0]) == 3.0
    with pytest.raises(ValueError):
        a.determinize(max_states=0)


def test_snapshots_share_graphs():
    source = Automaton("12+")
    assert source.weighted(2)([1, 2, 2]) == 6
    snapshot = source.snapshot()
    price = snapshot.weighted(lambda edge: 0.0 if edge.epsilon else 2.0, initial=1, final=0.5)
    both = snapshot.multi_weighted(2, lambda edge: 5.0)
    assert price.graph.same_as(both.graph)
    assert not source.weighted(1).graph.same_as(source.weighted(1).graph)
    assert price([1, 2]) == 5.5
    assert both.scalarize([1, 1])([1, 2, 2]) == 21
    assert price.to_automaton() == source and snapshot.run([1, 2]) and [1] not in snapshot
    edge = snapshot.origin(0)
    assert (edge.source, edge.epsilon, edge.labels) == (0, False, IntervalSet_of(1))
    with pytest.raises(ValueError):
        source.weighted(math.nan)
    with pytest.raises(TypeError):
        source.weighted("2")


def IntervalSet_of(x):
    from autolib import IntervalSet

    return IntervalSet(x)


def test_unfoldings():
    a = example()
    layers = LayeredAutomaton.exactly(a.graph, 3)
    assert layers.best_path(a).symbols == [1, 0, 0]
    analysis = layers.analyze(a, lambda position, edge: position + 1.0)
    assert analysis.optimal_cost == 6.5
    assert analysis.cost_at(0, 1) == 6.5
    assert [p.length for p in []] == []
    assert all(len(p) == 2 for p in k_best_paths_of_length(a, 2, 5))
    assert [w.symbols for w in k_best_words_of_length(a, 2, 3)] == [[1, 0], [0, 0], [2, 0]]


def test_graph_builder_and_queries():
    b = AutomatonGraph.Builder()
    p, q = b.add_state(), b.add_state(accept=True)
    b.set_initial(p)
    assert b.add_transition(p, q, range(0, 5)) == 0
    assert b.add_epsilon(q, p) == 1
    g = b.build()
    assert g.state_count == 2 and g.edge_count == 2 and g.initial == p
    assert g.outgoing(p) == [0] and g.incoming(p) == [1]
    assert g.labels(0) == IntervalSet_of(range(0, 5)) and g.is_epsilon(1)
    assert g.run([1, 2]) and not g.is_acyclic and not g.is_deterministic
    assert g.useful_states() == [True, True] and g.coaccessible_states() == [True, True]
    assert WeightedAutomaton.of(g, [1.0, 0.5])([1, 1]) == 2.5


def test_exact_costs_are_fractions():
    from autolib import MultiWeightedAutomaton

    b = MultiWeightedAutomaton.Builder(1)
    for _ in range(3):
        b.add_state()
    b.set_initial(0)
    b.set_accept(2)
    b.add_transition(0, 1, 0, [0.1])
    b.add_transition(1, 2, 0, [0.2])
    front = b.build().pareto_front()
    assert front[0].exact_cost(0) == Fraction(0.1) + Fraction(0.2)
