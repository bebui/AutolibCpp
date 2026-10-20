import autolib
from autolib import Automaton

a = Automaton("(0|1)*0(0|1)")            # the second symbol from the end is 0
autolib.render(a, "automaton.svg")
