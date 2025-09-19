/****************************************************************************
 *          C_IOGATE.H
 *          Iogate GClass.
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif


/*********************************************************************
 *      Constants
 *********************************************************************/

/**rst**
 *

TODO IOGate crea __temp__ con iogate y iogate_gobj

==================================================================
Api (Gotter)IOGate <-> Channel <-> Tube (old Spiderden<->Gossamer)
==================================================================

IOGate: Old spiderden, centralized point of all iogates (old gossamers).
        His goal is to have and execute the routing rules of messages,
        (actually this job is done by his mirror (gotter))
        and have all logs, statistics, and operation in a central point
        (Don't repeat the same in all gotters).

Channel: Is in part the old gossamer.
        Actually the old gossamer has been splitted in Channels and Tubes.
        The tube is the old gossamer,
        and the iogate is a colletion of tubes belong to the same logic connection.
        The goal of iogate is to do the distribution of messages between tubes.
        All tubes (server or/and clients) of a iogate are binded to the same local ip.


    HACK This architecture close a superior circle.

    Client
    ======

    Data Flow                                                                  Connector
    ---------                                                                  ---------

    service (Ex: yunos and yuneta utilities (yuneta,ybatch,...)                agent_client
        │    being clients of agent service)
        │                                   Input               Output
        │                                   -----               ------
        │
        └━━━▷ IEvent_cli                    IEV_MESSAGE/IEvent  SEND_MESSAGE
                │
                └━━━▷ IOGate                ON_MESSAGE/GBuffer  SEND_MESSAGE
                    │
                    └━━━▷ Channel           ON_MESSAGE/GBuffer  SEND_MESSAGE
                        │
                        └━━━▷ Tube          RX_DATA             TX_DATA
                            (Ex: GWebsocket)

    Server
    ======

    Data Flow                                                                   Connector
    ---------                                                                   ---------

    service (Your offer, Ex: agent)                                             agent_client
        │
        │                                   Input               Output
        │                                   -----               ------
        │
        └━━━▷ IOGate                        IEV_MESSAGE/IEvent  SEND_MESSAGE
                │
                └━━━▷ IEvent_srv            ON_MESSAGE/GBuffer  SEND_MESSAGE
                    │
                    └━━━▷ Channel           ON_MESSAGE/GBuffer  SEND_MESSAGE
                        │
                        └━━━▷ Tube          RX_DATA             TX_DATA
                            (Ex: GWebsocket)
                        ...


│ TODO Esto de abajo hay que revisarlo, lo bueno es lo de arriba.
▼

            ┌───────────────────┐           ┌───────────────────┐
            │                   │           │                   │
            │  IOGate's parent  │           │  IOGate's parent  │
            │                   │           │                   │
            └───────────────────┘           └───────────────────┘
                △                                           │
                │                                           │EV_SEND_MESSAGE
  EV_ON_MESSAGE │                                           │
                │                                           ▽
            ┌───────────────────┐           ┌───────────────────┐
            │                   │           │                   │
            │      IOGate       │           │      IOGate       │
            │                   │           │                   │
            └───────────────────┘           └───────────────────┘
                △                                           │
                │                                           │EV_SEND_MESSAGE
  EV_ON_MESSAGE │                                           │
                │                                           ▽
            ┌───────────────────┐           ┌───────────────────┐
            │                   │           │                   │
            │      Channel      │           │      Channel      │
            │                   │           │                   │
            └───────────────────┘           └───────────────────┘
                △                                           │
                │                                           │EV_SEND_MESSAGE
  EV_ON_MESSAGE │                                           │
                │                                           ▽
            ┌───────────────────┐           ┌───────────────────┐
            │                   │           │                   │
            │      Tube         │           │      Tube         │
            │                   │           │                   │
            └───────────────────┘           └───────────────────┘



From Channel/Tube (gossamer) view:
==================================

TOP Input Events (Requests)
---------------------------

  * EV_SEND_MESSAGE
  * EV_DROP

TOP Output Events (Publications)
---------------------------------

  * EV_ON_OPEN
  * EV_ON_CLOSE
  * EV_ON_MESSAGE
  * EV_ON_ID        same flow as EV_ON_MESSAGE
  * EV_ON_ID_NAK    same flow as EV_ON_MESSAGE


**rst**/

/*********************************************************************
 *      Interface
 *********************************************************************/
/*
 *  Available subscriptions for iogate's users
 */
#define I_IOGATE_SUBSCRIPTIONS  \
    {EV_ON_MESSAGE,       0,  0,  0}, \
    {EV_ON_ID,            0,  0,  0}, \
    {EV_ON_ID_NAK,        0,  0,  0}, \
    {EV_ON_OPEN,          0,  0,  0}, \
    {EV_ON_CLOSE,         0,  0,  0},


/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_IOGATE);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_iogate(void);

#ifdef __cplusplus
}
#endif
