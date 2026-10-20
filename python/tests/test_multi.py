"""Automata weighted by vectors."""

import pytest

from autolib import MultiWeightedAutomaton, WeightedAutomaton, dominates


def choice():
    b = MultiWeightedAutomaton.Builder(2)
    b.add_state()
    b.add_state()
    b.set_initial(0, [1, 2])
    b.set_accept(1, [3, 4])
    for costs in ([0, 10], [5, 5], [10, 0], [6, 6], [5, 5]):
        b.add_transition(0, 1, 7, costs)
    return b.build()


def test_views_optima_and_fronts():
    a = choice()
    assert a.dimensions == 2
    assert a.project(0)([7]) == 4 and a.project(1)([7]) == 6
    assert a.scalarize([1, 2])([7]) == 26
    assert a.lexicographic_best_path().costs == [4, 16]
    assert a.lexicographic_best_path([1, 0]).costs == [14, 6]
    assert a.select_dimensions([1, 0]).lexicographic_best_path([1, 0]).costs == [16, 4]
    front = a.pareto_front()
    assert len(front) == 3 and len(a.pareto_front(all_paths=True)) == 4
    assert sorted(p.costs for p in front) == [[4, 16], [9, 11], [14, 6]]
    assert sorted(p.costs for p in a.pareto_for_word([7])) == [[4, 16], [9, 11], [14, 6]]
    assert len(a.pareto_for_word([8])) == 0
    assert front.is_complete and not front.includes_all_paths
    assert dominates([1, 2], [1, 3]) and not dominates([1, 2], [1, 2])
    with pytest.raises(ValueError):
        a.scalarize([1])
    with pytest.raises(IndexError):
        a.project(2)


def test_products_keep_every_combination():
    b = WeightedAutomaton.Builder()
    b.add_state()
    b.add_state()
    b.set_initial(0)
    b.set_accept(1)
    b.add_transition(0, 1, 1, 0)
    b.add_transition(0, 1, 1, 10)
    a = b.build()
    c = a.with_weights([10, 0])
    correlated = MultiWeightedAutomaton.from_weightings([a, c])
    independent = MultiWeightedAutomaton.synchronized_product([a, c])
    assert correlated.scalarize([1, 1])([1]) == 10
    assert independent.scalarize([1, 1])([1]) == 0
    assert independent.dimensions == 2 and correlated.graph.same_as(a.graph)
    with pytest.raises(ValueError):
        correlated.compile()


def test_compiled_vectors():
    np = pytest.importorskip("numpy")

    b = MultiWeightedAutomaton.Builder(2)
    b.add_state()
    b.add_state()
    b.set_initial(0, [1, 2])
    b.set_accept(1, [3, 4])
    b.add_transition(0, 1, range(-(2**31), 2**31), [0.5, -0.25])
    compiled = b.build().compile()
    assert compiled.costs([5]) == [4.5, 5.75]
    assert compiled.costs([5, 5]) is None
    out = np.zeros(2)
    assert compiled.costs_into(np.array([9], dtype=np.int32), out) and list(out) == [4.5, 5.75]
    assert compiled.scalarize([1, -2])([0]) == 4.5 - 2 * 5.75
    assert compiled.project(1)([0]) == 5.75
