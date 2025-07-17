/****************************************************************************
 *          C_WN_TOOLBAR.H
 *          Wn_toolbar GClass.
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>
#include "c_wn_stdscr.h"

/**rst**

.. _wn_toolbar-gclass:

**"Wn_toolbar"** :ref:`GClass`
===========================

Description
===========

Tool bar

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

``GCLASS_WN_TOOLBAR_NAME``
   Macro of the gclass string name, i.e **"Wn_toolbar"**.

``GCLASS_WN_TOOLBAR``
   Macro of the :func:`gclass_wn_toolbar()` function.


**rst**/

#ifdef __cplusplus
extern "C"{
#endif

/**rst**
   Return a pointer to the :ref:`GCLASS` struct defining the :ref:`wn_toolbar-gclass`.
**rst**/
PUBLIC GCLASS *gclass_wn_toolbar(void);

#define GCLASS_WN_TOOLBAR_NAME "Wn_toolbar"
#define GCLASS_WN_TOOLBAR gclass_wn_toolbar()


#ifdef __cplusplus
}
#endif
