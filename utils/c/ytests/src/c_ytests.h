/****************************************************************************
 *          C_YTESTS.H
 *          YTests GClass.
 *
 *          Yuneta Tests
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
 *  Available subscriptions for ytests's users
 */
#define I_YTESTS_SUBSCRIPTIONS    \
    {"EV_ON_SAMPLE1",               0,  0,  0}, \
    {"EV_ON_SAMPLE2",               0,  0,  0},


/**rst**
.. _ytests-gclass:

**"YTests"** :ref:`GClass`
================================

Yuneta Tests

``GCLASS_YTESTS_NAME``
   Macro of the gclass string name, i.e **"YTests"**.

``GCLASS_YTESTS``
   Macro of the :func:`gclass_ytests()` function.

**rst**/
PUBLIC GCLASS *gclass_ytests(void);

#define GCLASS_YTESTS_NAME "YTests"
#define GCLASS_YTESTS gclass_ytests()


#ifdef __cplusplus
}
#endif
