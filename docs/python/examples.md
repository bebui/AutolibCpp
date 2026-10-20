# Examples

Complete scripts, to copy and run as they are : each one prints what is shown below it. They are run by the
tests, so their outputs are up to date. Some use numpy (`pip install numpy`).

## Validating identifiers

Strings are lists of code points : a set of characters is an `IntervalSet`, and the regular operations build the language of identifiers. Counting and uniform sampling come for free.

```{literalinclude} examples/identifiers.py
:language: python
```

Output :

```{literalinclude} examples/identifiers.out
:language: text
```

## Suspicious sequences in event logs

Events are small integers, and a log is a numpy array of a million of them. A `Searcher` finds every brute-force login in one pass, without copying the array.

```{literalinclude} examples/event_logs.py
:language: python
```

Output :

```{literalinclude} examples/event_logs.out
:language: text
```

## DNA motifs on both strands

A motif with alternatives, and its reverse complement on the other strand : the union of both automata searches both at once.

```{literalinclude} examples/dna_motifs.py
:language: python
```

Output :

```{literalinclude} examples/dna_motifs.out
:language: text
```

## Counting and drawing words

Exact counts with Python integers, the words of a length in order, ranks, and uniform draws among balanced parentheses.

```{literalinclude} examples/binary_words.py
:language: python
```

Output :

```{literalinclude} examples/binary_words.out
:language: text
```

## Spelling suggestions with edit distances

A weighted automaton whose cost is the edit distance to a word : insertions and substitutions read any letter, deletions are epsilon transitions. `k_best_words` lists the closest words, cheapest first.

```{literalinclude} examples/spelling.py
:language: python
```

Output :

```{literalinclude} examples/spelling.out
:language: text
```

## Journeys : time against money

Two costs per leg of a journey. The Pareto front keeps the journeys that no other beats on both costs; a price of time chooses one.

```{literalinclude} examples/journeys.py
:language: python
```

Output :

```{literalinclude} examples/journeys.out
:language: text
```

## Scheduling with hourly prices

The rules of a schedule are an automaton : six hours a day, by blocks of at least two. An unfolding by hour gives each hour its price, and the best path is the cheapest schedule.

```{literalinclude} examples/energy_schedule.py
:language: python
```

Output :

```{literalinclude} examples/energy_schedule.out
:language: text
```
