"""Enumeration, counting, ranks and uniform sampling of words."""

import random
from collections import Counter

import pytest

from autolib import Automaton, WordSampler


def test_words_in_shortlex_order():
    assert list(Automaton("(1|2)3?").words(2)) == [[1], [2], [1, 3], [2, 3]]
    assert list(Automaton("0*").words(3)) == [[], [0], [0, 0], [0, 0, 0]]
    assert list(Automaton("0&1").words(4)) == []
    it = Automaton(".").words(1)
    assert next(it) == [-(2**31)] and next(it) == [-(2**31) + 1]
    with pytest.raises(ValueError):
        Automaton("0").words(-1)


def test_counts_are_exact_python_integers():
    assert Automaton("(0|1)*").count_words(100) == 2**100
    assert Automaton(".*").count_words(3) == 2**96
    fibonacci = Automaton("~(.*11.*)&(0|1)*")
    a, b = 1, 2
    for length in range(200):
        assert fibonacci.count_words(length) == a
        a, b = b, a + b


def test_sampler_ranks_and_draws():
    sampler = WordSampler(Automaton("(0|1)*0(0|1){3}"), 20)
    assert sampler.count == 2**19 == len(sampler) and sampler.length == 20
    w = sampler.draw(random.Random(1))
    assert sampler.unrank(sampler.rank(w)) == w
    assert sampler[0] == [0] * 20 and sampler[-1] == [1] * 16 + [0, 1, 1, 1]
    assert sampler.rank([1]) is None
    huge = WordSampler(Automaton("(.<3>|<-1>..)*&~(.*<0><0>.*)"), 200)
    assert huge.count > 2**2000
    rng = random.Random(9)
    for _ in range(20):
        w = huge.draw(rng)
        assert huge.unrank(huge.rank(w)) == w
    with pytest.raises(OverflowError):
        len(huge)
    with pytest.raises(IndexError):
        WordSampler(Automaton("00"), 3).draw()


def test_random_words_are_uniform():
    a = Automaton("(0|1)*0(0|1){2}")
    rng = random.Random(17)
    seen = Counter(tuple(a.random_word(4, rng)) for _ in range(16000))
    assert len(seen) == 8 and all(1700 < c < 2300 for c in seen.values())
    assert a.random_word(2) is None
