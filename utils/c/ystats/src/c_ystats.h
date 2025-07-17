/****************************************************************************
 *          C_YSTATS.H
 *          YStats GClass.
 *
 *          Yuneta Statistics
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta_tls.h>

#ifdef __cplusplus
extern "C"{
#endif

/**rst**
.. _ystats-gclass:

**"YStats"** :ref:`GClass`
================================

Yuneta Statistics

``GCLASS_YSTATS_NAME``
   Macro of the gclass string name, i.e **"YStats"**.

``GCLASS_YSTATS``
   Macro of the :func:`gclass_ystats()` function.

**rst**/
PUBLIC GCLASS *gclass_ystats(void);

#define GCLASS_YSTATS_NAME "YStats"
#define GCLASS_YSTATS gclass_ystats()


#ifdef __cplusplus
}
#endif
