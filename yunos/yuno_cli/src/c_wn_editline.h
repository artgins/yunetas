/****************************************************************************
 *          C_WN_EDITLINE.H
 *          Wn_editline GClass.
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>
#include "c_wn_stdscr.h"

/**rst**

.. _wn_editline-gclass:

**"Wn_editline"** :ref:`GClass`
===========================

Description
===========

Edit Line

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

``GCLASS_WN_EDITLINE_NAME``
   Macro of the gclass string name, i.e **"Wn_editline"**.

``GCLASS_WN_EDITLINE``
   Macro of the :func:`gclass_wn_editline()` function.


**rst**/

#ifdef __cplusplus
extern "C"{
#endif


/**rst**
   Return a pointer to the :ref:`GCLASS` struct defining the :ref:`wn_editline-gclass`.
**rst**/
PUBLIC GCLASS *gclass_wn_editline(void);

#define GCLASS_WN_EDITLINE_NAME "Wn_editline"
#define GCLASS_WN_EDITLINE gclass_wn_editline()


#ifdef __cplusplus
}
#endif
