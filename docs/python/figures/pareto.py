import autolib
from autolib import MultiWeightedAutomaton

b = MultiWeightedAutomaton.Builder(2)                    # minutes, euros
home, station, office = b.add_state(), b.add_state(), b.add_state()
b.set_initial(home)
b.set_accept(office)
b.add_transition(home, station, 0, [15, 0])              # walk
b.add_transition(home, station, 0, [5, 8])               # taxi
b.add_transition(station, office, 0, [40, 4])            # slow train
b.add_transition(station, office, 0, [20, 15])           # fast train
journey = b.build()

fastest = min(journey.pareto_front(), key=lambda path: path.cost(0))
autolib.render(journey, "pareto.svg", highlight=fastest)
