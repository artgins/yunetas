/****************************************************************************
 *          C_MQTT.H
 *          GClass of MQTT protocol.
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
