/****************************************************************************
 *          C_TEST8.H
 *
 *          A class to test C_TCP: a connection accepted by the legacy method
 *          into a channel with no C_TCP (a volatile clisrv) dropped from
 *          inside its EV_RX_DATA, over TLS and in clear
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
GOBJ_DECLARE_GCLASS(C_TEST8);
GOBJ_DECLARE_GCLASS(C_TEST8_HOST);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test8(void);


#ifdef __cplusplus
}
#endif
