"""Event logs : find suspicious sequences in a long numpy array of events."""

import numpy as np

from autolib import Automaton

LOGIN_OK, LOGIN_FAILED, READ, WRITE, LOGOUT = range(5)

# Three failed logins or more, then a success : a brute force that worked
suspicious = Automaton(f"<{LOGIN_FAILED}>{{3,}}<{LOGIN_OK}>").searcher()

rng = np.random.default_rng(42)
log = rng.choice(5, size=1_000_000, p=[0.2, 0.25, 0.3, 0.2, 0.05]).astype(np.int32)

matches = suspicious.find_all(log)              # one pass over the million events
print("suspicious sequences :", len(matches))
first = matches[0]
print("first one at", first.start, ":", log[first.start:first.end].tolist())
print("longest :", max(len(m) for m in matches), "events")
