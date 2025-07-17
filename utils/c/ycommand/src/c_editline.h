/****************************************************************************
 *          C_EDITLINE.H
 *          Editline GClass.
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>

/**rst**

.. _editline-gclass:

**"Editline"** :ref:`GClass`
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

``GCLASS_EDITLINE_NAME``
   Macro of the gclass string name, i.e **"Editline"**.

``GCLASS_EDITLINE``
   Macro of the :func:`gclass_editline()` function.


**rst**/

#ifdef __cplusplus
extern "C"{
#endif


/**rst**
   Return a pointer to the :ref:`GCLASS` struct defining the :ref:`editline-gclass`.
**rst**/
PUBLIC GCLASS *gclass_editline(void);

#define GCLASS_EDITLINE_NAME "Editline"
#define GCLASS_EDITLINE gclass_editline()


#ifdef __cplusplus
}
#endif
