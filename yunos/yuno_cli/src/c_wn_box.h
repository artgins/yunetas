/****************************************************************************
 *          C_WN_BOX.H
 *          Wn_box GClass.
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>
#include "c_wn_stdscr.h"

/**rst**

.. _wn_box-gclass:

**"Wn_box"** :ref:`GClass`
===========================

Description
===========

UI Box

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

``GCLASS_WN_BOX_NAME``
   Macro of the gclass string name, i.e **"Wn_box"**.

``GCLASS_WN_BOX``
   Macro of the :func:`gclass_wn_box()` function.


**rst**/

#ifdef __cplusplus
extern "C"{
#endif

/**rst**
   Return a pointer to the :ref:`GCLASS` struct defining the :ref:`wn_box-gclass`.
**rst**/
PUBLIC GCLASS *gclass_wn_box(void);

#define GCLASS_WN_BOX_NAME "Wn_box"
#define GCLASS_WN_BOX gclass_wn_box()


#ifdef __cplusplus
}
#endif
