/****************************************************************************
 *          c_perf_ievent.h
 *          Perf_ievent GClass.
 *
 *          The client of the inter-yuno round trips of perf_ievent: sends
 *          EV_PERF_PING to the remote service `echo`, and the next one when
 *          the EV_PERF_PONG of the previous one comes back.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_PERF_IEVENT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_PERF_PING);
GOBJ_DECLARE_EVENT(EV_PERF_PONG);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_perf_ievent(void);

#ifdef __cplusplus
}
#endif
