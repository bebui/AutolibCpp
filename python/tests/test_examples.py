"""The example scripts of the documentation print what the documentation shows."""

import pathlib
import subprocess
import sys

import pytest

examples = pathlib.Path(__file__).resolve().parents[2] / "docs" / "python" / "examples"
scripts = sorted(examples.glob("*.py")) if examples.is_dir() else []


@pytest.mark.skipif(not scripts, reason="the documentation is not in this tree")
@pytest.mark.parametrize("script", scripts, ids=lambda p: p.stem)
def test_example_output(script):
    if "import numpy" in script.read_text():
        pytest.importorskip("numpy")
    result = subprocess.run([sys.executable, str(script)], capture_output=True, text=True, check=True)
    assert result.stdout == script.with_suffix(".out").read_text()
