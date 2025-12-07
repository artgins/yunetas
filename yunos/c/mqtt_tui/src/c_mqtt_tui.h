/****************************************************************************
 *          C_MQTT_TUI.H
 *          Mqtt_client GClass.
 *
 *          Mqtt client
 *
 *          Copyright (c) 2025, ArtGins.
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
GOBJ_DECLARE_GCLASS(C_MQTT_TUI);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_SEND_MQTT_SUBSCRIBE);
GOBJ_DECLARE_EVENT(EV_SEND_MQTT_UNSUBSCRIBE);
GOBJ_DECLARE_EVENT(EV_SEND_MQTT_PUBLISH);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_mqtt_client(void);

#ifdef __cplusplus
}
#endif
