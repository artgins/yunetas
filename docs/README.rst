README
======

The documentation is built with `sphinx`_.

The sphinx package is installed using `pipx`_

It has been tested in Kubuntu >= 23.04.

Instructions to build the documentation
---------------------------------------



Install `pipx`_ ::

    sudo apt update
    sudo apt install pipx
    pipx ensurepath

Install `sphinx`_  with **pipx** ::

    pipx install sphinx
    pipx inject sphinx sphinx-book-theme
    pipx inject sphinx sphinx_copybutton


Build sphinx html ::

    cd doc.yuneta.io
    make html

.. _pipx:       https://pipx.pypa.io/stable/installation/
.. _sphinx:     https://www.sphinx-doc.org/
