/****************************************************************************
 *          C_CLI.H
 *          Cli GClass.
 *
 *          Yuneta Command Line Interface
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta_tls.h>
#include "c_wn_stdscr.h"
#include "c_wn_layout.h"
#include "c_wn_box.h"
#include "c_wn_stsline.h"
#include "c_wn_editline.h"
#include "c_wn_list.h"
#include "c_wn_toolbar.h"
#include "c_wn_static.h"

/**rst**

.. _cli-gclass:

**"Cli"** :ref:`GClass`
===========================

Description
===========

Yuneta Command Line Interface

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

``GCLASS_CLI_NAME``
   Macro of the gclass string name, i.e **"Cli"**.

``GCLASS_CLI``
   Macro of the :func:`gclass_cli()` function.


**rst**/

#ifdef __cplusplus
extern "C"{
#endif

/**rst**
   Return a pointer to the :ref:`GCLASS` struct defining the :ref:`cli-gclass`.
**rst**/
PUBLIC GCLASS *gclass_cli(void);

#define GCLASS_CLI_NAME "Cli"
#define GCLASS_CLI gclass_cli()


#ifdef __cplusplus
}
#endif
