/****************************************************************************
 *          C_TEST_CC_PEER.H
 *
 *          The far end of the control center's gates, in a test: a side
 *          (__top_side__, __input_side__), a channel of a side, or the
 *          transport below an agent's C_IEVENT_SRV. It records what reaches it.
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
GOBJ_DECLARE_GCLASS(C_TEST_CC_PEER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_cc_peer(void);

#ifdef __cplusplus
}
#endif
