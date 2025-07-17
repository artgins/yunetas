/****************************************************************************
 *          C_WATCHFS.H
 *          Watchfs GClass.
 *
 *
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#ifndef _C_WATCHFS_H
#define _C_WATCHFS_H 1

#include <yuneta.h>

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Interface
 *********************************************************************/
/*
 *  Available subscriptions for watchfs's users
 */
#define I_WATCHFS_SUBSCRIPTIONS    \
    {"EV_ON_SAMPLE1",               0,  0,  0}, \
    {"EV_ON_SAMPLE2",               0,  0,  0},


/**rst**
.. _watchfs-gclass:

**"Watchfs"** :ref:`GClass`
================================



``GCLASS_WATCHFS_NAME``
   Macro of the gclass string name, i.e **"Watchfs"**.

``GCLASS_WATCHFS``
   Macro of the :func:`gclass_watchfs()` function.

**rst**/
PUBLIC GCLASS *gclass_watchfs(void);

#define GCLASS_WATCHFS_NAME "Watchfs"
#define GCLASS_WATCHFS gclass_watchfs()


#ifdef __cplusplus
}
#endif

#endif
