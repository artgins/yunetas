/****************************************************************************
 *          C_TEST4.H
 *
 *          Test large TLS payload (triggers many TLS records)
 *
 *          Tasks
 *          - Play pepon as server with echo
 *          - Open __out_side__
 *          - On open, send a 1 MB message
 *          - On receiving the echoed message, verify integrity
 *          - Shutdown
 *
 *          Copyright (c) 2024, Artgins.
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
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test4(void);


#ifdef __cplusplus
}
#endif
