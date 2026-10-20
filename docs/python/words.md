# Words : enumerate, count, sample

## Enumeration

`words(max_length)` lists the accepted words lazily, by length, then lexicographically :

```python
>>> from autolib import Automaton
>>> list(Automaton("(1|2)3?").words(2))
[[1], [2], [1, 3], [2, 3]]
>>> it = Automaton("(0|1)*").words(1000)       # 2**1001 - 1 words : computed one at a time
>>> next(it), next(it), next(it)
([], [0], [1])

```

## Counting

`count_words(length)` counts exactly, with Python integers, whatever their size :

```python
>>> fibonacci = Automaton("~(.*11.*)&(0|1)*")     # binary words without two consecutive 1
>>> [fibonacci.count_words(n) for n in range(10)]
[1, 2, 3, 5, 8, 13, 21, 34, 55, 89]
>>> fibonacci.count_words(1000) > 10**200
True

```

## Uniform sampling

`random_word(length, rng)` draws a word uniformly among the accepted words of that length. It takes a
`random.Random` for reproducible draws :

```python
>>> import random
>>> rng = random.Random(2026)
>>> w = fibonacci.random_word(30, rng)
>>> len(w), w in fibonacci
(30, True)

```

Uniform means uniform over the *words*, not over the paths : rare shapes are drawn as often as common ones.

```python
>>> from collections import Counter
>>> a = Automaton("0000|(1|2){4}")          # one word of zeros, and 16 words of 1 and 2
>>> draws = Counter(tuple(a.random_word(4, rng)) for _ in range(17000))
>>> len(draws), 800 < draws[(0, 0, 0, 0)] < 1200
(17, True)

```

## Ranks

A `WordSampler` numbers the words of one length in lexicographic order. It counts once, then draws, ranks and
unranks quickly, even among astronomically many words :

```python
>>> from autolib import WordSampler
>>> sampler = WordSampler(Automaton("(0|1)*0(0|1){3}"), 20)
>>> sampler.count
524288
>>> sampler[0], sampler[-1][-5:]
([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [1, 0, 1, 1, 1])
>>> sampler.rank([0] * 19 + [1])
1
>>> huge = WordSampler(Automaton(".{200}"), 200)
>>> huge.count == 2**6400
True
>>> w = huge.draw(rng)
>>> huge.unrank(huge.rank(w)) == w
True

```

A use : generating test inputs that follow a format, uniformly, without rejection. Here the times of a day, as
the four digits of `hhmm` :

```python
>>> digit, sixty = "(0|1|2|3|4|5|6|7|8|9)", "(0|1|2|3|4|5)(0|1|2|3|4|5|6|7|8|9)"
>>> time = Automaton(f"((0|1){digit}|2(0|1|2|3)){sixty}")
>>> time.count_words(4)
1440
>>> times = WordSampler(time, 4)
>>> h1, h2, m1, m2 = times.draw(rng)
>>> 0 <= 10 * h1 + h2 < 24 and 0 <= 10 * m1 + m2 < 60
True

```
