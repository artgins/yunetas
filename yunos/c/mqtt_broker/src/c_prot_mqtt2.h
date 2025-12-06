/****************************************************************************
 *          C_MQTT.H
 *          GClass of MQTT protocol.
 *
 *  Implementation of the MQTT protocol.
 *

TODO review this schema

    Input Events                                Output Events

                    ┌───────────────────────┐
        start   ━━━▷│●                      │
                    │-----------------------│
                    │                       │
                    │                       │====▷  EV_ON_OPEN
                    │                       │====▷  EV_ON_MESSAGE
                    │                       │====▷  EV_ON_CLOSE
                    │                       │
                    │-----------------------│
        stop    ━━━▷│■  ◁--(auto) in clisrv │====▷  EV_STOPPED
                    └───────────────────────┘


 *          Copyright (c) 2022 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>
#include "mqtt_util.h"

#ifdef __cplusplus
extern "C"{
#endif


/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_PROT_MQTT2);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/*
 *  Like the connect callback. This is called when the client receives a CONNACK
 */
GOBJ_DECLARE_EVENT(EV_MQTT_CONNECTED);

/*
 *  Like the disconnect callback. This is called when the broker has received the
 *  DISCONNECT command and has disconnected the client.
 */
GOBJ_DECLARE_EVENT(EV_MQTT_DISCONNECTED);

/*
 *  Like the publish callback. This is called when a message initiated with
 *  cmd_publish has been sent to the broker. "Sent" means different
 *  things depending on the QoS of the message:
 *
 *  QoS 0: The PUBLISH was passed to the local operating system for delivery,
 *         there is no guarantee that it was delivered to the remote broker.
 *  QoS 1: The PUBLISH was sent to the remote broker and the corresponding
 *         PUBACK was received by the library.
 *  QoS 2: The PUBLISH was sent to the remote broker and the corresponding
 *         PUBCOMP was received by the library.
 */
GOBJ_DECLARE_EVENT(EV_MQTT_PUBLISH);

/*
 *  Like the message callback. This is called when a message is received from the
 *  broker and the required QoS flow has completed.
 */
GOBJ_DECLARE_EVENT(EV_MQTT_MESSAGE);

/*
 *  Like the subscribe callback. This is called when the library receives a
 *  SUBACK message in response to a SUBSCRIBE
 */
GOBJ_DECLARE_EVENT(EV_MQTT_SUBSCRIBE);

/*
 *  Like the unsubscribe callback. This is called when the library receives a
 *  UNSUBACK message in response to an UNSUBSCRIBE.
 */
GOBJ_DECLARE_EVENT(EV_MQTT_UNSUBSCRIBE);


/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_prot_mqtt2(void);


#ifdef __cplusplus
}
#endif
