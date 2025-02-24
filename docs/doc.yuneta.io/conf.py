# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

import os
from urllib.request import urlopen
from pathlib import Path
from docutils.parsers.rst import roles
from docutils import nodes

project = 'Yuneta Simplified'
copyright = '2024, ArtGins'
author = 'ArtGins'
release = '7.0.0'
show_authors = False
master_doc = "index"

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    "sphinx.ext.todo",
    "sphinx.ext.viewcode",
    "sphinx.ext.autodoc",
    "sphinx.ext.autosummary",
    "sphinx.ext.graphviz",
    "sphinx.ext.ifconfig",
    "sphinx.ext.inheritance_diagram",
    "sphinx.ext.intersphinx",
    "sphinx.ext.napoleon",
    "ablog",
    "myst_nb",
    "numpydoc",
    "sphinx_design",
    "sphinx_copybutton",
    "sphinx_togglebutton",
    "sphinxext.opengraph",
    "sphinxcontrib.youtube",
    "sphinx_thebe",
    "sphinx_tabs.tabs",
    "sphinxcontrib.bibtex",
    "sphinxcontrib.mermaid",
]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

intersphinx_mapping = {
    "python": ("https://docs.python.org/3.8", None),
    "sphinx": ("https://www.sphinx-doc.org/en/master", None),
    "pst": ("https://pydata-sphinx-theme.readthedocs.io/en/latest/", None),
}
nitpick_ignore = [
    ("py:class", "docutils.nodes.document"),
    ("py:class", "docutils.parsers.rst.directives.body.Sidebar"),
]

suppress_warnings = ["myst.domains", "ref.ref"]

numfig = True
# html_use_index = True

myst_enable_extensions = [
    "dollarmath",
    "amsmath",
    "deflist",
    # "html_admonition",
    # "html_image",
    "colon_fence",
    # "smartquotes",
    # "replacements",
    # "linkify",
    # "substitution",
]


# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "sphinx_book_theme"
html_logo = "_static/yuneta-label.svg"
html_title = "Yuneta Simplified"
html_copy_source = True
html_favicon = "_static/yuneta-y.svg"
html_last_updated_fmt = ""

html_sidebars = {
    "reference/blog/*": [
        "navbar-logo.html",
        "search-field.html",
        "ablog/postcard.html",
        "ablog/recentposts.html",
        "ablog/tagcloud.html",
        "ablog/categories.html",
        "ablog/archives.html",
        "sbt-sidebar-nav.html",
    ]
}
# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ["_static"]
html_css_files = ["custom.css"]
nb_execution_mode = "cache"
thebe_config = {
    "repository_url": "https://github.com/binder-examples/jupyter-stacks-datascience",
    "repository_branch": "master",
}

html_theme_options = {
    "path_to_docs": "docs",
    "repository_url": "https://github.com/artgins/yunetas",
    "repository_branch": "master",
    "launch_buttons": {
        "binderhub_url": "https://mybinder.org",
        "colab_url": "https://colab.research.google.com/",
        "deepnote_url": "https://deepnote.com/",
        "notebook_interface": "jupyterlab",
        "thebe": True,
        # "jupyterhub_url": "https://datahub.berkeley.edu",  # For testing
    },
    "use_edit_page_button": True,
    "use_source_button": True,
    "use_issues_button": True,
    # "use_repository_button": True,
    "use_download_button": True,
    "use_sidenotes": True,
    "show_toc_level": 2,
    "announcement": (
    ),
    "logo": {
        "image_dark": "_static/yuneta-label.svg",
        "text": html_title,  # Uncomment to try text with logo
    },
    "icon_links": [
        {
            "name": "Yuneta Simplified",
            "url": "https://github.com/artgins/yunetas",
            "icon": "_static/yuneta-logo.png",
            "type": "local",
        },
        {
            "name": "GitHub",
            "url": "https://github.com/artgins/yunetas",
            "icon": "fa-brands fa-github",
        },
        {
            "name": "PyPI",
            "url": "https://pypi.org/project/yunetas/",
            "icon": "https://img.shields.io/pypi/dw/yunetas",
            "type": "url",
        },
    ],
    # For testing
    # "use_fullscreen_button": False,
    # "home_page_in_toc": True,
    # "extra_footer": "<a href='https://google.com'>Test</a>",  # DEPRECATED KEY
    # "show_navbar_depth": 2,
    # Testing layout areas
    # "navbar_start": ["test.html"],
    # "navbar_center": ["test.html"],
    # "navbar_end": ["test.html"],
    # "navbar_persistent": ["test.html"],
    # "footer_start": ["test.html"],
    # "footer_end": ["test.html"]
    "extra_footer": "Made with <a href='https://www.sphinx-doc.org/en/master/' target='_blank'>Sphinx</a>, <a href='https://sphinx-book-theme.readthedocs.io' target='_blank'>sphinx-book-theme</a> and <a href='https://mystmd.org/guide' target='_blank'>MyST</a>"
}

# sphinxext.opengraph
ogp_social_cards = {
    "image": "_static/yuneta-logo2.png",
}

# -- ABlog config -------------------------------------------------
blog_path = "reference/blog"
blog_post_pattern = "reference/blog/*.md"
blog_baseurl = "https://sphinx-book-theme.readthedocs.io"
fontawesome_included = True
post_auto_image = 1
post_auto_excerpt = 2
nb_execution_show_tb = "READTHEDOCS" in os.environ
bibtex_bibfiles = []
# To test that style looks good with common bibtex config
bibtex_reference_style = "author_year"
bibtex_default_style = "plain"
numpydoc_show_class_members = False  # for automodule:: urllib.parse stub file issue
linkcheck_ignore = [
    "http://someurl/release",  # This is a fake link
    "https://doi.org",  # These don't resolve properly and cause SSL issues
]


def setup(app):
    # -- To demonstrate ReadTheDocs switcher -------------------------------------
    # This links a few JS and CSS files that mimic the environment that RTD uses
    # so that we can test RTD-like behavior. We don't need to run it on RTD and we
    # don't wanted it loaded in GitHub Actions because it messes up the lighthouse
    # results.
    if not os.environ.get("READTHEDOCS") and not os.environ.get("GITHUB_ACTIONS"):
        app.add_css_file(
            "https://assets.readthedocs.org/static/css/readthedocs-doc-embed.css"
        )
        app.add_css_file("https://assets.readthedocs.org/static/css/badge_only.css")

        # Create the dummy data file so we can link it
        # ref: https://github.com/readthedocs/readthedocs.org/blob/bc3e147770e5740314a8e8c33fec5d111c850498/readthedocs/core/static-src/core/js/doc-embed/footer.js  # noqa: E501
        app.add_js_file("rtd-data.js")
        app.add_js_file(
            "https://assets.readthedocs.org/static/javascript/readthedocs-doc-embed.js",
            priority=501,
        )

def danger_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    node = nodes.inline(text, text)
    node['classes'].append('danger')
    return [node], []

roles.register_local_role('danger', danger_role)

def warning_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    node = nodes.inline(text, text)
    node['classes'].append('warning')
    return [node], []

roles.register_local_role('warning', warning_role)
