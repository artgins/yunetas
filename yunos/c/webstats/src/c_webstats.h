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
GOBJ_DECLARE_STATE(ST_LOOKING_UP);      // asking the registries about the top clients
GOBJ_DECLARE_STATE(ST_REPORTING);       // building the report and sending it

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_REPORT_READY);    // the daily record, for whoever wants it
GOBJ_DECLARE_EVENT(EV_NEXT_FILE);       // internal: take the next file, next cycle
GOBJ_DECLARE_EVENT(EV_NEXT_LOOKUP);     // internal: look up the next client, next cycle
GOBJ_DECLARE_EVENT(EV_LOOKUP_DONE);     // internal: stop the client of a lookup, out of its stack

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_webstats(void);

#ifdef __cplusplus
}
#endif
