web-skeleton3
=============

Generator of static html/css/js code tree.

Based on:
    - mako_
    - webassets_

.. _mako: https://pypi.python.org/pypi/Mako
.. _webassets: https://pypi.python.org/pypi/webassets

Install
-------

Install with::

    pip3 install web-skeleton3

Remember install::

    sudo apt-get install ruby-dev ruby-rubygems
    sudo gem install compass
    sudo pip3 install scour
    sudo pip3 install cssutils
    sudo pip3 install rjsmin

Upload to pypi
--------------

python setup.py sdist bdist_wheel
python -m twine upload dist/*

License
-------

Licensed under the  `The MIT License <http://www.opensource.org/licenses/mit-license>`_.
See LICENSE.txt in the source distribution for details.
