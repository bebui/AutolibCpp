#!/usr/bin/env python3
# Reference results of exact binary fractions, computed with the exact fractions of Python.
# Columns : a, b (hexadecimal doubles), then for x = a + b + c - d and y = a * b : the nearest doubles of x
# and y, the sign of a - b, and the exact decimal expansions of x and y.
import random
import sys
from decimal import Decimal, getcontext
from fractions import Fraction

getcontext().prec = 4000
rng = random.Random(2027)
specials = [0.0, -0.0, 5e-324, -5e-324, 2.2250738585072014e-308, 2.225073858507201e-308, 1.7976931348623157e308,
            1e16, -1e16, 1.0, 0.1, 0.2, 0.3, 0.5, 2 ** -1074 * 3, 2 ** 53 + 2.0, 1e-300, 1e300]


def value():
    kind = rng.random()
    if kind < 0.3:
        return rng.choice(specials) * rng.choice([1, -1])
    if kind < 0.5:
        return float(rng.randint(-1000, 1000)) / (2 ** rng.randint(0, 10))
    if kind < 0.7:
        return rng.uniform(-1, 1) * 2.0 ** rng.randint(-1074, 1023)
    return rng.uniform(-1e6, 1e6)


def nearest(f):
    try:
        return float(f).hex()
    except OverflowError:
        return "inf" if f > 0 else "-inf"


def decimal(f):
    d = Decimal(f.numerator) / Decimal(f.denominator)
    text = format(d, "f")
    if "." in text:
        text = text.rstrip("0").rstrip(".")
    return "0" if text in ("-0", "") else text


for _ in range(600):
    a, b, c, d = value(), value(), value(), value()
    if rng.random() < 0.3:
        c, d = -a, b  # cancellations
    x = Fraction(a) + Fraction(b) + Fraction(c) - Fraction(d)
    y = Fraction(a) * Fraction(b)
    sign = (Fraction(a) > Fraction(b)) - (Fraction(a) < Fraction(b))
    print("\t".join([a.hex(), b.hex(), c.hex(), d.hex(), nearest(x), nearest(y), str(sign), decimal(x), decimal(y)]))
