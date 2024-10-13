/****************************************************************************
 *          c_esp_ethernet.h
 *
 *          GClass Ethernet
 *          Low level esp-idf
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
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
GOBJ_DECLARE_GCLASS(C_ETHERNET);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DECLARE_STATE(ST_ETHERNET_WAIT_START);
GOBJ_DECLARE_STATE(ST_ETHERNET_WAIT_CONNECTED);
GOBJ_DECLARE_STATE(ST_ETHERNET_WAIT_IP);
GOBJ_DECLARE_STATE(ST_ETHERNET_IP_ASSIGNED);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_ETHERNET_START);
GOBJ_DECLARE_EVENT(EV_ETHERNET_STOP);
GOBJ_DECLARE_EVENT(EV_ETHERNET_LINK_UP);
GOBJ_DECLARE_EVENT(EV_ETHERNET_LINK_DOWN);
GOBJ_DECLARE_EVENT(EV_ETHERNET_GOT_IP);
GOBJ_DECLARE_EVENT(EV_ETHERNET_LOST_IP);
GOBJ_DECLARE_EVENT(EV_ETHERNET_ON_OPEN);
GOBJ_DECLARE_EVENT(EV_ETHERNET_ON_CLOSE);


/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_esp_ethernet(void);


#ifdef __cplusplus
}
#endif
