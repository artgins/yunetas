/****************************************************************************
 *          C_GSS_UDP_S.H
 *          GssUdpS GClass.
 *
 *          Gossamer UDP Server
 *
            Api Gossamer
            ------------

            Input Events:
            - EV_SEND_MESSAGE

            Output Events:
            - EV_ON_OPEN
            - EV_ON_CLOSE
            - EV_ON_MESSAGE

 *
 *          Copyright (c) 2014 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_GSS_UDP_S);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_gss_udp_s(void);

#ifdef __cplusplus
}
#endif
