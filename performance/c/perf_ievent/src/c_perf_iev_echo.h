/****************************************************************************
 *          c_perf_iev_echo.h
 *          Perf_iev_echo GClass.
 *
 *          The service `echo` of perf_ievent: answers every EV_PERF_PING it
 *          receives through the gate with an EV_PERF_PONG, published to the
 *          remote subscription of the client.
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
GOBJ_DECLARE_GCLASS(C_PERF_IEV_ECHO);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_perf_iev_echo(void);

#ifdef __cplusplus
}
#endif
