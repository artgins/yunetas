/****************************************************************************
 *          C_YBATCH.H
 *          YBatch GClass.
 *
 *          Yuneta Batch
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta_tls.h>

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Interface
 *********************************************************************/
/*
 *  Available subscriptions for ybatch's users
 */
#define I_YBATCH_SUBSCRIPTIONS    \
    {"EV_ON_SAMPLE1",               0,  0,  0}, \
    {"EV_ON_SAMPLE2",               0,  0,  0},


/**rst**
.. _ybatch-gclass:

**"YBatch"** :ref:`GClass`
================================

Yuneta Batch

``GCLASS_YBATCH_NAME``
   Macro of the gclass string name, i.e **"YBatch"**.

``GCLASS_YBATCH``
   Macro of the :func:`gclass_ybatch()` function.

**rst**/
PUBLIC GCLASS *gclass_ybatch(void);

#define GCLASS_YBATCH_NAME "YBatch"
#define GCLASS_YBATCH gclass_ybatch()


#ifdef __cplusplus
}
#endif
