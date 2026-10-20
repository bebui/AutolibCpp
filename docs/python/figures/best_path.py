import autolib
from autolib import WeightedAutomaton

b = WeightedAutomaton.Builder()
home, work = b.add_state(), b.add_state()
b.set_initial(home)
b.set_accept(work, 0.5)                   # final cost
b.add_transition(home, work, 1, 30.0)     # bus
b.add_transition(home, work, 1, 20.0)     # bike
b.add_transition(home, work, 2, 45.0)     # on foot
b.add_transition(work, work, 0, 5.0)      # coffee
commute = b.build()

best = commute.best_word_of_length(2)                   # [1, 0] : bike, then coffee
autolib.render(commute, "best_path.svg", highlight=commute.best_path(best))
