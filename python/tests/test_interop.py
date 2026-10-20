"""numpy arrays, searches and calls from several threads."""

import threading

import pytest

np = pytest.importorskip("numpy")

from autolib import Automaton, CompiledDFA, Match, Searcher


def test_numpy_words():
    a = Automaton("(1|2)*12")
    compiled = a.compile()
    for dtype in (np.int32, np.int64):
        word = np.array([2, 1, 2], dtype=dtype)
        assert a.run(word) and compiled(word) and word in a
    assert compiled(np.array([0, 2, 1, 2], dtype=np.int32)[1:])
    strided = np.array([1, 0, 2, 0], dtype=np.int64)[::2]
    assert a.run(strided)
    with pytest.raises(TypeError):
        a.run(np.array([2**40], dtype=np.int64))
    with pytest.raises(TypeError):
        a.run("12")


def test_search():
    s = Searcher(Automaton("12+"))
    assert s.find([3, 1, 2, 2, 4]) == Match(1, 4)
    assert tuple(s.find([3, 1, 2, 2, 4])) == (1, 4)
    assert s.find([3, 4]) is None
    assert s.find_all([1, 2, 0, 1, 2, 2]) == [Match(0, 2), Match(3, 6)]
    text = np.random.default_rng(4).integers(0, 4, 200_000, dtype=np.int32)
    matches = s.find_all(text)
    assert all(text[m.start] == 1 and all(text[m.start + 1 : m.end] == 2) for m in matches[:100])
    assert s.contains(text)


def test_threads_share_automata():
    dfa = CompiledDFA(Automaton("(0|1)*0(0|1){8}"))
    words = [np.random.default_rng(i).integers(0, 2, 100_000, dtype=np.int32) for i in range(8)]
    expected = [dfa(w) for w in words]
    results = [None] * 8

    def work(i):
        results[i] = dfa(words[i])

    threads = [threading.Thread(target=work, args=(i,)) for i in range(8)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert results == expected
