# autolib

A modern C++20 library for finite automata over integer symbols, with fast algorithms and natural operators.

autolib is a rewrite in C++ of the Java library [fr.menana:automaton](https://github.com/bebui/Automaton) 4.0.0.
It keeps its algorithms and reproduces its results exactly, with a value-semantics API, operator overloading,
a flat memory layout and a modern CMake build.

```cpp
#include <autolib/autolib.hpp>
using namespace autolib::literals;

auto a = "(0|1)*0(0|1){2}"_re;      // minimal DFA of a regular expression
auto b = "(0|1)*11(0|1)*"_re;

auto c = (a | b) & ~"11"_re;         // union, intersection, complement
auto d = a - b;                      // difference
auto e = a + *b;                     // concatenation, Kleene star
bool same = (a - b) == (a & ~b);     // equality of languages : true
bool included = (a & b) <= a;        // inclusion of languages : true
bool accepted = a({1, 0, 1, 1});     // membership : true
auto w = d.shortest_word();          // std::optional<word> : {0, 0, 0}
```

Documentation : [C++](https://bebui.github.io/AutolibCpp/cpp/) · [Python](https://bebui.github.io/AutolibCpp/python/)

The library is also a Python package, with the same algorithms :

```sh
pip install automaton-lib
```

```python
from autolib import Automaton
a = Automaton("(0|1)*0(0|1){2}")
[0, 1, 1] in a & ~Automaton("11")     # True
a.count_words(100) == 2**99            # True, exactly
```

## Features

- **Automata over 32-bit integer symbols**, whose transitions read sets of intervals, so that huge alphabets cost
  nothing; non-deterministic automata and epsilon transitions are supported.
- **Regular expressions** over integers, compiled in linear time by the Thompson construction, with intersection
  `&` and complement `~`.
- **Operators** for the operations of regular languages, and for comparing languages.
- **Fast algorithms** :
  - determinization by subsets on symbol classes, with flat hash tables;
  - minimization by the partition refinement of Valmari and Lehtinen, in O(m log n), or by Brzozowski's method;
  - reduction of non-deterministic automata by the coarsest bisimulation (Paige and Tarjan), which never blows up;
  - products restricted to reachable pairs for intersection, union and differences;
  - equality by Hopcroft and Karp's union-find, inclusion by antichains without determinizing the right operand;
  - shortest words, counter-examples, finiteness, emptiness, universality.
- **Compiled DFAs** for reading many words quickly : a transition table indexed by state and symbol class, the class
  of a symbol being read in an array when the symbols span a small range.
- **Search** of the leftmost-longest factors of a sequence belonging to a language, in linear time.
- **Words of a language** : lazy enumeration in shortlex order, exact counting of the words of a length, uniform
  random sampling, and conversion between words and their ranks in lexicographic order.
- **Weighted automata** : additive costs on the edges, the cost of a word being the least cost of its accepting
  paths. Shortest paths (Dijkstra, DAG, Bellman-Ford), costs of words, of lengths and of prefixes, union,
  concatenation, product, star, exact determinization and minimization, the k cheapest paths and the k cheapest
  distinct words, lazily, compiled evaluators.
- **Automata weighted by vectors** : several costs at once, with Pareto fronts, lexicographic optima,
  scalarizations and products keeping every combination of paths.
- **Exact arithmetic where doubles would round** : the algorithms comparing sums of costs (Bellman-Ford beyond the
  integers, the transformations, the enumerations by cost) count in units of the finest quantum of the weights,
  and round each result once; `dyadic` gives exact sums of doubles.
- **Big integers** without dependency, for counts beyond 64 bits : `big_integer` has the operators of the
  built-in integers, stores the values fitting in 64 bits inline, and takes fast paths on them.

## Operators

| Expression       | Meaning                                              | Result                               |
|------------------|------------------------------------------------------|--------------------------------------|
| `a \| b`         | union                                                | minimal DFA if both are deterministic |
| `a & b`          | intersection                                         | minimal DFA                          |
| `a - b`          | difference                                           | minimal DFA                          |
| `~a`             | complement                                           | complete DFA                         |
| `a + b`          | concatenation                                        | minimal DFA if both are deterministic |
| `*a`, `+a`       | Kleene star, one or more repetitions : prefix forms of `r*` and `r+` | minimal DFA if `a` is deterministic |
| `a.star()`, `a.plus()`, `a.optional()`, `a.repeat(n, m)` | repetitions          | minimal DFA if `a` is deterministic   |
| `a == b`         | same language                                        | `bool`                               |
| `a <= b`, `a < b`, `a >= b`, `a > b` | inclusion of languages           | `bool`                               |
| `a <=> b`        | inclusion as a partial order                         | `std::partial_ordering`              |
| `a(w)`           | membership of a word                                 | `bool`                               |

The compound assignments `|=`, `&=`, `-=` and `+=` are also defined. Interval sets have the same operators
`|`, `&`, `-`, `~`.

## Regular expressions

| Syntax              | Language                                                    |
|---------------------|-------------------------------------------------------------|
| `0` … `9`           | the one-symbol word made of that digit                      |
| `<n>`               | the one-symbol word `n`, any 32-bit integer : `<-3>`, `<42>` |
| `.`                 | every one-symbol word                                       |
| `rs`, `r\|s`        | concatenation, union                                        |
| `r&s`, `~r`         | intersection, complement                                    |
| `r*`, `r+`, `r?`    | repetitions                                                 |
| `r{n}`, `r{n,m}`, `r{n,}`, `r{,m}` | bounded repetitions                          |
| `(r)`, `()`         | grouping, the empty word                                    |

Precedence, from the loosest : `|`, `&`, concatenation, `~`, postfix operators.
Malformed expressions throw `autolib::regex_error`, which tells the position of the error.

```cpp
autolib::regex r("(<10>|<20>)+.{2,}");
auto nfa = r.to_nfa();     // Thompson automaton
auto dfa = r.to_dfa();     // minimal DFA
```

## Building automata by hand

```cpp
autolib::automaton a;
auto s0 = a.add_state();
auto s1 = a.add_state(true);                          // accepting
a.set_initial(s0);
a.add_transition(s0, s1, autolib::interval_set::range(0, 9));
a.add_transition(s1, s1, 0);
a.add_epsilon(s1, s0);
for (const auto& t : a.transitions(s0))
    std::cout << t.target << ' ' << t.labels << '\n';  // 1 {0..9}
```

## Compiled automata and search

```cpp
autolib::compiled_dfa dfa("(1|2)*12"_re);              // immutable, thread safe
bool ok = dfa({2, 1, 2});

autolib::searcher s("12+"_re);
auto m = s.find({3, 1, 2, 2, 4});                      // match{1, 4} : symbols 1 to 3
auto all = s.find_all(text);                           // std::vector<match>
```

## Words of a language

```cpp
auto a = "(0|1)*0(0|1){3}"_re;
for (const word& w : autolib::words(a, 5))          // lazily, in shortlex order
    ...;                                            // {0,0,0,0}, {0,0,0,1}, ... , {1,0,1,1,1}

autolib::big_integer n = autolib::count_words(a, 500);  // 2^499, exactly

std::mt19937_64 rng(42);
auto w = autolib::random_word(a, 500, rng);         // std::optional<word>, uniform among the 2^499 words

autolib::word_sampler sampler(a, 500);              // counts once, then draws many words quickly
word x = sampler(rng);
auto r = sampler.rank(x);                           // rank in lexicographic order, sampler.unrank(*r) == x
```

`big_integer` is a signed integer of arbitrary size :

```cpp
using namespace autolib::literals;
autolib::big_integer x = 1;
x <<= 100;                                          // 2^100
auto y = (x * x - 1) % 1000;                        // 375
auto z = 123456789012345678901234567890_big;
std::cout << z / 7 << ' ' << z.to_double() << '\n';
```

## Weighted automata

```cpp
autolib::weighted_automaton::builder b;
auto s = b.add_state(), t = b.add_state();
b.set_initial(s);
b.set_accept(t, 0.5);                                  // final cost
b.add_transition(s, t, 1, 2.0);                        // reads 1 for 2.0
b.add_transition(s, t, autolib::interval_set::range(0, 9), 3.0);
b.add_transition(t, t, 0, 0.25);
auto a = b.build();

double c = a({1, 0});                                  // 2.75 : the best accepting path
auto best = a.best_path();                             // its edges, symbols and cost
auto cheapest = a.k_best_words(5);                     // the 5 cheapest distinct words
for (const auto& path : a.paths_by_cost())             // every path by increasing cost, lazily
    if (path.cost() > 4) break;
auto d = a.determinize().minimize();                   // exact, with costs pushed toward the start
auto both = a | a.star();                              // least cost over both
double fast = d.compile()({1, 0, 0});                  // evaluator of a deterministic automaton
```

An automaton is weighted directly, and a snapshot lets several weightings share the same graph :

```cpp
auto source = "12+"_re;
auto direct = source.weighted([](const automaton_edge&) { return 2.0; });
double six = direct({1, 2, 2});                        // 6

auto snapshot = source.snapshot();                     // the structure, captured once
auto price = snapshot.weighted([](const automaton_edge& e) { return e.epsilon ? 0.0 : 2.0; });
auto duration = snapshot.weighted([](const automaton_edge&) { return 5.0; });
auto both = snapshot.multi_weighted({[](const automaton_edge&) { return 2.0; },
                                     [](const automaton_edge&) { return 5.0; }});
bool shared = price.graph().same_as(both.graph());     // true : same edges, same numbers
bool member = price.run({1, 2});                       // the language, whatever the costs
autolib::automaton language = price.to_automaton();    // back to an automaton, without the costs
```

Several costs at once :

```cpp
autolib::multi_weighted_automaton::builder m(2);       // time and money
auto p = m.add_state(), q = m.add_state();
m.set_initial(p);
m.set_accept(q);
m.add_transition(p, q, 7, {10, 1});                    // slow and cheap
m.add_transition(p, q, 7, {2, 5});                     // fast and expensive
auto v = m.build();
auto front = v.pareto_front();                         // both paths
auto quick = v.lexicographic_best_path({0, 1});        // time first
auto mix = v.scalarize({1, 2});                        // a scalar automaton : time + 2 money
```

The weighted part follows the conventions of the Java library : exact results where the Java library is exact,
identical paths and orders, and explicit errors (`std::invalid_argument` outside the supported family,
`std::length_error` when a determinization exceeds its budget of states, `std::range_error` when an exact weight is
not a double).

## Using the library

autolib needs a C++20 compiler (GCC 11, Clang 14, MSVC 2022, Apple Clang 15 or later) and CMake 3.25.

With `FetchContent` :

```cmake
include(FetchContent)
FetchContent_Declare(autolib GIT_REPOSITORY https://github.com/bebui/AutolibCpp.git GIT_TAG master)
FetchContent_MakeAvailable(autolib)
target_link_libraries(my_program PRIVATE autolib::autolib)
```

Or installed, then found by `find_package` :

```sh
cmake --preset release
cmake --build build/release
cmake --install build/release --prefix /usr/local
```

```cmake
find_package(autolib REQUIRED)
target_link_libraries(my_program PRIVATE autolib::autolib)
```

## Python

The Python package `automaton-lib`, imported as `autolib`, binds the library with nanobind
(`python/`). Wheels are built for Linux, macOS and Windows by `cibuildwheel`; `pip install .` builds it from the
sources. The guides of `docs/python` hold examples run by the tests :

```sh
pip install . pytest numpy
pytest python/tests
pytest --doctest-glob="*.md" docs/python
```

## Releasing

From GitHub : Actions → "Python wheels" → Run workflow, with the version `1.2.0`. The workflow sets it in
`CMakeLists.txt`, commits, tags `v1.2.0`, builds and tests the wheels of every platform, publishes them on PyPI and
creates the GitHub release.

From a terminal : `tools/release.sh 1.2.0` sets the version, commits, tags and pushes; the tag runs the same build
and publication.

## Developing

```sh
cmake --preset debug && cmake --build --preset debug && ctest --preset debug       # warnings as errors
cmake --preset sanitize && cmake --build --preset sanitize && ctest --preset sanitize  # ASan and UBSan
cmake --preset bench && cmake --build --preset bench && build/bench/benchmarks/autolib_benchmarks
```

The tests check every operation against a direct simulation on all the short words of random automata, and
against the results of the Java library on 600 random expressions (`tests/data/java_oracle.tsv`) and 400 random
weighted automata, cyclic, with epsilon edges and signed weights (`tests/data/java_weighted.tsv`) : costs, k best
paths and words with their edges, determinizations, minimizations and errors, all generated by
`tools/java-oracle`. Big integers and dyadics are checked against the exact numbers of Python (`tools/big-oracle`).

## Performance

Median times on the same machine and identical inputs, against the Java library 4.0.0 on OpenJDK 17
(`benchmarks/benchmarks.cpp` and `tools/java-bench/Bench.java`) :

| Benchmark                                        | Java 4.0.0 | autolib  | Speed-up |
|--------------------------------------------------|-----------:|---------:|---------:|
| Expression to minimal DFA, `(0\|1)*0(0\|1){10}`  | 25.5 ms    | 7.0 ms   | 3.6×     |
| Expression to minimal DFA, `(0\|1)*0(0\|1){14}`  | 534 ms     | 216 ms   | 2.5×     |
| Expression to minimal DFA, 2000 words            | 91.4 ms    | 11.7 ms  | 7.8×     |
| Determinization, random NFA of 40 states         | 4.2 ms     | 2.2 ms   | 2.0×     |
| Determinization, random NFA of 60 states         | 29.6 ms    | 15.0 ms  | 2.0×     |
| Minimization, 32768 states                       | 271 ms     | 109 ms   | 2.5×     |
| Minimization, 20000 states over 2000 symbols     | 355 ms     | 153 ms   | 2.3×     |
| Intersection                                     | 30.6 ms    | 8.8 ms   | 3.5×     |
| Equality of languages                            | 193 µs     | 55 µs    | 3.5×     |
| Inclusion of languages                           | 2.1 ms     | 0.48 ms  | 4.4×     |
| Complement                                       | 383 µs     | 183 µs   | 2.1×     |
| Shortest word                                    | 21 µs      | 5.4 µs   | 3.9×     |
| Reading 10⁶ symbols, automaton                   | 60.0 ms    | 40.3 ms  | 1.5×     |
| Reading 10⁶ symbols, compiled DFA                | 15.8 ms    | 4.9 ms   | 3.2×     |
| Searching every match in 10⁶ symbols             | 28.2 ms    | 17.2 ms  | 1.6×     |
| Counting words of length 60 (64-bit counts)      | 7.5 ms     | 0.77 ms  | 9.7×     |
| Counting words of length 2000 (2000-bit counts)  | 1695 ms    | 253 ms   | 6.7×     |
| Random word of length 1000                       | 455 ms     | 172 ms   | 2.6×     |
| Random word of length 1000, sampler reused       | 455 ms     | 0.71 ms  | 640×     |

Weighted automata, measured back to back on a loaded machine :

| Benchmark                                                      | Java 4.0.0 | autolib  | Speed-up |
|----------------------------------------------------------------|-----------:|---------:|---------:|
| Costs of 1000 words of length 30, NFA of 300 states with epsilon | 2259 ms  | 1359 ms  | 1.7×     |
| Shortest paths, 200000 states (Dijkstra)                       | 446 ms     | 363 ms   | 1.2×     |
| Shortest paths, 400 states with signed fractions (exact Bellman-Ford) | 2.31 ms | 1.65 ms | 1.4×  |
| Exact determinization, 6358 states built                       | 95.8 ms    | 86.9 ms  | 1.1×     |
| Exact minimization, 2000 states                                | 30.0 ms    | 23.7 ms  | 1.3×     |
| 2000 cheapest paths of a cyclic automaton                      | 13.7 ms    | 13.5 ms  | 1.0×     |
| 500 cheapest distinct words of an NFA                          | 19.0 ms    | 13.5 ms  | 1.4×     |
| Pareto front of a DAG weighted by pairs                        | 2.60 ms    | 1.64 ms  | 1.6×     |
| Cost of 10⁶ symbols, compiled weighted DFA                     | 44.1 ms    | 20.2 ms  | 2.2×     |

## License

GNU Lesser General Public License, version 3 or later : see `LICENSE`. The LGPL adds permissions to the GNU General
Public License, version 3, whose text is in `LICENSES/GPL-3.0-or-later.txt`.
