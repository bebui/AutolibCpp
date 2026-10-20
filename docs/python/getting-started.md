# Getting started

## Installing

```{code-block} sh
pip install automaton-lib
```

Ready-made wheels exist for Linux (x86-64 and ARM, glibc and musl), macOS (Intel and Apple Silicon) and Windows,
for CPython 3.10 and later and PyPy. Elsewhere, pip builds the package from its sources with any C++20 compiler.

The package installs under the name `autolib` :

```python
>>> import autolib
>>> autolib.Automaton
<class 'autolib._autolib.Automaton'>

```

## Automata from regular expressions

An automaton recognizes a set of words, its *language*. Words are sequences of 32-bit integers, and the simplest
way to describe a language is a regular expression :

```python
>>> from autolib import Automaton
>>> even = Automaton("(0|1)*0")       # binary words ending with 0
>>> [1, 0, 1, 0] in even
True
>>> even([1, 1])                       # the same question, as a call
False

```

`Automaton(expression)` builds the *minimal deterministic automaton* of the expression : the smallest machine that
recognizes the language and reads each word without hesitation.

```python
>>> even.state_count, even.is_deterministic
(2, True)

```

## Operators

The operations of languages are Python operators, and they return new automata :

```python
>>> ends_with_0 = Automaton("(0|1)*0")
>>> starts_with_1 = Automaton("1(0|1)*")
>>> both = ends_with_0 & starts_with_1         # intersection
>>> either = ends_with_0 | starts_with_1       # union
>>> only_first = ends_with_0 - starts_with_1   # difference
>>> others = ~ends_with_0                      # complement : every other word
>>> then = ends_with_0 + starts_with_1         # concatenation
>>> [1, 0] in both, [0, 0] in only_first, [1] in others
(True, True, True)

```

Repetitions have methods, and a few operators :

| Python                | Language                                   |
|-----------------------|--------------------------------------------|
| `a.star()`            | zero or more words of `a`                  |
| `+a`, `a.plus()`      | one or more words of `a`                   |
| `a.optional()`        | zero or one word of `a`                    |
| `a * 3`, `a ** 3`     | exactly three words of `a`                 |
| `a.repeat(2, 5)`      | from two to five words of `a`              |
| `a.repeat(2)`         | two or more words of `a`                   |

Comparisons compare *languages*, not machines :

```python
>>> (ends_with_0 - starts_with_1) == (ends_with_0 & ~starts_with_1)
True
>>> both <= ends_with_0          # inclusion
True
>>> Automaton("(01)*") == Automaton("()|01(01)*")
True

```

## Building an automaton by hand

States are numbered from 0. A transition reads an integer, a `range`, or a set of symbols :

```python
>>> a = Automaton()
>>> start, digits = a.add_state(), a.add_state(accept=True)
>>> a.initial = start
>>> a.add_transition(start, digits, range(0, 10))     # one digit, 0 to 9
>>> a.add_transition(digits, digits, range(0, 10))    # then any number of digits
>>> a.transitions(start)
[(1, IntervalSet({0..9}))]
>>> [4, 2] in a, [] in a, [42] in a
(True, False, False)

```

Large alphabets cost nothing : a transition stores intervals, never the symbols one by one.

```python
>>> from autolib import IntervalSet
>>> letters = IntervalSet([range(65, 91), range(97, 123)])     # code points of A-Z and a-z
>>> letters
IntervalSet({65..90, 97..122})
>>> word = Automaton.symbols(letters).plus()
>>> [ord(c) for c in "Hello"] in word
True

```

## Transformations

| Method              | Result                                                            |
|---------------------|-------------------------------------------------------------------|
| `determinize()`     | an equivalent deterministic automaton                             |
| `minimize()`        | the minimal deterministic automaton                               |
| `reduce()`          | a smaller automaton, possibly non-deterministic, never larger     |
| `remove_epsilon()`  | an equivalent automaton without epsilon transitions               |
| `reverse()`         | the automaton of the mirror words                                 |
| `compile()`         | a compiled DFA, to read many words quickly                        |

```python
>>> nfa = Automaton.nfa("(0|1)*1(0|1){8}")      # Thompson automaton, built in linear time
>>> nfa.state_count, nfa.minimize().state_count
(30, 512)

```

The minimal automaton of this language has 512 states : it must remember the last nine symbols. The
non-deterministic one has 30, and every operation accepts both.
