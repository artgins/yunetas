/****************************************************************************
 *          c_esp_wifi.h
 *
 *          GClass Wifi
 *          Low level esp-idf
 *
 *          Copyright (c) 2023 Niyamaka, 2024 ArtGins.
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
GOBJ_DECLARE_GCLASS(C_WIFI);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DECLARE_STATE(ST_WIFI_WAIT_START);
GOBJ_DECLARE_STATE(ST_WIFI_WAIT_SCAN);
GOBJ_DECLARE_STATE(ST_WIFI_WAIT_SSID_CONF);
GOBJ_DECLARE_STATE(ST_WIFI_WAIT_STA_CONNECTED);
GOBJ_DECLARE_STATE(ST_WIFI_WAIT_IP);
GOBJ_DECLARE_STATE(ST_WIFI_IP_ASSIGNED);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_WIFI_STA_START);
GOBJ_DECLARE_EVENT(EV_WIFI_STA_STOP);
GOBJ_DECLARE_EVENT(EV_WIFI_STA_CONNECTED);
GOBJ_DECLARE_EVENT(EV_WIFI_STA_DISCONNECTED);
GOBJ_DECLARE_EVENT(EV_WIFI_SCAN_DONE);
GOBJ_DECLARE_EVENT(EV_WIFI_SMARTCONFIG_DONE);
GOBJ_DECLARE_EVENT(EV_WIFI_GOT_IP);
GOBJ_DECLARE_EVENT(EV_WIFI_LOST_IP);
GOBJ_DECLARE_EVENT(EV_WIFI_ON_OPEN);
GOBJ_DECLARE_EVENT(EV_WIFI_ON_CLOSE);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_esp_wifi(void);

#ifdef __cplusplus
}
#endif
