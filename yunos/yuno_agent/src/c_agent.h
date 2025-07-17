/****************************************************************************
 *          C_AGENT.H
 *          Agent GClass.
 *
 *          Yuneta Agent, the first authority of realms and yunos in a host
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>
#include "c_pty.h"

/**rst**

.. _agent-gclass:

**"Agent"** :ref:`GClass`
===========================

Description
===========

Yuneta Agent, the first authority of realms and yunos in a host

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

``GCLASS_AGENT_NAME``
   Macro of the gclass string name, i.e **"Agent"**.

``GCLASS_AGENT``
   Macro of the :func:`gclass_agent()` function.


**rst**/

#ifdef __cplusplus
extern "C"{
#endif

/**rst**
   Return a pointer to the :ref:`GCLASS` struct defining the :ref:`agent-gclass`.
**rst**/
PUBLIC GCLASS *gclass_agent(void);

#define GCLASS_AGENT_NAME "Agent"
#define GCLASS_AGENT gclass_agent()


#ifdef __cplusplus
}
#endif
