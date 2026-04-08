# Configuration file for the Sphinx documentation builder.

import logging
from pathlib import Path

from pygments.lexers.special import TextLexer
from sphinx.highlighting import lexers
from sphinx.util import logging as sphinx_logging


class DuplicateCDeclarationFilter(logging.Filter):
    def filter(self, record):
        return 'Duplicate C declaration' not in record.getMessage()


_original_warning_suppressor_filter = sphinx_logging.WarningSuppressor.filter


def _warning_suppressor_filter(self, record):
    if 'Duplicate C declaration' in record.getMessage():
        return False

    return _original_warning_suppressor_filter(self, record)


sphinx_logging.WarningSuppressor.filter = _warning_suppressor_filter

# -- Project information -----------------------------------------------------
project = 'ThunderOS'
copyright = '2025, ThunderOS Team'
author = 'ThunderOS Team'
release = (Path(__file__).resolve().parents[2] / 'VERSION').read_text(encoding='utf-8').strip()

# License information
project_license = 'GPL v3'

# -- General configuration ---------------------------------------------------
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.viewcode',
    'sphinx.ext.todo',
]

# Use plain-text highlighting for tutorial lexers that Pygments does not ship.
lexers['gdb'] = TextLexer()
lexers['ld'] = TextLexer()

# Several docs include pedagogical snippets that are intentionally not strict
# assembler/C syntax; suppress lexer parse warnings for those examples.
suppress_warnings = [
    'misc.highlighting_failure',
]

templates_path = ['_templates']
exclude_patterns = []

# -- Options for HTML output -------------------------------------------------
html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

# Theme options for better readability
html_theme_options = {
    'navigation_depth': 4,
    'collapse_navigation': False,
    'sticky_navigation': True,
    'includehidden': True,
    'titles_only': False,
    'style_nav_header_background': '#2980B9',  # Blue header
}

# Custom CSS for better code block styling
html_css_files = [
    'custom.css',
]

# Logo and favicon (optional - add these files to _static/ if you have them)
# html_logo = '_static/logo.png'
# html_favicon = '_static/favicon.ico'

# Show "Edit on GitHub" link
html_context = {
    'display_github': True,
    'github_user': 'cmelnu',
    'github_repo': 'thunderos',
    'github_version': 'main',
    'conf_py_path': '/docs/source/',
}

# Syntax highlighting
pygments_style = 'monokai'  # Dark theme for code blocks

# -- Extension configuration -------------------------------------------------
todo_include_todos = True


def _install_warning_filters(app):
    warning_filter = DuplicateCDeclarationFilter()

    for logger_name in ('', 'sphinx'):
        logger = logging.getLogger(logger_name)
        for handler in logger.handlers:
            handler.addFilter(warning_filter)


def setup(app):
    # The reference and internals guides intentionally document some C symbols
    # in parallel. Filter only the duplicate-declaration warning text so clean
    # builds still report real documentation issues.
    app.connect('builder-inited', _install_warning_filters)
