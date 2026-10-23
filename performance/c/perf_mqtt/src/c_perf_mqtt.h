/****************************************************************************
 *          c_perf_mqtt.h
 *          Perf_mqtt GClass.
 *
 *          The client of perf_mqtt: publishes QoS 0 on a topic it is
 *          subscribed to, through the embedded broker, and publishes the
 *          next message when the previous one comes back.
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
GOBJ_DECLARE_GCLASS(C_PERF_MQTT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_perf_mqtt(void);

#ifdef __cplusplus
}
#endif
