/****************************************************************************
 *          C_TEST4.H
 *
 *          Test C_TCP reconnect-on-tx keeps the pending dl_tx across FAILED
 *          reconnects (inactivity model). Regression for the dl_tx-loss fixed
 *          in c_tcp.c: bytes queued while disconnected (server down) must
 *          survive the failed connect retries and be delivered once the
 *          server comes up.
 *
 *          Copyright (c) 2024-2026, ArtGins.
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
GOBJ_DECLARE_GCLASS(C_TEST4);

/*------------------------*
 *      Prototypes
 *------------------------*/
PUBLIC int register_c_test4(void);


#ifdef __cplusplus
}
#endif
