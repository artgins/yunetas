/****************************************************************************
 *          C_YCOMMAND.H
 *          YCommand GClass.
 *
 *          Yuneta Command utility
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta_tls.h>
#include "c_editline.h"

#ifdef __cplusplus
extern "C"{
#endif

/**rst**
.. _ycommand-gclass:

**"YCommand"** :ref:`GClass`
================================

Yuneta Statistics

``GCLASS_YCOMMAND_NAME``
   Macro of the gclass string name, i.e **"YCommand"**.

``GCLASS_YCOMMAND``
   Macro of the :func:`gclass_ycommand()` function.

**rst**/
PUBLIC GCLASS *gclass_ycommand(void);

#define GCLASS_YCOMMAND_NAME "YCommand"
#define GCLASS_YCOMMAND gclass_ycommand()


#ifdef __cplusplus
}
#endif
