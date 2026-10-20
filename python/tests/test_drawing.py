"""Graphviz descriptions and drawings."""

import shutil

import pytest

import autolib
from autolib import Automaton, MultiWeightedAutomaton, WeightedAutomaton


def commute():
    b = WeightedAutomaton.Builder()
    home, work = b.add_state(), b.add_state()
    b.set_initial(home, 1.5)
    b.set_accept(work, 0.5)
    b.add_transition(home, work, 1, 30.0)
    b.add_transition(home, work, 1, 20.0)
    b.add_epsilon(work, home, 0.0)
    return b.build()


def test_descriptions():
    a = commute()
    dot = a.to_dot()
    assert dot.startswith("digraph automaton {")
    assert 'start -> 0 [label="/ 1.5"]' in dot
    assert '1 [shape=doublecircle, label="1\\n/ 0.5"]' in dot
    assert '0 -> 1 [label="{1} / 30"]' in dot and '0 -> 1 [label="{1} / 20"]' in dot
    assert "style=dashed" in dot
    marked = a.to_dot(a.best_path())
    assert '0 -> 1 [label="{1} / 20", color="#d6336c"' in marked
    assert a.to_dot([0]).count('fontcolor="#d6336c"') == 1
    assert '0 -> 1 [label="{1}"]' in a.graph.to_dot()
    with pytest.raises(IndexError):
        a.to_dot([5])
    b = MultiWeightedAutomaton.Builder(2)
    p, q = b.add_state(), b.add_state()
    b.set_initial(p)
    b.set_accept(q)
    b.add_transition(p, q, 7, [10, 1.5])
    v = b.build()
    assert '{7} / (10, 1.5)' in v.to_dot()
    assert "#d6336c" in v.to_dot(v.pareto_front()[0])


@pytest.mark.skipif(shutil.which("dot") is None, reason="Graphviz is not installed")
def test_drawings(tmp_path):
    a = commute()
    svg = autolib.to_svg(a, highlight=a.best_path())
    assert svg.lstrip().startswith("<?xml") and "<svg" in svg
    assert a._repr_svg_().count("<svg") == 1
    autolib.render(Automaton("12*"), str(tmp_path / "a.png"))
    assert (tmp_path / "a.png").read_bytes().startswith(b"\x89PNG")


def test_without_graphviz(monkeypatch):
    monkeypatch.setattr(shutil, "which", lambda name: None)
    assert Automaton("12")._repr_svg_() is None
    with pytest.raises(RuntimeError, match="graphviz.org"):
        autolib.to_svg(Automaton("12"))
    with pytest.raises(TypeError):
        autolib.to_svg(Automaton("12"), highlight=[0])
