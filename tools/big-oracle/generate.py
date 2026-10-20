#!/usr/bin/env python3
# Reference results of big integer arithmetic, computed with the exact integers of Python.
# Columns : a, b, a+b, a-b, a*b, a/b, a%b (truncated toward zero), k, a<<k, a>>k (floor), float(a), bit_length(a)
import random
import sys

rng = random.Random(2026)
B = 1 << 64


def special():
    edge = [0, 1, B - 1, B, B + 1, B * B - 1, B * B, (B - 1) * B, 10 ** 19, 10 ** 38 - 1, 2 ** 53 + 1,
            2 ** 1023 * (2 ** 53 - 1) // 2 ** 52, 2 ** 1024, (2 ** 53 + 1) * 2 ** 1000]
    return rng.choice(edge) + rng.choice([-1, 0, 0, 1])


def value():
    kind = rng.random()
    if kind < 0.2:
        x = special()
    elif kind < 0.5:
        x = rng.getrandbits(rng.randint(0, 64))
    elif kind < 0.8:
        x = rng.getrandbits(rng.randint(65, 384))
    else:
        # Long runs of ones or zeros, which stress the carries and the corrections of the division
        n = rng.randint(2, 7)
        x = 0
        for _ in range(n):
            x = x * B + rng.choice([0, B - 1, B - 2, 1, 1 << 63, rng.getrandbits(64)])
    return -x if rng.random() < 0.4 else x


def trunc_div(a, b):
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def to_float(a):
    try:
        return repr(float(a))
    except OverflowError:
        return "inf" if a > 0 else "-inf"


out = sys.stdout
for _ in range(1500):
    a, b = value(), value()
    if rng.random() < 0.3 and a != 0:
        b = rng.choice([a, -a, a + 1, a // 3 + 1])
    k = rng.choice([0, 1, 63, 64, 65, 127, 128, rng.randint(0, 700)])
    q = trunc_div(a, b) if b != 0 else "x"
    r = a - q * b if b != 0 else "x"
    out.write("\t".join(map(str, [a, b, a + b, a - b, a * b, q, r, k, a << k, a >> k, to_float(a), abs(a).bit_length()])) + "\n")
