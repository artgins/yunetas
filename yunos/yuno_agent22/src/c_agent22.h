/****************************************************************************
 *          C_AGENT22.H
 *          Agent22 GClass.
 *
 *          Yuneta Agent22, the first authority of realms and yunos in a host
 *
 *          Copyright (c) 2022 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>
#include "c_pty.h"

/**rst**

.. _agent22-gclass:

**"Agent22"** :ref:`GClass`
===========================

Description
===========

Yuneta Agent22, the first authority of realms and yunos in a host

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

``GCLASS_AGENT22_NAME``
   Macro of the gclass string name, i.e **"Agent22"**.

``GCLASS_AGENT22``
   Macro of the :func:`gclass_agent22()` function.


**rst**/

#ifdef __cplusplus
extern "C"{
#endif

/**rst**
   Return a pointer to the :ref:`GCLASS` struct defining the :ref:`agent22-gclass`.
**rst**/
PUBLIC GCLASS *gclass_agent22(void);

#define GCLASS_AGENT22_NAME "Agent22"
#define GCLASS_AGENT22 gclass_agent22()


#ifdef __cplusplus
}
#endif
