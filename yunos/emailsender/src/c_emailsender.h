/****************************************************************************
 *          C_EMAILSENDER.H
 *          Emailsender GClass.
 *
 *          Email sender
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>
#include "c_curl.h"

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Interface
 *********************************************************************/
/*
 *  Available subscriptions for emailsender's users
 */
#define I_EMAILSENDER_SUBSCRIPTIONS    \
    {"EV_ON_SAMPLE1",               0,  0,  0}, \
    {"EV_ON_SAMPLE2",               0,  0,  0},


/**rst**
.. _emailsender-gclass:

**"Emailsender"** :ref:`GClass`
================================

Email sender

``GCLASS_EMAILSENDER_NAME``
   Macro of the gclass string name, i.e **"Emailsender"**.

``GCLASS_EMAILSENDER``
   Macro of the :func:`gclass_emailsender()` function.

**rst**/
PUBLIC GCLASS *gclass_emailsender(void);

#define GCLASS_EMAILSENDER_NAME "Emailsender"
#define GCLASS_EMAILSENDER gclass_emailsender()


#ifdef __cplusplus
}
#endif
