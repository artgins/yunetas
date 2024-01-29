.. jsyuneta documentation master file, created by
   sphinx-quickstart on Fri Mar 15 16:01:21 2013.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

JsYuneta
========

JsYuneta is a porting of C `yuneta <http://yuneta.io>`_
framework to javascript.

Important differences from `yuneta <http://yuneta.io>`_:

* All gobjs are :term:`unique-named-gobj`.
  If you don't supply a name then a random name is created.
  (That is to have a better link between jquery tree and jsyuneta tree)

* The configuration items are in ``config`` variable.

* The keyword arguments of events are in ``kw`` variable.


Yuno
====

.. toctree::
    :maxdepth: 1

    yuno


GObj
====

.. toctree::
    :maxdepth: 1

    gobj



Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
