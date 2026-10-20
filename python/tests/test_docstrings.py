"""The examples of the docstrings run and give what they show."""

import doctest
import inspect

import pytest

import autolib

objects = [autolib] + [getattr(autolib, name) for name in autolib.__all__ if inspect.isclass(getattr(autolib, name))]


@pytest.mark.parametrize("obj", objects, ids=lambda o: getattr(o, "__name__", str(o)))
def test_docstring_examples(obj):
    text = obj.__doc__ or ""
    parser = doctest.DocTestParser()
    test = parser.get_doctest(text, dict(vars(autolib)), getattr(obj, "__name__", "autolib"), None, 0)
    runner = doctest.DocTestRunner(optionflags=doctest.ELLIPSIS | doctest.NORMALIZE_WHITESPACE)
    runner.run(test)
    assert runner.failures == 0
