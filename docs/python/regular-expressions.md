# Regular expressions

Expressions describe languages over integer symbols.

| Syntax                 | Language                                                        |
|------------------------|-----------------------------------------------------------------|
| `0` … `9`              | the one-symbol word made of that digit                          |
| `<n>`                  | the one-symbol word `n`, any 32-bit integer : `<-3>`, `<1000>`  |
| `.`                    | every one-symbol word                                           |
| `rs`                   | concatenation                                                   |
| `r\|s`                 | union                                                           |
| `r&s`                  | intersection                                                    |
| `~r`                   | complement                                                      |
| `r*`, `r+`, `r?`       | zero or more, one or more, zero or one                          |
| `r{n}`, `r{n,m}`       | exactly `n`, from `n` to `m`                                    |
| `r{n,}`, `r{,m}`       | at least `n`, at most `m`                                       |
| `(r)`, `()`            | grouping, the empty word                                        |

Precedence, from the loosest : `|`, `&`, concatenation, `~`, then the postfix operators.

```python
>>> from autolib import Automaton, Regex
>>> big = Automaton("<1000><-1>*<1000>")
>>> [1000, -1, -1, 1000] in big
True
>>> no_double = Automaton("~(.*11.*)&(0|1)*")     # binary words without two consecutive 1
>>> [1, 0, 1] in no_double, [0, 1, 1] in no_double
(True, False)

```

## Deterministic or not

`Automaton(expression)` gives the minimal DFA. `Automaton.nfa(expression)` gives the Thompson automaton, built in
linear time with epsilon transitions, which avoids the possible blow-up of determinization :

```python
>>> Automaton.nfa("(0|1)*0(0|1){20}").state_count       # the minimal DFA has 2**21 states
66
>>> Automaton.nfa("(0|1)*0(0|1){20}").run([0] + [1] * 20)
True

```

## Errors

A malformed expression raises a `RegexError`, a `ValueError` telling where the problem is :

```python
>>> from autolib import RegexError
>>> try:
...     Regex("(1|2")
... except RegexError as error:
...     print(error.position, error)
4 Expected ')' but reached the end at position 4 in "(1|2"

```
