# Sphinx configuration of the Python documentation of automaton-lib.

import pathlib
import runpy
import os

import autolib

# The drawings of the visualization page, made by the scripts it shows
figures = pathlib.Path(__file__).parent / "figures"
here = os.getcwd()
os.chdir(figures)
try:
    for script in sorted(figures.glob("*.py")):
        runpy.run_path(str(script), run_name="__main__")
finally:
    os.chdir(here)

project = "automaton-lib"
author = "Julien Menana"
copyright = "2025-2026, Julien Menana"
release = autolib.__version__

extensions = ["myst_parser", "sphinx.ext.autodoc", "sphinx.ext.intersphinx", "sphinx_copybutton"]
# The copy button copies the code without the prompts and the outputs
copybutton_prompt_text = r">>> |\.\.\. "
copybutton_prompt_is_regexp = True
copybutton_only_copy_prompt_lines = True
myst_enable_extensions = ["colon_fence", "deflist"]
source_suffix = {".md": "markdown", ".rst": "restructuredtext"}
intersphinx_mapping = {"python": ("https://docs.python.org/3", None)}
autodoc_member_order = "bysource"
autodoc_default_options = {"members": True, "special-members": "__call__, __contains__", "undoc-members": True}

html_theme = "furo"
html_title = f"automaton-lib {release}"
html_theme_options = {
    "source_repository": "https://github.com/bebui/AutolibCpp/",
    "source_branch": "master",
    "source_directory": "docs/python/",
}
