/****************************************************************************
 *          C_LOGCENTER.H
 *          Logcenter GClass.
 *
 *          Log Center
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Interface
 *********************************************************************/
/*
 *  Available subscriptions for logcenter's users
 */
#define I_LOGCENTER_SUBSCRIPTIONS    \
    {"EV_ON_SAMPLE1",               0,  0,  0}, \
    {"EV_ON_SAMPLE2",               0,  0,  0},


/**rst**
.. _logcenter-gclass:

**"Logcenter"** :ref:`GClass`
================================

Log Center

``GCLASS_LOGCENTER_NAME``
   Macro of the gclass string name, i.e **"Logcenter"**.

``GCLASS_LOGCENTER``
   Macro of the :func:`gclass_logcenter()` function.

**rst**/
PUBLIC GCLASS *gclass_logcenter(void);

#define GCLASS_LOGCENTER_NAME "Logcenter"
#define GCLASS_LOGCENTER gclass_logcenter()


#ifdef __cplusplus
}
#endif
