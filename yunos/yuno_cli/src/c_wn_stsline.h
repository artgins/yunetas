/****************************************************************************
 *          C_WN_STSLINE.H
 *          Wn_stsline GClass.
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>
#include "c_wn_stdscr.h"

/**rst**

.. _wn_stsline-gclass:

**"Wn_stsline"** :ref:`GClass`
===========================

Description
===========

Status Line

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

``GCLASS_WN_STSLINE_NAME``
   Macro of the gclass string name, i.e **"Wn_stsline"**.

``GCLASS_WN_STSLINE``
   Macro of the :func:`gclass_wn_stsline()` function.


**rst**/

#ifdef __cplusplus
extern "C"{
#endif

/**rst**
   Return a pointer to the :ref:`GCLASS` struct defining the :ref:`wn_stsline-gclass`.
**rst**/
PUBLIC GCLASS *gclass_wn_stsline(void);

#define GCLASS_WN_STSLINE_NAME "Wn_stsline"
#define GCLASS_WN_STSLINE gclass_wn_stsline()


#ifdef __cplusplus
}
#endif
