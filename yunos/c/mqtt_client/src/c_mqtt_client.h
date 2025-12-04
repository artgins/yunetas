/****************************************************************************
 *          C_MQTT_CLIENT.H
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
GOBJ_DECLARE_GCLASS(C_MQTT_CLIENT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_SEND_MQTT_PUBLISH);
GOBJ_DECLARE_EVENT(EV_SEND_MQTT_SUBSCRIBE);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_mqtt_client(void);

#ifdef __cplusplus
}
#endif
