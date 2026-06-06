/****************************************************************************
 *          C_MQTT.H
 *          GClass of MQTT protocol.
 *
 *  ⚠️ DEPRECATED — do NOT use in new developments.
 *  C_PROT_MQTT is kept only until the gates still using it migrate to
 *  C_PROT_MQTT2. Use c_prot_mqtt2.h (C_PROT_MQTT2), the complete MQTT
 *  implementation, for all new work. This gclass will be removed once the
 *  last consumer migrates.
 *
 *  Implementation of the MQTT protocol.
 *


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

#ifdef __cplusplus
extern "C"{
#endif


/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_PROT_MQTT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_prot_mqtt(void);


#ifdef __cplusplus
}
#endif
