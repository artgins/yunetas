/****************************************************************************
 *          C_WN_LAYOUT.H
 *          Wn_layout GClass.
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>
#include "c_wn_stdscr.h"

/**rst**

.. _wn_layout-gclass:

**"Wn_layout"** :ref:`GClass`
===========================

Description
===========

UI Layout

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

``GCLASS_WN_LAYOUT_NAME``
   Macro of the gclass string name, i.e **"Wn_layout"**.

``GCLASS_WN_LAYOUT``
   Macro of the :func:`gclass_wn_layout()` function.


**rst**/

#ifdef __cplusplus
extern "C"{
#endif

/**rst**
   Return a pointer to the :ref:`GCLASS` struct defining the :ref:`wn_layout-gclass`.
**rst**/
PUBLIC GCLASS *gclass_wn_layout(void);

#define GCLASS_WN_LAYOUT_NAME "Wn_layout"
#define GCLASS_WN_LAYOUT gclass_wn_layout()


#ifdef __cplusplus
}
#endif
