"""Journeys with two costs, time and money : the Pareto front and the price of time."""

from autolib import MultiWeightedAutomaton

TIME, MONEY = 0, 1
legs = {  # (from, to) : options (name, minutes, euros)
    ("home", "station"): [("walk", 15, 0), ("bus", 8, 2), ("taxi", 5, 9)],
    ("station", "airport"): [("train", 40, 6), ("express", 20, 18)],
    ("home", "airport"): [("taxi", 45, 55), ("shuttle", 70, 12)],
}

b = MultiWeightedAutomaton.Builder(2)
place = {p: b.add_state() for p in ["home", "station", "airport"]}
b.set_initial(place["home"])
b.set_accept(place["airport"])
names = {}
for (a, z), options in legs.items():
    for name, minutes, euros in options:
        names[b.add_transition(place[a], place[z], 0, [minutes, euros])] = name
journeys = b.build()

print("Pareto front :")
for path in sorted(journeys.pareto_front(), key=lambda p: p.costs):
    print(f"  {int(path.cost(TIME)):3} min {int(path.cost(MONEY)):3} EUR  {' + '.join(names[e] for e in path.edges)}")

for euros_per_hour in [5, 20, 60]:
    best = journeys.scalarize([euros_per_hour / 60, 1]).best_path()
    print(f"time worth {euros_per_hour:2} EUR/h : {' + '.join(names[e] for e in best.edges)}")
