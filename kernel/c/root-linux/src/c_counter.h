/****************************************************************************
 *          C_COUNTER.H
 *          Counter GClass.
 *          Copyright (c) 2014 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/

/*^^ ======================== start sphinx doc ================================

Counter GClass
==============

    Counter of events, filtered by attr's regex or acceptance callback.
    When counter reachs his count then a configured event is published.
    Counter accepts subscriber.

    WARNING: this gobj is auto-destroyed when reached his final count.


Diagram
=======

    Input Events            Counter             Output Events
                    ┌───────────────────┐
        start   ━━━▷│●                  │
                    │-------------------│
                    │                   │
      EV_COUNT ====▷│                   │====▷  EV_FINAL_COUNT
                    │                   │
                    │-------------------│
        stop    ━━━▷│■  ◁---(auto)      │
                    │                   │
                    └───────────────────┘



{
    "info": "xxxx",
    "user_data": 7105288,
    "user_data2": 7076024,
    "final_count": 1,
    "expiration_timeout": 4000,
    "input_schema": [
        {
            "event": "EV_ON_CLOSE_ROUTE",
            "filters": {
                "__route__": 7104008
            }
        }
    ]
}

Attributes
==========

final_count
-----------

    Type: ASN_UINT32

    Final count that activates publishing output event.

input_schema
------------

    Type: ASN_JSON, EvChkItem

    input_schema is a schema to validate and count the input events.
    input_schema is list of EvChkItem json objects.

    EvChkItem: a json **object** with this schema:
        [{
            'event': str,
            'filters': {
                field: str (regex),
                field: int ,
            }
        }]

    Regex filters only can be applied in string attributes.
    For the remain of field type you can use an acceptance callback.


expiration_timeout
------------------

    Timeout in miliseconds to finish the counter
    although the final count hasn't been reached.


Input Events
============

EV_COUNT
--------

    Fields:



Output Events
=============

EV_FINAL_COUNT
--------------

    Fields:

        * info: str, User info.
        * jn_info: json, User json info.
        * max_count: int, Desired final count.
        * cur_count: int, count reached.


============================= end sphinx doc ============================= ^^*/
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
GOBJ_DECLARE_GCLASS(C_COUNTER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_COUNT);
GOBJ_DECLARE_EVENT(EV_FINAL_COUNT);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_counter(void);

#ifdef __cplusplus
}
#endif
