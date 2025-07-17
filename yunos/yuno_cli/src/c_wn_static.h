/****************************************************************************
 *          C_WN_STATIC.H
 *          Wn_static GClass.
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>
#include "c_wn_stdscr.h"

/**rst**

.. _wn_static-gclass:

**"Wn_static"** :ref:`GClass`
===========================

Description
===========

Static control

Events
======

Input Events
------------

Order
^^^^^

Request
^^^^^^^

Output Events
-------------

Response
^^^^^^^^

Unsolicited
^^^^^^^^^^^

Macros
======

``GCLASS_WN_STATIC_NAME``
   Macro of the gclass string name, i.e **"Wn_static"**.

``GCLASS_WN_STATIC``
   Macro of the :func:`gclass_wn_static()` function.


**rst**/

#ifdef __cplusplus
extern "C"{
#endif

/**rst**
   Return a pointer to the :ref:`GCLASS` struct defining the :ref:`wn_static-gclass`.
**rst**/
PUBLIC GCLASS *gclass_wn_static(void);

#define GCLASS_WN_STATIC_NAME "Wn_static"
#define GCLASS_WN_STATIC gclass_wn_static()


#ifdef __cplusplus
}
#endif
