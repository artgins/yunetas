/****************************************************************************
 *          C_TEST13.H
 *
 *          A class to test C_TCP: txMsgs and cur_tx_bytes, in clear and
 *          over TLS
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
GOBJ_DECLARE_GCLASS(C_TEST13);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_SEND_THE_MESSAGES);   // posted when both clients connected

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test13(void);


#ifdef __cplusplus
}
#endif
