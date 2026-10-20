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
auto e = a + b.star();               // concatenation, Kleene star
bool same = (a - b) == (a & ~b);     // equality of languages : true
bool included = (a & b) <= a;        // inclusion of languages : true
bool accepted = a({1, 0, 1, 1});     // membership : true
auto w = d.shortest_word();          // std::optional<word> : {0, 0, 0}
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

## Developing

```sh
cmake --preset debug && cmake --build --preset debug && ctest --preset debug       # warnings as errors
cmake --preset sanitize && cmake --build --preset sanitize && ctest --preset sanitize  # ASan and UBSan
cmake --preset bench && cmake --build --preset bench && build/bench/benchmarks/autolib_benchmarks
```

The tests check every operation against a direct simulation on all the short words of random automata, and
against the results of the Java library on 600 random expressions (`tests/data/java_oracle.tsv`, generated by
`tools/java-oracle`). Big integers are checked against the integers of Python (`tools/big-oracle`).

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

## License

GNU Lesser General Public License, version 3 or later : see `COPYING.LESSER` and `COPYING`.
