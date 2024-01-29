.. _GObj:

GObj
====


See :ref:`create_gclass` to know how create your automata,
deriving the GObj class.

GObj has available the next style `yuneta <http://yune>`_ methods:

* :ref:`mt_create`
* :ref:`send_event`
* :ref:`publish_event`
* :ref:`subscribe_event`
* :ref:`delete_subscription`
* :ref:`change_state`
* :ref:`current_state`

And these specific of javascript/browser:

* :ref:`build_jquery_link_list`
* :ref:`child_by_index`
* :ref:`set_machine_trace`
* :ref:`set_timeout`
* :ref:`clear_timeout`


.. _create_gclass:

Creating a GClass
-----------------

A GClass is a subclass of GObj class.

A GClass (class defining a gobj) must have to be defined as:

.. code-block:: javascript

    var GCLASS_FSM = {};
    var GCLASS_CONFIG = {};

    var GClass = GObj.__makeSubclass__();
    var proto = GClass.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        this.name = name || '';  // set before super(), to put the same smachine name
        GObj.prototype.__init__.call(this, GCLASS_FSM, GCLASS_CONFIG);
        __update_dict__(this.config, kw || {});
        return this;
    };
    proto.mt_create= function() {
        /**********************************
         *      mt_create method
         **********************************/
    }


.. _mt_create:

mt_create
---------

Initialization method.

Override this method in your gclass to do the gobj initialization.

.. code-block:: javascript

    mt_create();


.. _send_event:

send_event
----------

Send an event.

.. code-block:: javascript

    var result = gobj.send_event(destination, event, kw);

:param destination: gobj to send the event.
:param event: event to send.
:param kw: keyword arguments of the event.
:return: return the return of the executed action.


.. _publish_event:

publish_event
---------------

Broadcast an event.

Send the event to all the event's subscribers.

.. code-block:: javascript

    gobj.publish_event(event, kw);

:param event: event to send.
:param kw: keyword arguments of the event.


.. _subscribe_event:

subscribe_event
---------------

Subcribe to an event.

.. code-block:: javascript

    gobj.subscribe_event(event_name, kw, subscriber_gobj);

:param event_name: event to subscribe.
:param kw: keyword arguments of the subscription.
:param subscriber_gobj: gobj subscribing.


.. _delete_subscription:

delete_subscription
-------------------

Delete a subscription.

.. code-block:: javascript

    gobj.delete_subscription(event_name, subscriber_gobj);

:param event_name: event to subscribe.
:param subscriber_gobj: gobj subscribing.


.. _change_state:

change_state
-------------

Set a new state.

This method is a wrapping of SMachine.change_state.

.. code-block:: javascript

    gobj.change_state(new_state);

:param new_state: new state to set.


.. _current_state:

current_state
-----------------

Get the current state.

This method is a wrapping of SMachine.current_state.

.. code-block:: javascript

    var cur_state = gobj.current_state();


.. _build_jquery_link_list:

build_jquery_link_list
----------------------

GObj has jquery helpers.

You can associate the gobj tree with the DOM tree through JQuery elements.

An gobj has two variables that you can use to link a parallel jquery/Dom tree:

* $head_insert_point
* $tail_insert_point

Build the jquery link list.

.. code-block:: javascript

    gobj.build_jquery_link_list();


.. _child_by_index:

child_by_index
--------------

Get the n-child of a gobj.

.. code-block:: javascript

    var child = gobj.child_by_index(n);

:param n: n-order child.
:return: gobj found or ``undefined`` if not found.


.. _set_machine_trace:

set_machine_trace
-----------------

Set trace of the gobj's machine.

.. code-block:: javascript

    gobj.set_machine_trace(value);

:param value: true or false.


.. _set_timeout:

set_timeout
-----------

Set a timeout event to the gobj.

In jsfsm world, there is only one event timer per gobj/machine.

This method is a wrapping of SMachine.set_timeout.

.. code-block:: javascript

    gobj.set_timeout(msec);

:param msec: timeout in miliseconds.


.. _clear_timeout:

clear_timeout
-------------

Clear the current timeout of the gobj.

This method is a wrapping of SMachine.clear_timeout.

.. code-block:: javascript

    gobj.clear_timeout();

