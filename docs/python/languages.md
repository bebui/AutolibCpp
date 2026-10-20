# Questions on languages

These questions read the language, not the machine. They never change the automaton.

```python
>>> from autolib import Automaton
>>> Automaton("0&1").is_empty()
True
>>> Automaton("(0|1)*|~(0|1)*").is_universal()
True
>>> Automaton("0{2,5}|1").is_finite(), Automaton("01*").is_finite()
(True, False)

```

## Equality and inclusion

`==` is the equality of languages, `<=` and `<` the inclusion. They do not need determinizing both sides :
inclusion uses antichains, equality a union-find on the product.

```python
>>> a = Automaton("(0|1)*00(0|1)*")      # contains 0 0
>>> b = Automaton("(0|1)*0(0|1)*")       # contains 0
>>> a <= b, b <= a, a < b
(True, False, True)

```

When two languages differ, a counter-example tells why : a shortest word in one of them only.

```python
>>> a.counter_example(b)
[0]
>>> print(Automaton("(01)*").counter_example(Automaton("()|01(01)*")))     # equal languages
None

```

## Shortest words

```python
>>> Automaton("(0|1)*11(0|1)*1").shortest_word()
[1, 1, 1]
>>> Automaton("0&1").shortest_word() is None
True

```

## A small proof by automata

Every binary word without two consecutive 1 and ending with 1 ends with 01, or is the word 1 :

```python
>>> no_11 = Automaton("~(.*11.*)&(0|1)*")
>>> ends_1 = Automaton("(0|1)*1")
>>> (no_11 & ends_1) <= Automaton("(0|1)*01|1")
True

```
