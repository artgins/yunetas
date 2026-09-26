/****************************************************************************
 *          C_TEST5.H
 *
 *          Test a BURST of TLS messages sent at once, as a client resending its window
 *
 *          Tasks
 *          - Play pepon as server with echo
 *          - Open __out_side__
 *          - On open, send 400 messages of 64 KB at once (25 MB)
 *          - Verify every echoed message: complete, intact, in order
 *          - Shutdown
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
GOBJ_DECLARE_GCLASS(C_TEST5);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test5(void);


#ifdef __cplusplus
}
#endif
