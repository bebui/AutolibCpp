"""Finite automata over integer symbols, in C++."""

from collections.abc import Callable, Iterable, Iterator, Sequence
import enum
import fractions
from typing import Annotated, overload

import numpy
from numpy.typing import NDArray

from autolib import RegexError as RegexError


MIN_SYMBOL: int = -2147483648

MAX_SYMBOL: int = 2147483647

class IntervalSet:
    """
    A set of 32-bit integer symbols, stored as sorted disjoint intervals.

    Built from an integer, a ``range`` of step 1, or an iterable of integers and ranges::

        >>> IntervalSet(range(0, 10)) | IntervalSet([20, 21])
        IntervalSet({0..9, 20..21})
        >>> 5 in IntervalSet.range(0, 9)
        True
    """

    @overload
    def __init__(self) -> None:
        """The empty set."""

    @overload
    def __init__(self, symbols: int | range | IntervalSet | Iterable[int | range | IntervalSet]) -> None:
        """
        The set of an integer, a range of step 1, or an iterable of integers and ranges.
        """

    @staticmethod
    def range(min: int, max: int) -> IntervalSet:
        """The symbols from min to max, both included."""

    @staticmethod
    def all() -> IntervalSet:
        """Every 32-bit symbol."""

    @property
    def intervals(self) -> list[tuple[int, int]]:
        """The intervals, as pairs (min, max) of included bounds."""

    @property
    def cardinality(self) -> int:
        """The number of symbols."""

    @property
    def min(self) -> int:
        """The least symbol."""

    @property
    def max(self) -> int:
        """The greatest symbol."""

    def __len__(self) -> int: ...

    def __bool__(self) -> bool: ...

    @overload
    def __contains__(self, arg: int, /) -> bool: ...

    @overload
    def __contains__(self, arg: IntervalSet, /) -> bool: ...

    def __iter__(self) -> object: ...

    def isdisjoint(self, other: IntervalSet) -> bool: ...

    def __or__(self, arg: IntervalSet, /) -> IntervalSet: ...

    def __and__(self, arg: IntervalSet, /) -> IntervalSet: ...

    def __sub__(self, arg: IntervalSet, /) -> IntervalSet: ...

    def __xor__(self, arg: IntervalSet, /) -> IntervalSet: ...

    def __invert__(self) -> IntervalSet: ...

    def __eq__(self, arg: IntervalSet, /) -> bool: ...

    def __le__(self, arg: IntervalSet, /) -> bool: ...

    def __lt__(self, arg: IntervalSet, /) -> bool: ...

    def __ge__(self, arg: IntervalSet, /) -> bool: ...

    def __gt__(self, arg: IntervalSet, /) -> bool: ...

    def __hash__(self) -> int: ...

    def __str__(self) -> str: ...

    def __repr__(self) -> str: ...

class WordIterator:
    """The words of a language, computed one at a time."""

    def __iter__(self) -> object: ...

    def __next__(self) -> list[int]: ...

class Edge:
    """A transition as seen by the cost functions of a weighting."""

    @property
    def source(self) -> int:
        """The state it leaves."""

    @property
    def target(self) -> int:
        """The state it reaches."""

    @property
    def epsilon(self) -> bool:
        """Whether it reads nothing."""

    @property
    def labels(self) -> IntervalSet:
        """The symbols it reads, empty for an epsilon transition."""

    @property
    def index(self) -> int:
        """
        Its rank among the transitions, or the epsilon transitions, of its source.
        """

    def __repr__(self) -> str: ...

class Automaton:
    """
    A finite automaton over 32-bit integer symbols, deterministic or not, with epsilon transitions.

    Operators follow the operations of regular languages::

        >>> a = Automaton("(0|1)*0")        # minimal DFA of a regular expression
        >>> b = Automaton("1(0|1)*")
        >>> c = (a | b) & ~Automaton("11")  # union, intersection, complement
        >>> [1, 1, 0] in a                   # membership
        True
        >>> (a - b) == (a & ~b)              # equality of languages
        True
        >>> a <= (a | b)                     # inclusion of languages
        True

    Automata are values : operations return new automata.
    """

    @overload
    def __init__(self) -> None:
        """The automaton without any state, recognizing nothing."""

    @overload
    def __init__(self, expression: str) -> None:
        """The minimal DFA of a regular expression over integers."""

    @staticmethod
    def from_regex(expression: str) -> Automaton:
        """The minimal DFA of a regular expression."""

    @staticmethod
    def nfa(expression: str) -> Automaton:
        """
        The Thompson automaton of a regular expression, built in linear time, with epsilon transitions.
        """

    @staticmethod
    def empty_language() -> Automaton:
        """Recognizes no word."""

    @staticmethod
    def empty_word() -> Automaton:
        """Recognizes the empty word only."""

    @staticmethod
    def symbols(symbols: int | range | IntervalSet | Iterable[int | range | IntervalSet]) -> Automaton:
        """Recognizes the words of one symbol among some symbols."""

    @staticmethod
    def any_symbol() -> Automaton:
        """Recognizes every word of one symbol."""

    @staticmethod
    def universal() -> Automaton:
        """Recognizes every word."""

    @staticmethod
    def of_word(word: Sequence[int]) -> Automaton:
        """Recognizes one word."""

    def add_state(self, accept: bool = False) -> int:
        """Adds a state and returns its number."""

    @property
    def state_count(self) -> int:
        """The number of states."""

    @property
    def transition_count(self) -> int:
        """The number of transitions, epsilon transitions included."""

    @property
    def initial(self) -> int | None:
        """The initial state, or None."""

    @initial.setter
    def initial(self, arg: int, /) -> None: ...

    def set_initial(self, state: int) -> None: ...

    def is_accept(self, state: int) -> bool: ...

    def set_accept(self, state: int, accept: bool = True) -> None: ...

    def add_transition(self, source: int, target: int, labels: int | range | IntervalSet | Iterable[int | range | IntervalSet]) -> None:
        """
        Adds a transition reading an integer, a range, an IntervalSet or an iterable of them.
        """

    def add_epsilon(self, source: int, target: int) -> None:
        """Adds an epsilon transition."""

    def transitions(self, state: int) -> list[tuple[int, IntervalSet]]:
        """The transitions leaving a state, as pairs (target, labels)."""

    def epsilons(self, state: int) -> list[int]:
        """The targets of the epsilon transitions leaving a state."""

    @property
    def is_deterministic(self) -> bool: ...

    def step(self, state: int, symbol: int) -> int | None:
        """The state reached from a state of a deterministic automaton, or None."""

    def trim(self) -> None:
        """Removes the useless states, in place."""

    def run(self, word: Sequence[int]) -> bool:
        """Whether a word is accepted."""

    def __contains__(self, arg: Sequence[int], /) -> bool: ...

    def __call__(self, word: Sequence[int]) -> bool: ...

    def determinize(self) -> Automaton: ...

    def minimize(self, algorithm: str = 'partition') -> Automaton:
        """The minimal DFA, by partition refinement or by Brzozowski's method."""

    def remove_epsilon(self) -> Automaton: ...

    def reduce(self) -> Automaton:
        """
        An equivalent automaton whose states are merged by bisimulation : never blows up.
        """

    def reverse(self) -> Automaton:
        """The automaton of the mirror language."""

    def star(self) -> Automaton:
        """Kleene star."""

    def plus(self) -> Automaton:
        """One or more repetitions."""

    def optional(self) -> Automaton:
        """Zero or one occurrence."""

    def repeat(self, min: int, max: int | None = None) -> Automaton:
        """From min to max repetitions, None meaning unbounded."""

    def __or__(self, arg: Automaton, /) -> Automaton: ...

    def __and__(self, arg: Automaton, /) -> Automaton: ...

    def __sub__(self, arg: Automaton, /) -> Automaton: ...

    def __xor__(self, arg: Automaton, /) -> Automaton: ...

    def __add__(self, arg: Automaton, /) -> Automaton: ...

    def __invert__(self) -> Automaton: ...

    def __pos__(self) -> Automaton: ...

    def __mul__(self, count: int) -> Automaton: ...

    def __pow__(self, count: int) -> Automaton: ...

    def complement(self) -> Automaton: ...

    def concatenate(self, other: Automaton) -> Automaton: ...

    def is_empty(self) -> bool: ...

    def is_universal(self) -> bool: ...

    def is_finite(self) -> bool: ...

    def same_language(self, other: Automaton) -> bool: ...

    def is_subset_of(self, other: Automaton) -> bool: ...

    def shortest_word(self) -> list[int] | None:
        """A shortest accepted word, or None."""

    def counter_example(self, other: Automaton) -> list[int] | None:
        """
        A shortest word accepted by one automaton only, or None if both languages are equal.
        """

    def __eq__(self, arg: Automaton, /) -> bool: ...

    def __ne__(self, arg: Automaton, /) -> bool: ...

    def __le__(self, arg: Automaton, /) -> bool: ...

    def __lt__(self, arg: Automaton, /) -> bool: ...

    def __ge__(self, arg: Automaton, /) -> bool: ...

    def __gt__(self, arg: Automaton, /) -> bool: ...

    def words(self, max_length: int) -> WordIterator:
        """
        The accepted words up to a length, lazily, in shortlex order : by length, then lexicographically.
        """

    def count_words(self, length: int) -> int:
        """The number of accepted words of a length, exactly."""

    def random_word(self, length: int, rng: object | None = None) -> list[int] | None:
        """
        A word of a length drawn uniformly among the accepted ones, or None; rng is a random.Random, or the random module.
        """

    def compile(self, lookup_table: bool = True) -> CompiledDFA:
        """A compiled DFA, to read many words quickly."""

    def searcher(self) -> Searcher:
        """A searcher of the factors of texts in the language."""

    def to_dot(self) -> str:
        """The Graphviz representation."""

    def __str__(self) -> str: ...

    def __repr__(self) -> str: ...

    def __copy__(self) -> Automaton: ...

    def __deepcopy__(self, memo: object) -> Automaton: ...

    __hash__: None = None

    def snapshot(self) -> AutomatonSnapshot:
        """The structure captured once, for several weightings."""

    def weighted(self, cost: float | Callable[[Edge], float]) -> WeightedAutomaton:
        """
        This automaton with a cost per transition : a number or a function of an Edge.
        """

    def multi_weighted(self, *costs: float | Callable[[Edge], float]) -> MultiWeightedAutomaton:
        """This automaton weighted by vectors, one cost per dimension."""

class Regex:
    """
    A parsed regular expression over integer symbols.

    Symbols are the digits 0 to 9 or integers between angle brackets, ``<-3>``, ``<42>``; ``.`` is any symbol.
    Operators : concatenation, ``|`` union, ``&`` intersection, ``~`` complement, ``*``, ``+``, ``?``, ``{n}``,
    ``{n,m}``, ``{n,}``, ``{,m}``, and ``()`` for the empty word::

        >>> Regex("(<10>|<20>)+.{2,}").to_dfa().shortest_word()
        [10, -2147483648, -2147483648]
    """

    def __init__(self, expression: str) -> None: ...

    def to_nfa(self) -> Automaton:
        """The Thompson automaton, built in linear time."""

    def to_dfa(self) -> Automaton:
        """The minimal DFA."""

    def __str__(self) -> str: ...

    def __repr__(self) -> str: ...

class WordSampler:
    """
    The words of one length of a language, numbered in lexicographic order.

    Counts once, then draws, ranks and unranks words quickly::

        >>> sampler = WordSampler(Automaton("(0|1)*0(0|1){3}"), 20)
        >>> sampler.count
        524288
        >>> w = sampler.draw()
        >>> sampler.unrank(sampler.rank(w)) == w
        True
    """

    def __init__(self, automaton: Automaton, length: int) -> None: ...

    @property
    def length(self) -> int: ...

    @property
    def count(self) -> int:
        """The number of words, exactly."""

    def __len__(self) -> int: ...

    def __bool__(self) -> bool: ...

    def unrank(self, rank: int) -> list[int]:
        """The word of a rank, from 0 to count - 1."""

    def __getitem__(self, arg: int, /) -> list[int]: ...

    def rank(self, word: Sequence[int]) -> int | None:
        """The rank of a word, or None if it is not one of the words."""

    def draw(self, rng: object | None = None) -> list[int]:
        """A word drawn uniformly, with a random.Random, or the random module."""

class CompiledDFA:
    """
    An immutable DFA compiled into flat arrays, to read many words quickly.

    Accepts lists and numpy arrays of integers; arrays of int32 are read in place, and long words without the GIL.
    """

    def __init__(self, automaton: Automaton, lookup_table: bool = True) -> None: ...

    @property
    def state_count(self) -> int: ...

    @property
    def has_lookup_table(self) -> bool: ...

    def run(self, word: Sequence[int]) -> bool: ...

    def __call__(self, word: Sequence[int]) -> bool: ...

    def __contains__(self, arg: Sequence[int], /) -> bool: ...

class Match:
    """A factor text[start:end] of a text."""

    def __init__(self, start: int, end: int) -> None: ...

    @property
    def start(self) -> int: ...

    @property
    def end(self) -> int: ...

    def __len__(self) -> int: ...

    def __eq__(self, arg: Match, /) -> bool: ...

    def __iter__(self) -> Iterator: ...

    def __repr__(self) -> str: ...

class Searcher:
    """
    Searches the leftmost-longest factors of sequences belonging to a language, in linear time.

        >>> s = Searcher(Automaton("12+"))
        >>> s.find([3, 1, 2, 2, 4])
        Match(1, 4)
    """

    def __init__(self, language: Automaton) -> None: ...

    def find(self, text: Sequence[int], start: int = 0) -> Match | None:
        """The leftmost-longest match from a position, or None."""

    def find_all(self, text: Sequence[int]) -> list[Match]:
        """The successive non-overlapping leftmost-longest matches."""

    def contains(self, text: Sequence[int]) -> bool:
        """Whether a factor of the text belongs to the language."""

class AutomatonGraph:
    """
    An immutable multigraph whose edges read sets of symbols, or nothing.

    The structure of the weighted automata : weightings of the same graph share it. Edges keep the numbers given
    by their order of creation, and parallel edges are never merged.
    """

    def __init__(self) -> None:
        """The graph without any state."""

    class Builder:
        """Builds graphs; building takes a snapshot."""

        def __init__(self) -> None: ...

        def add_state(self, accept: bool = False) -> int: ...

        def set_initial(self, state: int) -> None: ...

        def set_accept(self, state: int, accept: bool = True) -> None: ...

        def add_transition(self, source: int, target: int, labels: int | range | IntervalSet | Iterable[int | range | IntervalSet]) -> int: ...

        def add_epsilon(self, source: int, target: int) -> int: ...

        def build(self) -> AutomatonGraph: ...

    @staticmethod
    def from_automaton(automaton: Automaton) -> AutomatonGraph: ...

    @property
    def state_count(self) -> int: ...

    @property
    def edge_count(self) -> int: ...

    @property
    def initial(self) -> int | None: ...

    def is_accept(self, state: int) -> bool: ...

    def source(self, edge: int) -> int: ...

    def target(self, edge: int) -> int: ...

    def is_epsilon(self, edge: int) -> bool: ...

    def labels(self, edge: int) -> IntervalSet: ...

    def accepts(self, edge: int, symbol: int) -> bool: ...

    def outgoing(self, state: int) -> list[int]: ...

    def incoming(self, state: int) -> list[int]: ...

    @property
    def is_acyclic(self) -> bool: ...

    @property
    def is_deterministic(self) -> bool: ...

    def topological_order(self) -> list[int]: ...

    def useful_states(self) -> list[bool]: ...

    def coaccessible_states(self) -> list[bool]: ...

    def run(self, word: Sequence[int]) -> bool: ...

    def __contains__(self, arg: Sequence[int], /) -> bool: ...

    def to_automaton(self) -> Automaton: ...

    def to_dot(self, highlight: Sequence[int] | None = None) -> str:
        """The Graphviz description, the edges of highlight drawn in colour."""

    def same_as(self, other: AutomatonGraph) -> bool:
        """Whether both designate the same graph."""

    def __repr__(self) -> str: ...

class WeightedPath:
    """A path : its edges, the symbols it reads and its cost."""

    @property
    def graph(self) -> AutomatonGraph: ...

    @property
    def start(self) -> int: ...

    @property
    def end(self) -> int: ...

    @property
    def edges(self) -> list[int]: ...

    @property
    def symbols(self) -> list[int]:
        """The word read, the least symbol of each edge unless a query chose."""

    @property
    def edge_count(self) -> int: ...

    @property
    def cost(self) -> float: ...

    def __len__(self) -> int: ...

    def __repr__(self) -> str: ...

class WeightedWord:
    """A word and its cost, the least cost of its accepting paths."""

    @property
    def symbols(self) -> list[int]: ...

    @property
    def cost(self) -> float: ...

    def __len__(self) -> int: ...

    def __iter__(self) -> Iterator: ...

    def __repr__(self) -> str: ...

class ShortestPaths:
    """Shortest distances from a state, with the best paths."""

    class Status(enum.Enum):
        FINITE = 0

        UNREACHABLE = 1

        UNBOUNDED = 2

    def distance_to(self, state: int) -> float: ...

    def distances(self) -> list[float]: ...

    def status_to(self, state: int) -> ShortestPaths.Status: ...

    def path_to(self, state: int) -> WeightedPath | None: ...

    @property
    def optimal_cost(self) -> float: ...

    def best_path(self) -> WeightedPath | None: ...

class PathAnalysis:
    """Distances in both directions : costs through a state or an edge."""

    def __init__(self, automaton: WeightedAutomaton) -> None: ...

    @property
    def forward(self) -> ShortestPaths: ...

    @property
    def backward(self) -> list[float]: ...

    def cost_through_state(self, state: int) -> float: ...

    def cost_through_edge(self, edge: int) -> float: ...

class PathIterator:
    """Accepting paths by increasing cost, computed one at a time."""

    def __iter__(self) -> object: ...

    def __next__(self) -> WeightedPath: ...

class WordCostIterator:
    """Distinct words by increasing cost, computed one at a time."""

    def __iter__(self) -> object: ...

    def __next__(self) -> WeightedWord: ...

class WeightedAutomaton:
    """
    Additive costs on the edges of a graph : the cost of a word is the least cost of its accepting paths.

        >>> b = WeightedAutomaton.Builder()
        >>> s, t = b.add_state(), b.add_state()
        >>> b.set_initial(s)
        >>> b.set_accept(t, 0.5)
        >>> _ = b.add_transition(s, t, 1, 2.0)
        >>> _ = b.add_transition(s, t, range(0, 10), 3.0)
        >>> a = b.build()
        >>> a([1])
        2.5

    A word not accepted costs inf, and -inf when negative cycles make its cost unbounded.
    """

    def __init__(self) -> None:
        """The automaton without any state."""

    class Builder:
        """Builds weighted automata : the graph and its costs together."""

        def __init__(self) -> None: ...

        def add_state(self) -> int: ...

        def set_initial(self, state: int, cost: float = 0.0) -> None: ...

        def set_accept(self, state: int, cost: float = 0.0) -> None: ...

        def add_transition(self, source: int, target: int, labels: int | range | IntervalSet | Iterable[int | range | IntervalSet], cost: float) -> int: ...

        def add_epsilon(self, source: int, target: int, cost: float) -> int: ...

        def build(self) -> WeightedAutomaton: ...

    class Evaluator:
        """Evaluates many words, reusing its buffers."""

        def cost(self, word: Sequence[int]) -> float: ...

        def __call__(self, word: Sequence[int]) -> float: ...

        def optimal_cost_with_prefix(self, prefix: Sequence[int]) -> float: ...

    @staticmethod
    def of(graph: AutomatonGraph, weights: Sequence[float], initial: float = 0.0, finals: Sequence[float] | None = None) -> WeightedAutomaton:
        """
        A weighting of a graph : one weight per edge, and optionally an initial cost and one final cost per state.
        """

    @staticmethod
    def from_automaton(automaton: Automaton, cost: float | Callable[[Edge], float]) -> WeightedAutomaton: ...

    @property
    def graph(self) -> AutomatonGraph: ...

    def weight(self, edge: int) -> float: ...

    @property
    def weights(self) -> list[float]: ...

    @property
    def initial_weight(self) -> float: ...

    def final_weight(self, state: int) -> float: ...

    def with_weights(self, weights: Sequence[float]) -> WeightedAutomaton: ...

    def cost(self, word: Sequence[int]) -> float: ...

    def __call__(self, word: Sequence[int]) -> float: ...

    def run(self, word: Sequence[int]) -> bool:
        """Whether an accepting path reads the word, whatever the costs."""

    def __contains__(self, arg: Sequence[int], /) -> bool: ...

    def to_automaton(self) -> Automaton:
        """The automaton of the language, without the costs."""

    @overload
    def to_dot(self, highlight: WeightedPath) -> str: ...

    @overload
    def to_dot(self, highlight: Sequence[int] | None = None) -> str:
        """
        The Graphviz description : symbols / cost on the edges, final costs under the accepting states; the edges of highlight, a WeightedPath or a list of edges, are drawn in colour.
        """

    def evaluator(self) -> WeightedAutomaton.Evaluator: ...

    def optimal_cost_with_prefix(self, prefix: Sequence[int]) -> float: ...

    @property
    def optimal_cost(self) -> float:
        """The best cost of the accepting paths."""

    def best_path(self, word: Sequence[int] | None = None) -> WeightedPath | None:
        """A best accepting path, reading a word if given, or None."""

    def cost_of_length(self, length: int) -> float: ...

    def cost_of_length_at_most(self, length: int) -> float: ...

    def best_path_of_length(self, length: int) -> WeightedPath | None: ...

    def best_word_of_length(self, length: int) -> list[int] | None: ...

    def __or__(self, arg: WeightedAutomaton, /) -> WeightedAutomaton: ...

    def __and__(self, arg: WeightedAutomaton, /) -> WeightedAutomaton: ...

    def __add__(self, arg: WeightedAutomaton, /) -> WeightedAutomaton: ...

    def __pos__(self) -> WeightedAutomaton: ...

    def union(self, other: WeightedAutomaton) -> WeightedAutomaton: ...

    def intersection(self, other: WeightedAutomaton) -> WeightedAutomaton: ...

    def concatenate(self, other: WeightedAutomaton) -> WeightedAutomaton: ...

    def reverse(self) -> WeightedAutomaton: ...

    def star(self) -> WeightedAutomaton: ...

    def plus(self) -> WeightedAutomaton: ...

    def repeat(self, min: int, max: int | None = None) -> WeightedAutomaton: ...

    def trim(self) -> WeightedAutomaton: ...

    def simplify(self) -> WeightedAutomaton: ...

    def remove_epsilon(self) -> WeightedAutomaton: ...

    def determinize(self, max_states: int = 10000) -> WeightedAutomaton:
        """
        An equivalent deterministic automaton, computed exactly; ValueError beyond max_states states.
        """

    def minimize(self) -> WeightedAutomaton:
        """The minimal deterministic automaton with the same costs."""

    def compile(self) -> CompiledWeightedDFA: ...

    def k_best_paths(self, k: int) -> list[WeightedPath]: ...

    def k_best_words(self, k: int) -> list[WeightedWord]: ...

    def paths_by_cost(self) -> PathIterator:
        """The accepting paths by increasing cost, lazily."""

    def words_by_cost(self) -> WordCostIterator:
        """The distinct words by increasing cost, lazily, without determinizing."""

    def __repr__(self) -> str: ...

def shortest_paths(automaton: WeightedAutomaton, reconstruct: bool = True) -> ShortestPaths: ...

def shortest_paths_from(automaton: WeightedAutomaton, source: int) -> ShortestPaths: ...

def dag_shortest_paths(automaton: WeightedAutomaton) -> ShortestPaths: ...

def dag_longest_paths(automaton: WeightedAutomaton) -> ShortestPaths: ...

def dijkstra(automaton: WeightedAutomaton) -> ShortestPaths: ...

def bellman_ford(automaton: WeightedAutomaton) -> ShortestPaths: ...

def backward_distances(automaton: WeightedAutomaton) -> list[float]: ...

def distances_to(automaton: WeightedAutomaton, destination: int) -> list[float]: ...

def cost_through_state(automaton: WeightedAutomaton, state: int) -> float: ...

def cost_through_edge(automaton: WeightedAutomaton, edge: int) -> float: ...

def determinize_dag(automaton: WeightedAutomaton, max_states: int = 18446744073709551615) -> WeightedAutomaton: ...

def minimize_dag(automaton: WeightedAutomaton) -> WeightedAutomaton: ...

def k_best_paths_of_length(automaton: WeightedAutomaton, length: int, k: int) -> list[WeightedPath]: ...

def k_best_words_of_length(automaton: WeightedAutomaton, length: int, k: int) -> list[WeightedWord]: ...

class MultiWeightedPath:
    """An accepting path and the vector of its costs."""

    @property
    def graph(self) -> AutomatonGraph: ...

    @property
    def start(self) -> int: ...

    @property
    def end(self) -> int: ...

    @property
    def edges(self) -> list[int]: ...

    @property
    def symbols(self) -> list[int]: ...

    @property
    def costs(self) -> list[float]: ...

    def cost(self, dimension: int) -> float: ...

    def exact_cost(self, dimension: int) -> fractions.Fraction:
        """The exact sum, as a fractions.Fraction."""

    def __len__(self) -> int: ...

    def __repr__(self) -> str: ...

class ParetoFront:
    """The non-dominated cost vectors, each with a path."""

    @property
    def paths(self) -> list[MultiWeightedPath]: ...

    @property
    def includes_all_paths(self) -> bool: ...

    @property
    def is_complete(self) -> bool: ...

    def __len__(self) -> int: ...

    def __iter__(self) -> Iterator: ...

    def __getitem__(self, arg: int, /) -> MultiWeightedPath: ...

def dominates(x: Sequence[float], y: Sequence[float]) -> bool:
    """
    Whether x dominates y for minimization : smaller or equal everywhere, smaller somewhere.
    """

class MultiWeightedAutomaton:
    """
    An automaton weighted by vectors : several additive costs at once.

        >>> b = MultiWeightedAutomaton.Builder(2)     # time and money
        >>> p, q = b.add_state(), b.add_state()
        >>> b.set_initial(p)
        >>> b.set_accept(q)
        >>> _ = b.add_transition(p, q, 7, [10, 1])
        >>> _ = b.add_transition(p, q, 7, [2, 5])
        >>> len(b.build().pareto_front())
        2
    """

    class Builder:
        """Builds automata weighted by vectors."""

        def __init__(self, dimensions: int) -> None: ...

        def add_state(self) -> int: ...

        def set_initial(self, state: int, costs: Sequence[float] | None = None) -> None: ...

        def set_accept(self, state: int, costs: Sequence[float] | None = None) -> None: ...

        def add_transition(self, source: int, target: int, labels: int | range | IntervalSet | Iterable[int | range | IntervalSet], costs: Sequence[float]) -> int: ...

        def add_epsilon(self, source: int, target: int, costs: Sequence[float]) -> int: ...

        def build(self) -> MultiWeightedAutomaton: ...

    @staticmethod
    def of(graph: AutomatonGraph, dimensions: int, weights: Sequence[float], initial: Sequence[float] | None = None, finals: Sequence[float] | None = None) -> MultiWeightedAutomaton: ...

    @staticmethod
    def from_weightings(weightings: Sequence[WeightedAutomaton]) -> MultiWeightedAutomaton:
        """
        Scalar weightings of one graph as the dimensions of a vector : costs stay correlated.
        """

    @staticmethod
    def synchronized_product(automata: Sequence[WeightedAutomaton]) -> MultiWeightedAutomaton:
        """
        The product on common words, one dimension per operand, every combination of paths kept.
        """

    @property
    def graph(self) -> AutomatonGraph: ...

    @property
    def dimensions(self) -> int: ...

    def weight(self, edge: int, dimension: int) -> float: ...

    def weights(self, edge: int) -> list[float]: ...

    @property
    def initial_weights(self) -> list[float]: ...

    def final_weights(self, state: int) -> list[float]: ...

    def run(self, word: Sequence[int]) -> bool: ...

    def __contains__(self, arg: Sequence[int], /) -> bool: ...

    def to_automaton(self) -> Automaton: ...

    @overload
    def to_dot(self, highlight: MultiWeightedPath) -> str: ...

    @overload
    def to_dot(self, highlight: Sequence[int] | None = None) -> str:
        """
        The Graphviz description with the vectors of costs; the edges of highlight, a MultiWeightedPath or a list of edges, are drawn in colour.
        """

    def project(self, dimension: int) -> WeightedAutomaton: ...

    def select_dimensions(self, dimensions: Sequence[int]) -> MultiWeightedAutomaton: ...

    def scalarize(self, coefficients: Sequence[float]) -> WeightedAutomaton:
        """The scalar automaton of a linear combination of the dimensions."""

    def path_costs(self, path: WeightedPath) -> list[float]: ...

    def lexicographic_best_path(self, priority: Sequence[int] | None = None) -> MultiWeightedPath | None:
        """The best accepting path for the lexicographic order of the dimensions."""

    def pareto_front(self, all_paths: bool = False) -> ParetoFront: ...

    def pareto_for_word(self, word: Sequence[int]) -> ParetoFront: ...

    def __or__(self, arg: MultiWeightedAutomaton, /) -> MultiWeightedAutomaton: ...

    def __and__(self, arg: MultiWeightedAutomaton, /) -> MultiWeightedAutomaton: ...

    def __add__(self, arg: MultiWeightedAutomaton, /) -> MultiWeightedAutomaton: ...

    def reverse(self) -> MultiWeightedAutomaton: ...

    def trim(self) -> MultiWeightedAutomaton: ...

    def simplify(self) -> MultiWeightedAutomaton: ...

    def product_independent(self, other: MultiWeightedAutomaton) -> MultiWeightedAutomaton: ...

    def concatenate_independent(self, other: MultiWeightedAutomaton) -> MultiWeightedAutomaton: ...

    def compile(self) -> CompiledMultiWeightedDFA: ...

    def __repr__(self) -> str: ...

class AutomatonSnapshot:
    """
    The structure of an automaton captured once, to weight it several times on the same graph.

        >>> snapshot = Automaton("12+").snapshot()
        >>> price = snapshot.weighted(2.0)
        >>> both = snapshot.multi_weighted(2.0, lambda edge: 0.0 if edge.epsilon else 5.0)
        >>> price.graph.same_as(both.graph)
        True
    """

    def __init__(self, automaton: Automaton) -> None: ...

    @property
    def graph(self) -> AutomatonGraph: ...

    def origin(self, edge: int) -> Edge:
        """The transition of the automaton an edge comes from."""

    def weighted(self, cost: float | Callable[[Edge], float], initial: float = 0.0, final: float | Callable[[int], float] | None = None) -> WeightedAutomaton:
        """
        A weighting : cost is a number or a function of an Edge, final a number or a function of an accepting state.
        """

    def multi_weighted(self, *costs: float | Callable[[Edge], float]) -> MultiWeightedAutomaton:
        """
        A weighting by vectors, one cost per dimension : numbers or functions of an Edge.
        """

    def run(self, word: Sequence[int]) -> bool: ...

    def __contains__(self, arg: Sequence[int], /) -> bool: ...

    def to_automaton(self) -> Automaton: ...

class LayeredAutomaton:
    """
    The unfolding of a graph by the number of symbols read, up to a horizon.

    Built once, evaluated with any weighting of the base graph, or costs depending on the position.
    """

    class Analysis:
        """The distances of one weighting of an unfolding."""

        def forward_distances(self) -> list[float]: ...

        def backward_distances(self) -> list[float]: ...

        @property
        def optimal_cost(self) -> float: ...

        def best_path(self) -> WeightedPath | None: ...

        def cost_at(self, position: int, symbol: int) -> float:
            """The best cost of the accepting paths reading a symbol at a position."""

    @staticmethod
    def exactly(graph: AutomatonGraph, horizon: int) -> LayeredAutomaton: ...

    @staticmethod
    def at_most(graph: AutomatonGraph, horizon: int) -> LayeredAutomaton: ...

    @staticmethod
    def for_word(graph: AutomatonGraph, word: Sequence[int]) -> LayeredAutomaton: ...

    @property
    def base_graph(self) -> AutomatonGraph: ...

    @property
    def graph(self) -> AutomatonGraph: ...

    @property
    def horizon(self) -> int: ...

    def original_state(self, state: int) -> int: ...

    def position(self, state: int) -> int: ...

    def original_edge(self, edge: int) -> int: ...

    @overload
    def evaluate(self, automaton: WeightedAutomaton, weights: Callable[[int, int], float] | None = None) -> WeightedAutomaton:
        """
        The weighting of the unfolding; weights(position, edge) gives costs depending on the position.
        """

    @overload
    def evaluate(self, automaton: MultiWeightedAutomaton) -> MultiWeightedAutomaton: ...

    def best_path(self, automaton: WeightedAutomaton) -> WeightedPath | None: ...

    @overload
    def map(self, path: WeightedPath) -> WeightedPath: ...

    @overload
    def map(self, path: MultiWeightedPath) -> MultiWeightedPath: ...

    def analyze(self, automaton: WeightedAutomaton, weights: Callable[[int, int], float] | None = None) -> LayeredAutomaton.Analysis: ...

class WeightedDFAIndex:
    """The transitions of a deterministic graph, shared by its weightings."""

    def __init__(self, graph: AutomatonGraph) -> None: ...

    @property
    def graph(self) -> AutomatonGraph: ...

    def transition(self, state: int | None, symbol: int) -> int | None: ...

class CompiledWeightedDFA:
    """The fast evaluator of a deterministic weighted automaton."""

    def __init__(self, automaton: WeightedAutomaton) -> None: ...

    @property
    def index(self) -> WeightedDFAIndex: ...

    def reweight(self, automaton: WeightedAutomaton) -> CompiledWeightedDFA: ...

    def cost(self, word: Sequence[int]) -> float: ...

    def __call__(self, word: Sequence[int]) -> float: ...

class CompiledMultiWeightedDFA:
    """The fast evaluator of a deterministic automaton weighted by vectors."""

    def __init__(self, automaton: MultiWeightedAutomaton) -> None: ...

    @property
    def dimensions(self) -> int: ...

    @property
    def graph(self) -> AutomatonGraph: ...

    def reweight(self, automaton: MultiWeightedAutomaton) -> CompiledMultiWeightedDFA: ...

    def project(self, dimension: int) -> CompiledWeightedDFA: ...

    def scalarize(self, coefficients: Sequence[float]) -> CompiledWeightedDFA: ...

    def costs(self, word: Sequence[int]) -> list[float] | None:
        """The cost vector of a word, or None if it is not accepted."""

    def costs_into(self, word: Sequence[int], out: Annotated[NDArray[numpy.float64], dict(shape=(None,), order='C')]) -> bool:
        """
        Writes the costs of a word in a numpy array, without allocating; returns whether it is accepted.
        """
