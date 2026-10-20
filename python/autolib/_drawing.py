# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (c) 2025-2026 Julien Menana
"""Drawings of automata with Graphviz : SVG for notebooks, and image files."""

import os
import shutil
import subprocess

INSTALL = (
    "drawing needs the Graphviz program 'dot' : https://graphviz.org/download/ "
    "(apt install graphviz, brew install graphviz, conda install graphviz, winget install graphviz)"
)


def _dot(automaton, highlight):
    if highlight is None:
        return automaton.to_dot()
    if hasattr(automaton, "graph") or type(automaton).__name__ == "AutomatonGraph":
        return automaton.to_dot(highlight)
    raise TypeError("only weighted automata and graphs highlight paths")


def _run(dot_text, output_format):
    program = shutil.which("dot")
    if program is None:
        raise RuntimeError(INSTALL)
    result = subprocess.run([program, f"-T{output_format}"], input=dot_text.encode(), capture_output=True, check=True)
    return result.stdout


def to_svg(automaton, highlight=None):
    """The drawing of an automaton, a weighted automaton or a graph, as SVG text.

    highlight is a path (WeightedPath, MultiWeightedPath) or a list of edges, drawn in colour.
    Needs the Graphviz program ``dot``.
    """
    return _run(_dot(automaton, highlight), "svg").decode()


def render(automaton, path, highlight=None):
    """Writes the drawing of an automaton to a file whose extension gives the format : .svg, .png, .pdf...

    Needs the Graphviz program ``dot``.
    """
    output_format = os.path.splitext(path)[1].lstrip(".").lower() or "svg"
    data = _run(_dot(automaton, highlight), output_format)
    with open(path, "wb") as file:
        file.write(data)


def _repr_svg_(self):
    # Jupyter shows this drawing; without Graphviz, it falls back to the text representation
    try:
        return to_svg(self)
    except (RuntimeError, subprocess.CalledProcessError):
        return None


def install(*classes):
    for cls in classes:
        cls._repr_svg_ = _repr_svg_
