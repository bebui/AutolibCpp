// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (c) 2025-2026 Julien Menana

// Graphs, snapshots, weighted and multi-weighted automata, unfoldings and compiled evaluators.

#include <nanobind/stl/function.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/variant.h>

#include <cmath>
#include <memory>
#include <sstream>

#include "casters.hpp"

namespace autolib::python {

using release_gil = nb::call_guard<nb::gil_scoped_release>;

namespace {

std::vector<edge_cost> to_edge_costs(nb::args costs) {
    std::vector<edge_cost> out;
    for (nb::handle c : costs)
        out.push_back(nb::cast<cost_arg>(c).value);
    return out;
}

template <class T>
std::vector<T> to_vector(std::span<const T> s) {
    return {s.begin(), s.end()};
}

std::string cost_text(double c) {
    std::ostringstream out;
    out << c;
    return out.str();
}

std::string word_text(const word& w) {
    std::string out = "[";
    for (std::size_t i = 0; i < w.size(); ++i)
        out += (i ? ", " : "") + std::to_string(w[i]);
    return out + "]";
}

/// A Python iterator over a lazy sequence.
template <class T>
struct lazy_iterator {
    std::shared_ptr<lazy_sequence<T>> sequence;
};

template <class T>
void bind_lazy(nb::module_& m, const char* name, const char* doc) {
    nb::class_<lazy_iterator<T>>(m, name, doc)
        .def("__iter__", [](nb::handle self) { return nb::borrow(self); })
        .def("__next__", [](lazy_iterator<T>& it) {
            std::optional<T> next;
            {
                nb::gil_scoped_release release;
                next = it.sequence->next();
            }
            if (!next)
                throw nb::stop_iteration();
            return std::move(*next);
        });
}

template <class T>
lazy_iterator<T> make_lazy(lazy_sequence<T> sequence) {
    return {std::make_shared<lazy_sequence<T>>(std::move(sequence))};
}

}  // namespace

void bind_weighted(nb::module_& m) {
    // ------------------------------------------------------------------ AutomatonGraph

    nb::class_<automaton_graph> graph(m, "AutomatonGraph", R"(An immutable multigraph whose edges read sets of symbols, or nothing.

The structure of the weighted automata : weightings of the same graph share it. Edges keep the numbers given
by their order of creation, and parallel edges are never merged.
)");
    nb::class_<automaton_graph::builder>(graph, "Builder", "Builds graphs; building takes a snapshot.")
        .def(nb::init<>())
        .def("add_state", &automaton_graph::builder::add_state, nb::arg("accept") = false)
        .def("set_initial", &automaton_graph::builder::set_initial, nb::arg("state"))
        .def("set_accept", &automaton_graph::builder::set_accept, nb::arg("state"), nb::arg("accept") = true)
        .def("add_transition", [](automaton_graph::builder& b, state_id from, state_id to, const labels_arg& labels) {
            return b.add_transition(from, to, labels.value());
        }, nb::arg("source"), nb::arg("target"), nb::arg("labels"))
        .def("add_epsilon", &automaton_graph::builder::add_epsilon, nb::arg("source"), nb::arg("target"))
        .def("build", &automaton_graph::builder::build);
    graph.def(nb::init<>(), "The graph without any state.")
        .def_static("from_automaton", &automaton_graph::from, nb::arg("automaton"))
        .def_prop_ro("state_count", &automaton_graph::state_count)
        .def_prop_ro("edge_count", &automaton_graph::edge_count)
        .def_prop_ro("initial", [](const automaton_graph& g) -> std::optional<state_id> {
            return g.initial() == no_state ? std::nullopt : std::optional<state_id>(g.initial());
        })
        .def("is_accept", &automaton_graph::is_accept, nb::arg("state"))
        .def("source", &automaton_graph::source, nb::arg("edge"))
        .def("target", &automaton_graph::target, nb::arg("edge"))
        .def("is_epsilon", &automaton_graph::is_epsilon, nb::arg("edge"))
        .def("labels", &automaton_graph::labels, nb::arg("edge"))
        .def("accepts", &automaton_graph::accepts, nb::arg("edge"), nb::arg("symbol"))
        .def("outgoing", [](const automaton_graph& g, state_id s) { return to_vector(g.outgoing(s)); }, nb::arg("state"))
        .def("incoming", [](const automaton_graph& g, state_id s) { return to_vector(g.incoming(s)); }, nb::arg("state"))
        .def_prop_ro("is_acyclic", &automaton_graph::is_acyclic)
        .def_prop_ro("is_deterministic", &automaton_graph::is_deterministic)
        .def("topological_order", [](const automaton_graph& g) { return to_vector(g.topological_order()); })
        .def("useful_states", &automaton_graph::useful_states)
        .def("coaccessible_states", &automaton_graph::coaccessible_states)
        .def("run", [](const automaton_graph& g, const word_view& w) { return g.run(w); }, nb::arg("word"))
        .def("__contains__", [](const automaton_graph& g, const word_view& w) { return g.run(w); })
        .def("to_automaton", &automaton_graph::to_automaton)
        .def("same_as", &automaton_graph::same_as, nb::arg("other"), "Whether both designate the same graph.")
        .def("__repr__", [](const automaton_graph& g) {
            return "<AutomatonGraph : " + std::to_string(g.state_count()) + " states, " + std::to_string(g.edge_count()) + " edges>";
        });

    // ------------------------------------------------------------------ Results

    nb::class_<weighted_path>(m, "WeightedPath", "A path : its edges, the symbols it reads and its cost.")
        .def_prop_ro("graph", &weighted_path::graph)
        .def_prop_ro("start", &weighted_path::start)
        .def_prop_ro("end", &weighted_path::end)
        .def_prop_ro("edges", [](const weighted_path& p) { return to_vector(p.edges()); })
        .def_prop_ro("symbols", &weighted_path::symbols, "The word read, the least symbol of each edge unless a query chose.")
        .def_prop_ro("edge_count", &weighted_path::edge_count)
        .def_prop_ro("cost", &weighted_path::cost)
        .def("__len__", &weighted_path::length)
        .def("__repr__", [](const weighted_path& p) {
            return "WeightedPath(symbols=" + word_text(p.symbols()) + ", cost=" + cost_text(p.cost()) + ")";
        });

    nb::class_<weighted_word>(m, "WeightedWord", "A word and its cost, the least cost of its accepting paths.")
        .def_ro("symbols", &weighted_word::symbols)
        .def_ro("cost", &weighted_word::cost)
        .def("__len__", &weighted_word::length)
        .def("__iter__", [](const weighted_word& w) { return nb::iter(nb::make_tuple(w.symbols, w.cost)); })
        .def("__repr__", [](const weighted_word& w) {
            return "WeightedWord(symbols=" + word_text(w.symbols) + ", cost=" + cost_text(w.cost) + ")";
        });

    nb::class_<shortest_path_result> result(m, "ShortestPaths", "Shortest distances from a state, with the best paths.");
    nb::enum_<shortest_path_result::status>(result, "Status")
        .value("FINITE", shortest_path_result::status::finite)
        .value("UNREACHABLE", shortest_path_result::status::unreachable)
        .value("UNBOUNDED", shortest_path_result::status::unbounded);
    result.def("distance_to", &shortest_path_result::distance_to, nb::arg("state"))
        .def("distances", &shortest_path_result::distances)
        .def("status_to", &shortest_path_result::status_to, nb::arg("state"))
        .def("path_to", &shortest_path_result::path_to, nb::arg("state"))
        .def_prop_ro("optimal_cost", &shortest_path_result::optimal_cost)
        .def("best_path", &shortest_path_result::best_path);

    nb::class_<path_analysis>(m, "PathAnalysis", "Distances in both directions : costs through a state or an edge.")
        .def(nb::init<const weighted_automaton&>(), nb::arg("automaton"), release_gil())
        .def_prop_ro("forward", &path_analysis::forward)
        .def_prop_ro("backward", [](const path_analysis& p) { return to_vector(p.backward()); })
        .def("cost_through_state", &path_analysis::cost_through_state, nb::arg("state"))
        .def("cost_through_edge", &path_analysis::cost_through_edge, nb::arg("edge"));

    bind_lazy<weighted_path>(m, "PathIterator", "Accepting paths by increasing cost, computed one at a time.");
    bind_lazy<weighted_word>(m, "WordCostIterator", "Distinct words by increasing cost, computed one at a time.");

    // ------------------------------------------------------------------ WeightedAutomaton

    nb::class_<weighted_automaton> weighted(m, "WeightedAutomaton", R"(Additive costs on the edges of a graph : the cost of a word is the least cost of its accepting paths.

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
)");
    nb::class_<weighted_automaton::builder>(weighted, "Builder", "Builds weighted automata : the graph and its costs together.")
        .def(nb::init<>())
        .def("add_state", &weighted_automaton::builder::add_state)
        .def("set_initial", &weighted_automaton::builder::set_initial, nb::arg("state"), nb::arg("cost") = 0.0)
        .def("set_accept", &weighted_automaton::builder::set_accept, nb::arg("state"), nb::arg("cost") = 0.0)
        .def("add_transition", [](weighted_automaton::builder& b, state_id from, state_id to, const labels_arg& labels, double cost) {
            return b.add_transition(from, to, labels.value(), cost);
        }, nb::arg("source"), nb::arg("target"), nb::arg("labels"), nb::arg("cost"))
        .def("add_epsilon", &weighted_automaton::builder::add_epsilon, nb::arg("source"), nb::arg("target"), nb::arg("cost"))
        .def("build", &weighted_automaton::builder::build);
    nb::class_<weighted_automaton::cost_evaluator>(weighted, "Evaluator", "Evaluates many words, reusing its buffers.")
        .def("cost", [](weighted_automaton::cost_evaluator& e, const word_view& w) { return e.cost(w); }, nb::arg("word"))
        .def("__call__", [](weighted_automaton::cost_evaluator& e, const word_view& w) { return e.cost(w); }, nb::arg("word"))
        .def("optimal_cost_with_prefix", [](weighted_automaton::cost_evaluator& e, const word_view& w) {
            return e.optimal_cost_with_prefix(w);
        }, nb::arg("prefix"));
    weighted.def(nb::init<>(), "The automaton without any state.")
        .def_static("of", [](const automaton_graph& g, const std::vector<double>& edges, double initial, const std::optional<std::vector<double>>& finals) {
            return weighted_automaton::of(g, edges, initial, finals ? *finals : std::vector<double>{});
        }, nb::arg("graph"), nb::arg("weights"), nb::arg("initial") = 0.0, nb::arg("finals") = nb::none(),
            "A weighting of a graph : one weight per edge, and optionally an initial cost and one final cost per state.")
        .def_static("from_automaton", [](const automaton& a, const cost_arg& cost) { return weighted_automaton::from(a, cost.value); },
                    nb::arg("automaton"), nb::arg("cost"))
        .def_prop_ro("graph", &weighted_automaton::graph)
        .def("weight", &weighted_automaton::weight, nb::arg("edge"))
        .def_prop_ro("weights", [](const weighted_automaton& a) { return to_vector(a.weights()); })
        .def_prop_ro("initial_weight", &weighted_automaton::initial_weight)
        .def("final_weight", &weighted_automaton::final_weight, nb::arg("state"))
        .def("with_weights", [](const weighted_automaton& a, const std::vector<double>& w) { return a.with_weights(w); }, nb::arg("weights"))
        .def("cost", [](const weighted_automaton& a, const word_view& w) { return a.cost(w); }, nb::arg("word"))
        .def("__call__", [](const weighted_automaton& a, const word_view& w) { return a.cost(w); }, nb::arg("word"))
        .def("run", [](const weighted_automaton& a, const word_view& w) { return a.run(w); }, nb::arg("word"),
             "Whether an accepting path reads the word, whatever the costs.")
        .def("__contains__", [](const weighted_automaton& a, const word_view& w) { return a.run(w); })
        .def("to_automaton", &weighted_automaton::to_automaton, "The automaton of the language, without the costs.")
        .def("evaluator", &weighted_automaton::evaluator)
        .def("optimal_cost_with_prefix", [](const weighted_automaton& a, const word_view& w) { return a.optimal_cost_with_prefix(w); },
             nb::arg("prefix"))
        .def_prop_ro("optimal_cost", &weighted_automaton::optimal_cost, "The best cost of the accepting paths.")
        .def("best_path", [](const weighted_automaton& a, std::optional<word_view> w) {
            nb::gil_scoped_release release;
            return w ? a.best_path(w->symbols) : a.best_path();
        }, nb::arg("word") = nb::none(), "A best accepting path, reading a word if given, or None.")
        .def("cost_of_length", &weighted_automaton::cost_of_length, nb::arg("length"), release_gil())
        .def("cost_of_length_at_most", &weighted_automaton::cost_of_length_at_most, nb::arg("length"), release_gil())
        .def("best_path_of_length", &weighted_automaton::best_path_of_length, nb::arg("length"), release_gil())
        .def("best_word_of_length", &weighted_automaton::best_word_of_length, nb::arg("length"), release_gil())
        .def("__or__", [](const weighted_automaton& a, const weighted_automaton& b) { return a | b; }, release_gil())
        .def("__and__", [](const weighted_automaton& a, const weighted_automaton& b) { return a & b; }, release_gil())
        .def("__add__", [](const weighted_automaton& a, const weighted_automaton& b) { return a + b; }, release_gil())
        .def("__pos__", [](const weighted_automaton& a) { return +a; }, release_gil())
        .def("union", &weighted_automaton::union_with, nb::arg("other"), release_gil())
        .def("intersection", &weighted_automaton::intersect_with, nb::arg("other"), release_gil())
        .def("concatenate", &weighted_automaton::concatenate, nb::arg("other"), release_gil())
        .def("reverse", &weighted_automaton::reverse, release_gil())
        .def("star", &weighted_automaton::star, release_gil())
        .def("plus", &weighted_automaton::plus, release_gil())
        .def("repeat", [](const weighted_automaton& a, int min, std::optional<int> max) { return a.repeat(min, max ? *max : -1); },
             nb::arg("min"), nb::arg("max") = nb::none(), release_gil())
        .def("trim", &weighted_automaton::trim, release_gil())
        .def("simplify", &weighted_automaton::simplify, release_gil())
        .def("remove_epsilon", &weighted_automaton::remove_epsilon, release_gil())
        .def("determinize", &weighted_automaton::determinize, nb::arg("max_states") = weighted_automaton::default_max_states,
             "An equivalent deterministic automaton, computed exactly; ValueError beyond max_states states.", release_gil())
        .def("minimize", &weighted_automaton::minimize, "The minimal deterministic automaton with the same costs.", release_gil())
        .def("compile", &weighted_automaton::compile, release_gil())
        .def("k_best_paths", &weighted_automaton::k_best_paths, nb::arg("k"), release_gil())
        .def("k_best_words", &weighted_automaton::k_best_words, nb::arg("k"), release_gil())
        .def("paths_by_cost", [](const weighted_automaton& a) { return make_lazy(a.paths_by_cost()); },
             "The accepting paths by increasing cost, lazily.")
        .def("words_by_cost", [](const weighted_automaton& a) { return make_lazy(a.words_by_cost()); },
             "The distinct words by increasing cost, lazily, without determinizing.")
        .def("__repr__", [](const weighted_automaton& a) {
            return "<WeightedAutomaton : " + std::to_string(a.graph().state_count()) + " states, " +
                   std::to_string(a.graph().edge_count()) + " edges>";
        });

    m.def("shortest_paths", &shortest_paths, nb::arg("automaton"), nb::arg("reconstruct") = true, release_gil());
    m.def("shortest_paths_from", &shortest_paths_from, nb::arg("automaton"), nb::arg("source"), release_gil());
    m.def("dag_shortest_paths", &dag_shortest_paths, nb::arg("automaton"), release_gil());
    m.def("dag_longest_paths", &dag_longest_paths, nb::arg("automaton"), release_gil());
    m.def("dijkstra", &dijkstra, nb::arg("automaton"), release_gil());
    m.def("bellman_ford", &bellman_ford, nb::arg("automaton"), release_gil());
    m.def("backward_distances", &backward_distances, nb::arg("automaton"), release_gil());
    m.def("distances_to", &distances_to, nb::arg("automaton"), nb::arg("destination"), release_gil());
    m.def("cost_through_state", &cost_through_state, nb::arg("automaton"), nb::arg("state"), release_gil());
    m.def("cost_through_edge", &cost_through_edge, nb::arg("automaton"), nb::arg("edge"), release_gil());
    m.def("determinize_dag", &determinize_dag, nb::arg("automaton"), nb::arg("max_states") = static_cast<std::size_t>(-1), release_gil());
    m.def("minimize_dag", &minimize_dag, nb::arg("automaton"), release_gil());
    m.def("k_best_paths_of_length", &k_best_paths_of_length, nb::arg("automaton"), nb::arg("length"), nb::arg("k"), release_gil());
    m.def("k_best_words_of_length", &k_best_words_of_length, nb::arg("automaton"), nb::arg("length"), nb::arg("k"), release_gil());

    // ------------------------------------------------------------------ Multi-weighted automata

    nb::class_<multi_weighted_path>(m, "MultiWeightedPath", "An accepting path and the vector of its costs.")
        .def_prop_ro("graph", &multi_weighted_path::graph)
        .def_prop_ro("start", &multi_weighted_path::start)
        .def_prop_ro("end", &multi_weighted_path::end)
        .def_prop_ro("edges", [](const multi_weighted_path& p) { return to_vector(p.edges()); })
        .def_prop_ro("symbols", &multi_weighted_path::symbols)
        .def_prop_ro("costs", [](const multi_weighted_path& p) { return to_vector(p.costs()); })
        .def("cost", &multi_weighted_path::cost, nb::arg("dimension"))
        .def("exact_cost", &multi_weighted_path::exact_cost, nb::arg("dimension"), "The exact sum, as a fractions.Fraction.")
        .def("__len__", &multi_weighted_path::length)
        .def("__repr__", [](const multi_weighted_path& p) {
            std::string costs;
            for (double c : p.costs())
                costs += (costs.empty() ? "" : ", ") + cost_text(c);
            return "MultiWeightedPath(symbols=" + word_text(p.symbols()) + ", costs=[" + costs + "])";
        });

    nb::class_<pareto_result>(m, "ParetoFront", "The non-dominated cost vectors, each with a path.")
        .def_prop_ro("paths", [](const pareto_result& r) { return to_vector(r.paths()); })
        .def_prop_ro("includes_all_paths", &pareto_result::includes_all_paths)
        .def_prop_ro("is_complete", &pareto_result::is_complete)
        .def("__len__", &pareto_result::size)
        .def("__iter__", [](const pareto_result& r) { return nb::iter(nb::cast(to_vector(r.paths()))); })
        .def("__getitem__", [](const pareto_result& r, std::size_t i) {
            if (i >= r.size())
                throw nb::index_error();
            return r.paths()[i];
        });

    m.def("dominates", [](const std::vector<double>& x, const std::vector<double>& y) { return dominates(x, y); }, nb::arg("x"), nb::arg("y"),
          "Whether x dominates y for minimization : smaller or equal everywhere, smaller somewhere.");

    nb::class_<multi_weighted_automaton> multi(m, "MultiWeightedAutomaton", R"(An automaton weighted by vectors : several additive costs at once.

    >>> b = MultiWeightedAutomaton.Builder(2)     # time and money
    >>> p, q = b.add_state(), b.add_state()
    >>> b.set_initial(p)
    >>> b.set_accept(q)
    >>> _ = b.add_transition(p, q, 7, [10, 1])
    >>> _ = b.add_transition(p, q, 7, [2, 5])
    >>> len(b.build().pareto_front())
    2
)");
    nb::class_<multi_weighted_automaton::builder>(multi, "Builder", "Builds automata weighted by vectors.")
        .def(nb::init<std::size_t>(), nb::arg("dimensions"))
        .def("add_state", &multi_weighted_automaton::builder::add_state)
        .def("set_initial", [](multi_weighted_automaton::builder& b, state_id s, const std::optional<std::vector<double>>& costs) {
            b.set_initial(s, costs ? *costs : std::vector<double>{});
        }, nb::arg("state"), nb::arg("costs") = nb::none())
        .def("set_accept", [](multi_weighted_automaton::builder& b, state_id s, const std::optional<std::vector<double>>& costs) {
            b.set_accept(s, costs ? *costs : std::vector<double>{});
        }, nb::arg("state"), nb::arg("costs") = nb::none())
        .def("add_transition", [](multi_weighted_automaton::builder& b, state_id from, state_id to, const labels_arg& labels, const std::vector<double>& costs) {
            return b.add_transition(from, to, labels.value(), costs);
        }, nb::arg("source"), nb::arg("target"), nb::arg("labels"), nb::arg("costs"))
        .def("add_epsilon", [](multi_weighted_automaton::builder& b, state_id from, state_id to, const std::vector<double>& costs) {
            return b.add_epsilon(from, to, costs);
        }, nb::arg("source"), nb::arg("target"), nb::arg("costs"))
        .def("build", &multi_weighted_automaton::builder::build);
    multi.def_static("of", [](const automaton_graph& g, std::size_t d, const std::vector<double>& edges,
                              const std::optional<std::vector<double>>& initial, const std::optional<std::vector<double>>& finals) {
            return multi_weighted_automaton::of(g, d, edges, initial ? *initial : std::vector<double>{}, finals ? *finals : std::vector<double>{});
        }, nb::arg("graph"), nb::arg("dimensions"), nb::arg("weights"), nb::arg("initial") = nb::none(), nb::arg("finals") = nb::none())
        .def_static("from_weightings", [](const std::vector<weighted_automaton>& w) { return multi_weighted_automaton::from_weightings(w); },
                    nb::arg("weightings"), "Scalar weightings of one graph as the dimensions of a vector : costs stay correlated.")
        .def_static("synchronized_product", [](const std::vector<weighted_automaton>& w) {
            return multi_weighted_automaton::synchronized_product(w);
        }, nb::arg("automata"), "The product on common words, one dimension per operand, every combination of paths kept.")
        .def_prop_ro("graph", &multi_weighted_automaton::graph)
        .def_prop_ro("dimensions", &multi_weighted_automaton::dimensions)
        .def("weight", &multi_weighted_automaton::weight, nb::arg("edge"), nb::arg("dimension"))
        .def("weights", [](const multi_weighted_automaton& a, edge_id e) { return to_vector(a.weights(e)); }, nb::arg("edge"))
        .def_prop_ro("initial_weights", [](const multi_weighted_automaton& a) { return to_vector(a.initial_weights()); })
        .def("final_weights", [](const multi_weighted_automaton& a, state_id s) { return to_vector(a.final_weights(s)); }, nb::arg("state"))
        .def("run", [](const multi_weighted_automaton& a, const word_view& w) { return a.run(w); }, nb::arg("word"))
        .def("__contains__", [](const multi_weighted_automaton& a, const word_view& w) { return a.run(w); })
        .def("to_automaton", &multi_weighted_automaton::to_automaton)
        .def("project", &multi_weighted_automaton::project, nb::arg("dimension"))
        .def("select_dimensions", [](const multi_weighted_automaton& a, const std::vector<std::size_t>& d) { return a.select_dimensions(d); },
             nb::arg("dimensions"))
        .def("scalarize", [](const multi_weighted_automaton& a, const std::vector<double>& c) { return a.scalarize(c); }, nb::arg("coefficients"),
             "The scalar automaton of a linear combination of the dimensions.")
        .def("path_costs", &multi_weighted_automaton::path_costs, nb::arg("path"))
        .def("lexicographic_best_path", [](const multi_weighted_automaton& a, std::optional<std::vector<std::size_t>> priority) {
            nb::gil_scoped_release release;
            return priority ? a.lexicographic_best_path(*priority) : a.lexicographic_best_path();
        }, nb::arg("priority") = nb::none(), "The best accepting path for the lexicographic order of the dimensions.")
        .def("pareto_front", &multi_weighted_automaton::pareto_front, nb::arg("all_paths") = false, release_gil())
        .def("pareto_for_word", [](const multi_weighted_automaton& a, const word_view& w) { return a.pareto_for_word(w); }, nb::arg("word"))
        .def("__or__", [](const multi_weighted_automaton& a, const multi_weighted_automaton& b) { return a | b; }, release_gil())
        .def("__and__", [](const multi_weighted_automaton& a, const multi_weighted_automaton& b) { return a & b; }, release_gil())
        .def("__add__", [](const multi_weighted_automaton& a, const multi_weighted_automaton& b) { return a + b; }, release_gil())
        .def("reverse", &multi_weighted_automaton::reverse, release_gil())
        .def("trim", &multi_weighted_automaton::trim, release_gil())
        .def("simplify", &multi_weighted_automaton::simplify, release_gil())
        .def("product_independent", &multi_weighted_automaton::product_independent, nb::arg("other"), release_gil())
        .def("concatenate_independent", &multi_weighted_automaton::concatenate_independent, nb::arg("other"), release_gil())
        .def("compile", &multi_weighted_automaton::compile, release_gil())
        .def("__repr__", [](const multi_weighted_automaton& a) {
            return "<MultiWeightedAutomaton : " + std::to_string(a.dimensions()) + " dimensions, " +
                   std::to_string(a.graph().state_count()) + " states, " + std::to_string(a.graph().edge_count()) + " edges>";
        });

    // ------------------------------------------------------------------ Snapshots and weightings of automata

    nb::class_<automaton_snapshot>(m, "AutomatonSnapshot", R"(The structure of an automaton captured once, to weight it several times on the same graph.

    >>> snapshot = Automaton("12+").snapshot()
    >>> price = snapshot.weighted(2.0)
    >>> both = snapshot.multi_weighted(2.0, lambda edge: 0.0 if edge.epsilon else 5.0)
    >>> price.graph.same_as(both.graph)
    True
)")
        .def(nb::init<const automaton&>(), nb::arg("automaton"))
        .def_prop_ro("graph", &automaton_snapshot::graph)
        .def("origin", &automaton_snapshot::origin, nb::arg("edge"), "The transition of the automaton an edge comes from.")
        .def("weighted", [](const automaton_snapshot& s, const cost_arg& cost, double initial,
                            std::optional<std::variant<double, std::function<double(state_id)>>> final_cost) {
            std::function<double(state_id)> finals;
            if (final_cost) {
                if (auto* value = std::get_if<double>(&*final_cost))
                    finals = [v = *value](state_id) { return v; };
                else
                    finals = std::get<std::function<double(state_id)>>(*final_cost);
            }
            return s.weighted(cost.value, initial, finals);
        }, nb::arg("cost"), nb::arg("initial") = 0.0, nb::arg("final") = nb::none(),
            "A weighting : cost is a number or a function of an Edge, final a number or a function of an accepting state.")
        .def("multi_weighted", [](const automaton_snapshot& s, nb::args costs) { return s.multi_weighted(to_edge_costs(costs)); },
             nb::sig("def multi_weighted(self, *costs: float | collections.abc.Callable[[Edge], float]) -> MultiWeightedAutomaton"),
             "A weighting by vectors, one cost per dimension : numbers or functions of an Edge.")
        .def("run", [](const automaton_snapshot& s, const word_view& w) { return s.run(w); }, nb::arg("word"))
        .def("__contains__", [](const automaton_snapshot& s, const word_view& w) { return s.run(w); })
        .def("to_automaton", &automaton_snapshot::to_automaton);

    auto automaton_class = nb::type<automaton>();
    nb::setattr(automaton_class, "snapshot", nb::cpp_function([](const automaton& a) { return a.snapshot(); }, nb::is_method(),
                                                              nb::name("snapshot"), "The structure captured once, for several weightings."));
    nb::setattr(automaton_class, "weighted", nb::cpp_function([](const automaton& a, const cost_arg& cost) { return a.weighted(cost.value); },
                                                              nb::is_method(), nb::name("weighted"), nb::arg("cost"),
                                                              "This automaton with a cost per transition : a number or a function of an Edge."));
    nb::setattr(automaton_class, "multi_weighted",
                nb::cpp_function([](const automaton& a, nb::args costs) { return a.multi_weighted(to_edge_costs(costs)); }, nb::is_method(),
                                 nb::name("multi_weighted"),
                                 nb::sig("def multi_weighted(self, *costs: float | collections.abc.Callable[[Edge], float]) -> MultiWeightedAutomaton"),
                                 "This automaton weighted by vectors, one cost per dimension."));

    // ------------------------------------------------------------------ Unfoldings

    nb::class_<layered_automaton> layered(m, "LayeredAutomaton", R"(The unfolding of a graph by the number of symbols read, up to a horizon.

Built once, evaluated with any weighting of the base graph, or costs depending on the position.
)");
    nb::class_<layered_automaton::analysis>(layered, "Analysis", "The distances of one weighting of an unfolding.")
        .def("forward_distances", &layered_automaton::analysis::forward_distances)
        .def("backward_distances", [](const layered_automaton::analysis& a) { return to_vector(a.backward_distances()); })
        .def_prop_ro("optimal_cost", &layered_automaton::analysis::optimal_cost)
        .def("best_path", &layered_automaton::analysis::best_path)
        .def("cost_at", &layered_automaton::analysis::cost_at, nb::arg("position"), nb::arg("symbol"),
             "The best cost of the accepting paths reading a symbol at a position.");
    layered.def_static("exactly", &layered_automaton::exactly, nb::arg("graph"), nb::arg("horizon"), release_gil())
        .def_static("at_most", &layered_automaton::at_most, nb::arg("graph"), nb::arg("horizon"), release_gil())
        .def_static("for_word", [](const automaton_graph& g, const word_view& w) { return layered_automaton::for_word(g, w); },
                    nb::arg("graph"), nb::arg("word"))
        .def_prop_ro("base_graph", &layered_automaton::base_graph)
        .def_prop_ro("graph", &layered_automaton::graph)
        .def_prop_ro("horizon", &layered_automaton::horizon)
        .def("original_state", &layered_automaton::original_state, nb::arg("state"))
        .def("position", &layered_automaton::position, nb::arg("state"))
        .def("original_edge", &layered_automaton::original_edge, nb::arg("edge"))
        .def("evaluate", [](const layered_automaton& l, const weighted_automaton& a, const std::optional<position_cost_arg>& weights) {
            return weights ? l.evaluate(a, weights->value) : l.evaluate(a);
        }, nb::arg("automaton"), nb::arg("weights") = nb::none(),
            "The weighting of the unfolding; weights(position, edge) gives costs depending on the position.")
        .def("evaluate", nb::overload_cast<const multi_weighted_automaton&>(&layered_automaton::evaluate, nb::const_), nb::arg("automaton"))
        .def("best_path", &layered_automaton::best_path, nb::arg("automaton"))
        .def("map", nb::overload_cast<const weighted_path&>(&layered_automaton::map, nb::const_), nb::arg("path"))
        .def("map", nb::overload_cast<const multi_weighted_path&>(&layered_automaton::map, nb::const_), nb::arg("path"))
        .def("analyze", [](const layered_automaton& l, const weighted_automaton& a, const std::optional<position_cost_arg>& weights) {
            return weights ? l.analyze(a, weights->value) : l.analyze(a);
        }, nb::arg("automaton"), nb::arg("weights") = nb::none());

    // ------------------------------------------------------------------ Compiled evaluators

    nb::class_<weighted_dfa_index>(m, "WeightedDFAIndex", "The transitions of a deterministic graph, shared by its weightings.")
        .def(nb::init<const automaton_graph&>(), nb::arg("graph"))
        .def_prop_ro("graph", &weighted_dfa_index::graph)
        .def("transition", [](const weighted_dfa_index& i, std::optional<state_id> s, symbol c) -> std::optional<edge_id> {
            auto e = i.transition(s ? *s : no_state, c);
            return e < 0 ? std::nullopt : std::optional<edge_id>(e);
        }, nb::arg("state"), nb::arg("symbol"));

    nb::class_<compiled_weighted_dfa>(m, "CompiledWeightedDFA", "The fast evaluator of a deterministic weighted automaton.")
        .def(nb::init<const weighted_automaton&>(), nb::arg("automaton"))
        .def_prop_ro("index", &compiled_weighted_dfa::index)
        .def("reweight", &compiled_weighted_dfa::reweight, nb::arg("automaton"))
        .def("cost", [](const compiled_weighted_dfa& d, const word_view& w) {
            nb::gil_scoped_release release;
            return d.cost(w);
        }, nb::arg("word"))
        .def("__call__", [](const compiled_weighted_dfa& d, const word_view& w) {
            nb::gil_scoped_release release;
            return d.cost(w);
        }, nb::arg("word"));

    nb::class_<compiled_multi_weighted_dfa>(m, "CompiledMultiWeightedDFA", "The fast evaluator of a deterministic automaton weighted by vectors.")
        .def(nb::init<const multi_weighted_automaton&>(), nb::arg("automaton"))
        .def_prop_ro("dimensions", &compiled_multi_weighted_dfa::dimensions)
        .def_prop_ro("graph", &compiled_multi_weighted_dfa::graph)
        .def("reweight", &compiled_multi_weighted_dfa::reweight, nb::arg("automaton"))
        .def("project", &compiled_multi_weighted_dfa::project, nb::arg("dimension"))
        .def("scalarize", [](const compiled_multi_weighted_dfa& d, const std::vector<double>& c) { return d.scalarize(c); }, nb::arg("coefficients"))
        .def("costs", [](const compiled_multi_weighted_dfa& d, const word_view& w) { return d.costs(w); }, nb::arg("word"),
             "The cost vector of a word, or None if it is not accepted.")
        .def("costs_into", [](const compiled_multi_weighted_dfa& d, const word_view& w, nb::ndarray<double, nb::ndim<1>, nb::c_contig> out) {
            nb::gil_scoped_release release;
            return d.costs(w, std::span<double>(out.data(), out.shape(0)));
        }, nb::arg("word"), nb::arg("out"), "Writes the costs of a word in a numpy array, without allocating; returns whether it is accepted.");
}

}  // namespace autolib::python
