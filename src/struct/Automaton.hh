/*
 * Automaton.hh
 *
 *  Created on: Dec 25, 2011
 *      Author: julien
 */

#ifndef AUTOMATON_HH_
#define AUTOMATON_HH_

#include <boost/graph/adjacency_list.hpp>
#include <string>

namespace structure {

using namespace boost;
using std::string;

struct symbol_t {
  typedef edge_property_tag kind;
};
struct name_t {
  typedef vertex_property_tag kind;
};

template < typename S >
class Automaton {
public:
	typedef property<symbol_t,S> EdgeProperty;
	typedef property<name_t,string> VertexProperty;

	typedef adjacency_list<vecS, vecS, directedS, VertexProperty, EdgeProperty> Graph;
	typedef Graph::vertex_descriptor State;

	Automaton();
	virtual ~Automaton();

	void addState(string name_p = "")
	{
		State newState = add_vertex(graph_m);
		graph_m[newState] = name_p;
	}



private:
	Graph graph_m;

};

} /* namespace structure */
#endif /* AUTOMATON_HH_ */
