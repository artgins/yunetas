/****************************************************************************
 *          C_WN_LIST.H
 *          Wn_list GClass.
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>
#include "c_wn_stdscr.h"

/**rst**

.. _wn_list-gclass:

**"Wn_list"** :ref:`GClass`
===========================

Description
===========

UI List

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

``GCLASS_WN_LIST_NAME``
   Macro of the gclass string name, i.e **"Wn_list"**.

``GCLASS_WN_LIST``
   Macro of the :func:`gclass_wn_list()` function.


**rst**/

#ifdef __cplusplus
extern "C"{
#endif

/**rst**
   Return a pointer to the :ref:`GCLASS` struct defining the :ref:`wn_list-gclass`.
**rst**/
PUBLIC GCLASS *gclass_wn_list(void);

#define GCLASS_WN_LIST_NAME "Wn_list"
#define GCLASS_WN_LIST gclass_wn_list()


#ifdef __cplusplus
}
#endif
