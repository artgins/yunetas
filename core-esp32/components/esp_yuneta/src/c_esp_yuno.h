/****************************************************************************
 *          c_esp_yuno.h
 *
 *          GClass __yuno__
 *          Low level esp-idf
 *
 *          Copyright (c) 2023 Niyamaka.
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
GOBJ_DECLARE_GCLASS(GC_YUNO);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DECLARE_STATE(ST_YUNO_NETWORK_OFF);
GOBJ_DECLARE_STATE(ST_YUNO_NETWORK_ON);
GOBJ_DECLARE_STATE(ST_YUNO_TIME_ON);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_YUNO_TIME_ON);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_esp_yuno(void);

PUBLIC int gobj_post_event(
    hgobj dst,
    gobj_event_t event,
    json_t *kw,  // owned
    hgobj src
);

#ifdef __cplusplus
}
#endif
