"""Automata, sets of symbols and regular expressions."""

import copy
import itertools

import pytest

from autolib import MAX_SYMBOL, MIN_SYMBOL, Automaton, IntervalSet, Regex, RegexError


def all_words(symbols, max_length):
    for length in range(max_length + 1):
        yield from (list(w) for w in itertools.product(symbols, repeat=length))


def test_interval_sets():
    s = IntervalSet([1, 2, 3, range(10, 13)])
    assert s.intervals == [(1, 3), (10, 12)]
    assert list(s) == [1, 2, 3, 10, 11, 12]
    assert len(s) == 6 and s.cardinality == 6
    assert 11 in s and 4 not in s
    assert IntervalSet.range(1, 2) in s
    assert s.min == 1 and s.max == 12
    assert IntervalSet(range(0, 10)) | IntervalSet(20) == IntervalSet([range(0, 10), 20])
    assert (s & IntervalSet(range(2, 11))).intervals == [(2, 3), (10, 10)]
    assert (s - IntervalSet(2)).intervals == [(1, 1), (3, 3), (10, 12)]
    assert (s ^ IntervalSet(range(3, 11))).intervals == [(1, 2), (4, 9), (11, 12)]
    assert ~IntervalSet() == IntervalSet.all()
    assert IntervalSet.all().cardinality == 2**32
    assert IntervalSet.range(MIN_SYMBOL, MIN_SYMBOL + 1).intervals == [(MIN_SYMBOL, MIN_SYMBOL + 1)]
    assert IntervalSet(5) <= s | IntervalSet(5) and not IntervalSet(5) <= s
    assert not IntervalSet() and IntervalSet(0)
    assert {IntervalSet(range(3)), IntervalSet([0, 1, 2])} == {IntervalSet.range(0, 2)}
    assert repr(IntervalSet.range(1, 3)) == "IntervalSet({1..3})"
    assert s.isdisjoint(IntervalSet(5))
    with pytest.raises(ValueError):
        IntervalSet(range(0, 10, 2))
    with pytest.raises(TypeError):
        IntervalSet("12")
    with pytest.raises(OverflowError):
        IntervalSet(2**40)
    with pytest.raises(ValueError):
        IntervalSet().min


def test_operators_follow_the_languages():
    a = Automaton("(0|1)*0")
    b = Automaton("1(0|1)*")
    assert [1, 1, 0] in a and a([1, 1, 0]) and a.run((1, 0))
    assert [1, 1] not in a
    assert (a - b) == (a & ~b)
    assert a <= (a | b) and a < (a | b) and (a | b) >= b and not a <= b
    assert (a ^ b) == ((a - b) | (b - a))
    assert a + b == Automaton("(0|1)*01(0|1)*")
    assert a.star() == Automaton("((0|1)*0)*")
    assert +Automaton("0") == Automaton("0+")
    assert Automaton("01") * 3 == Automaton("(01){3}") == Automaton("01") ** 3
    assert Automaton("0").repeat(2) == Automaton("0{2,}")
    assert Automaton("0").repeat(1, 3) == Automaton("0{1,3}")
    assert Automaton("0").optional() == Automaton("0?")
    assert a.reverse() == Automaton("0(0|1)*")
    assert a != b
    with pytest.raises(TypeError):
        hash(a)


def test_membership_agrees_with_every_word():
    for expression in ["(0|1)*0(0|1)", "(01|10)*", "~((0|1)*11(0|1)*)&(0|1)*", "0(1|2)*|2{2,3}", "(0|12)*2?"]:
        nfa = Automaton.nfa(expression)
        dfa = Automaton(expression)
        minimal = nfa.minimize("brzozowski")
        compiled = dfa.compile()
        for w in all_words([0, 1, 2], 5):
            expected = nfa.run(w)
            assert dfa.run(w) == minimal.run(w) == compiled(w) == nfa.reduce().run(w) == expected


def test_structure():
    a = Automaton()
    assert a.initial is None and a.state_count == 0
    s, t = a.add_state(), a.add_state(accept=True)
    a.initial = s
    a.add_transition(s, t, range(0, 10))
    a.add_transition(t, t, 0)
    a.add_epsilon(t, s)
    assert a.transitions(s) == [(t, IntervalSet.range(0, 9))]
    assert a.epsilons(t) == [s]
    assert a.is_accept(t) and not a.is_accept(s)
    assert a.transition_count == 3
    assert not a.is_deterministic
    assert [5, 0, 7] in a
    d = a.determinize()
    assert d.is_deterministic and d.step(d.initial, 3) is not None and d.step(d.initial, 30) is None
    assert d == a
    b = copy.deepcopy(a)
    b.set_accept(t, False)
    assert [5] in a and [5] not in b
    with pytest.raises(IndexError):
        a.is_accept(7)


def test_questions_on_languages():
    assert Automaton("0&1").is_empty()
    assert Automaton(".*").is_universal()
    assert Automaton("0{2,5}").is_finite() and not Automaton("0*").is_finite()
    assert Automaton("(0|1)*11").shortest_word() == [1, 1]
    assert Automaton("0&1").shortest_word() is None
    assert Automaton("0*").counter_example(Automaton("0{0,3}")) == [0, 0, 0, 0]
    assert Automaton("0*").counter_example(Automaton("0*")) is None
    assert Automaton.universal().is_universal()
    assert Automaton.empty_language().is_empty()
    assert Automaton.empty_word() == Automaton("()")
    assert Automaton.symbols([1, 2]) == Automaton("1|2")
    assert Automaton.any_symbol() == Automaton(".")
    assert Automaton.of_word([3, 1, 4]) == Automaton("314")
    assert Automaton("<-3>|<2147483647>").run([MAX_SYMBOL])


def test_regular_expressions():
    r = Regex("(<10>|<20>)+.{2,}")
    assert r.to_dfa().shortest_word() == [10, MIN_SYMBOL, MIN_SYMBOL]
    assert r.to_nfa() == r.to_dfa()
    with pytest.raises(RegexError) as error:
        Automaton("(1|")
    assert error.value.position == 3
    assert isinstance(error.value, ValueError)
    with pytest.raises(RegexError):
        Regex("1**{")


def test_output():
    a = Automaton("12")
    assert "digraph" in a.to_dot()
    assert "->" in str(a)
    assert repr(a).startswith("<Automaton : 3 states")
