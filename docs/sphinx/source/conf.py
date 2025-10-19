# -- Project information -----------------------------------------------------
project = "scopeX"
author = "Yan Liu"
copyright = "2025, Yan Liu"
release = "0.1.0"

# -- General configuration ---------------------------------------------------
# Common Extensions: Breathe(->Doxygen XML), MyST(supports Markdown), todo, cross-referencing, etc.
extensions = [
    "breathe",
    "myst_parser",
    "sphinx.ext.todo",
    "sphinx.ext.intersphinx",
    "sphinx.ext.ifconfig",
    "sphinx.ext.autosectionlabel",
]

# Allow Markdown (.md) as document source files
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

templates_path = ["_templates"]
exclude_patterns = []

# C++ Friendly Settings
primary_domain = "cpp"
highlight_language = "c++"

# -------- Breathe / Doxygen Integration (You have placed XML in docs/xml/) --------
from pathlib import Path

_docs_dir = Path(__file__).parent.resolve()
_breathe_xml = (_docs_dir / "xml").resolve()  # docs/xml
breathe_projects = {"scopeX": str(_breathe_xml)}
breathe_default_project = "scopeX"

# If you have multiple XML sets (multi-module) later, just add key-value pairs to the dict above
# breathe_projects = {"core": ".../core/xml", "algo": ".../algo/xml"}

# Default member visibility (can be refined as needed)
breathe_default_members = ("members", "undoc-members", "protected-members")

# Let Breathe determine language by extension (.h/.hpp -> C++)
breathe_domain_by_extension = {
    "h": "cpp",
    "hpp": "cpp",
    "hh": "cpp",
    "ipp": "cpp",
    "cc": "cpp",
    "cpp": "cpp",
}

# -- MyST（Markdown）Options ----------------------------------------------------
myst_enable_extensions = [
    "colon_fence",   # supports ::: fenced blocks
    "deflist",       # supports definition lists
    "linkify",       # supports automatic URL recognition
    "substitution",  # supports variable substitution
]

# -- Intersphinx（cross-project document linking, optional）-----------------------------------
intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "sphinx": ("https://www.sphinx-doc.org/en/master/", None),
}

# -- HTML Output ---------------------------------------------------------------
html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]  # You can place custom CSS in docs/_static
# html_logo = "_static/logo.png"       # Optional: place your logo
# html_favicon = "_static/favicon.ico" # Optional: place your website icon

html_theme_options = {
    "collapse_navigation": False,
    "navigation_depth": 4,
    "sticky_navigation": True,
    "style_external_links": True,
    "titles_only": False,
}

# -- Other Enhancements (Optional) --------------------------------------------------------
# Let autosectionlabel carry the document path prefix to avoid name clashes across different pages
autosectionlabel_prefix_document = True

# todo extension: whether to show TODOs during generation
todo_include_todos = True

# RST global variables available for substitution
rst_epilog = """
.. |Project| replace:: scopeX
.. |Year| replace:: 2025
"""

# If you plan to import Python modules in Sphinx later, you can enhance sys.path here
# import sys
# sys.path.insert(0, os.path.abspath(".."))
# -- Project information -----------------------------------------------------
project = "scopeX"
author = "Yan Liu"
copyright = "2025, Yan Liu"
release = "0.1.0"

# -- General configuration ---------------------------------------------------
# Common Extensions: Breathe(->Doxygen XML), MyST(->Markdown), todo, Intersphinx, etc.
extensions = [
    "breathe",
    "myst_parser",
    "sphinx.ext.todo",
    "sphinx.ext.intersphinx",
    "sphinx.ext.ifconfig",
    "sphinx.ext.autosectionlabel",
]

# Allow Markdown (.md) as a source file
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

templates_path = ["_templates"]
exclude_patterns = []

# C++ friendly settings
primary_domain = "cpp"
highlight_language = "c++"

# -------- Breathe / Doxygen Integration (You have placed XML in docs/xml/) --------
import os
from pathlib import Path

_docs_dir = Path(__file__).parent.resolve()
_breathe_xml = (_docs_dir / "xml").resolve()  # docs/xml
breathe_projects = {"scopeX": str(_breathe_xml)}
breathe_default_project = "scopeX"

# If you have multiple sets of XML (multiple modules), you can add key-value pairs to the above dict
# breathe_projects = {"core": ".../core/xml", "algo": ".../algo/xml"}

# Default member visibility (can be refined as needed)
breathe_default_members = ("members", "undoc-members", "protected-members")

# Let Breathe determine language by extension (.h/.hpp -> C++)
breathe_domain_by_extension = {
    "h": "cpp",
    "hpp": "cpp",
    "hh": "cpp",
    "ipp": "cpp",
    "cc": "cpp",
    "cpp": "cpp",
}

# -- MyST（Markdown）Options ----------------------------------------------------
myst_enable_extensions = [
    "colon_fence",   # supports ::: fenced blocks
    "deflist",       # supports definition lists
    "linkify",       # supports automatic URL recognition
    "substitution",  # supports variable substitution
]

# -- Intersphinx（Cross-project document linking, optional）-----------------------------------
intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "sphinx": ("https://www.sphinx-doc.org/en/master/", None),
}

# -- HTML Output ---------------------------------------------------------------
html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]  # You can place custom CSS in docs/_static
# html_logo = "_static/logo.png"       # Optional: place your logo
# html_favicon = "_static/favicon.ico" # Optional: place your website icon

html_theme_options = {
    "collapse_navigation": False,
    "navigation_depth": 4,
    "sticky_navigation": True,
    "style_external_links": True,
    "titles_only": False,
}

# -- Other Enhancements (Optional) --------------------------------------------------------
# Let autosectionlabel carry the document path prefix to avoid name clashes across different pages
autosectionlabel_prefix_document = True

# todo extension: whether to show TODOs during generation
todo_include_todos = True

# RST global variables available for substitution
rst_epilog = """
.. |Project| replace:: scopeX
.. |Year| replace:: 2025
"""

# If you plan to import Python modules in Sphinx later, you can enhance sys.path here
# import sys
# sys.path.insert(0, os.path.abspath(".."))
