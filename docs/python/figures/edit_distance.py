import autolib
from autolib import IntervalSet, WeightedAutomaton

letters = IntervalSet(range(ord("a"), ord("z") + 1))
target, b = "ab", WeightedAutomaton.Builder()
state = {(i, e): b.add_state() for i in range(len(target) + 1) for e in range(2)}
b.set_initial(state[0, 0])
for (i, e), s in state.items():
    if i == len(target):
        b.set_accept(s)
    if i < len(target):
        b.add_transition(s, state[i + 1, e], ord(target[i]), 0.0)        # match
    if e == 0:
        b.add_transition(s, state[i, 1], letters, 1.0)                   # insertion
        if i < len(target):
            b.add_transition(s, state[i + 1, 1], letters, 1.0)           # substitution
            b.add_epsilon(s, state[i + 1, 1], 1.0)                       # deletion, dashed
typos = b.build()

closest = typos.best_path([ord(c) for c in "xab"])     # one insertion
autolib.render(typos, "edit_distance.svg", highlight=closest)
