"""Scheduling with prices that change by the hour : costs depending on the position."""

from autolib import Automaton, LayeredAutomaton

OFF, ON = 0, 1
# Cents per kWh, hour by hour : cheap at night and at midday, when the sun shines
price = [24, 18, 17, 19, 26, 31, 38, 45, 42, 33, 25, 19, 15, 14, 16, 22, 34, 47, 52, 49, 40, 33, 28, 26]

# A machine runs 6 hours a day, by blocks of at least 2 hours
blocks = Automaton("0*(11+0+)*(11+)?")
six_hours = Automaton("0*(10*){6}")
rules = blocks & six_hours
plan = rules.snapshot().weighted(lambda edge: 1.0 if ON in edge.labels else 0.0)

layers = LayeredAutomaton.exactly(plan.graph, 24)
analysis = layers.analyze(plan, lambda hour, edge: price[hour] * plan.weight(edge))
best = analysis.best_path()
print("cost :", analysis.optimal_cost)
print("hours on :", [hour for hour, s in enumerate(best.symbols) if s == ON])
print("schedule :", "".join("#" if s == ON else "." for s in best.symbols))
