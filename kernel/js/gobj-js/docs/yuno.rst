Yuno
====

Container of all gobjs.

Unlike C Yuneta, the javascript yuno has not event loop.

To create a Yuno:

.. code-block:: javascript

    var yuno = new Yuno(name, role, settings);

:param name: string with the name of yuno.
:param role: string with the role of yuno.
:param settings: dictionary with the yuno settings.


Yuno has available the next methods:

* :ref:`gobj_create`
* :ref:`gobj_destroy`
* :ref:`find_unique_gobj`
* :ref:`send_inter_event`
* :ref:`subscribe_inter_event`
* :ref:`unsubscribe_inter_event`


Yuno is a subclass of :ref:`GObj`, so you have too all his methods.

.. _gobj_create:

gobj_create
-----------

Create a new gobj:

.. code-block:: javascript

    var gobj = yuno.gobj_create(name, gclass, kw, parent);

:param name: name string of gobj.
:param gclass: gclass type.
:param kw: keyword arguments.
:param parent: parent of gobj.
:return: new gobj instance.


.. _gobj_destroy:

gobj_destroy
------------

Destroy a gobj:

.. code-block:: javascript

    yuno.gobj_destroy(gobj);

:param gobj: gobj to destroy.


.. _find_unique_gobj:

find_unique_gobj
----------------

Find a :term:`unique-named-gobj`.

.. code-block:: javascript

    var gobj = yuno.find_unique_gobj(name);

:param name: name of gobj.
:return: gobj found or ``undefined`` if not found.


.. _send_inter_event:

send_inter_event
----------------

Send an event to an external yuno.

.. code-block:: javascript

    yuno.send_inter_event(
        iev_dst_yuno,
        iev_dst_role,
        iev_dst_srv,
        event,
        kw,
        iev_src_srv
    );

:param iev_dst_yuno: name of external yuno.
:param iev_dst_role: role of external yuno.
:param iev_dst_srv: service or public gobj of external yuno.
:param event: event name to send
:param kw: keyword arguments of event
:param iev_src_srv: service or public gobj sending the event


.. _subscribe_inter_event:

subscribe_inter_event
---------------------

Subscribe an event from an external yuno.

.. code-block:: javascript

    yuno.subscribe_inter_event(yuno_name, gobj_name, event_name, kw, subscriber_gobj);

:param yuno_name: name of external yuno.
:param gobj_name: name of external gobj.
:param event_name: event to subscribe.
:param kw: keyword arguments of the subscription.
:param subscriber_gobj: gobj subscribing.

.. _unsubscribe_inter_event:

unsubscribe_inter_event
-----------------------

Unsubscribe an event from an external yuno.

.. code-block:: javascript

    yuno.unsubscribe_inter_event(yuno_name, gobj_name, event_name, kw, subscriber_gobj);

:param yuno_name: name of external yuno.
:param gobj_name: name of external gobj.
:param event_name: event to subscribe.
:param kw: keyword arguments of the subscription.
:param subscriber_gobj: gobj subscribing.




Examples Yuno
===============

Create a new yuno instance:

.. code-block:: javascript

        yuno = new Yuno('sample', ['role'], {});

