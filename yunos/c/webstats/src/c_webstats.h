/****************************************************************************
 *          c_webstats.h
 *          Webstats GClass.
 *
 *          Daily report of the node's web server logs
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
GOBJ_DECLARE_GCLASS(C_WEBSTATS);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DECLARE_STATE(ST_READING);         // a reader is feeding lines
GOBJ_DECLARE_STATE(ST_REPORTING);       // building the report and sending it

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_REPORT_READY);    // the daily record, for whoever wants it

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_webstats(void);

#ifdef __cplusplus
}
#endif
