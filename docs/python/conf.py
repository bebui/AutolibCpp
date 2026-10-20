# Sphinx configuration of the Python documentation of automaton-lib.

import autolib

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
