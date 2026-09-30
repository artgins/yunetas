/***********************************************************************
 *          C_TEST_CC_PEER.C
 *
 *          The far end of the control center's gates, in a test.
 *
 *          One gclass plays three parts:
 *              - a SIDE: the `__top_side__` or `__input_side__` service,
 *                whose children are its channels;
 *              - a CHANNEL of a side: a web client (`opened` says whether
 *                it is connected), or the channel of an agent;
 *              - the TRANSPORT below an agent's C_IEVENT_SRV: what the
 *                control center sends to the agent arrives here as
 *                EV_SEND_MESSAGE, and is decoded.
 *
 *          Everything that reaches it is appended, as {event, kw}, to the
 *          list in its user data `received`, for the test to read.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_cc_peer.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_BOOLEAN,     "opened",           SDF_RD,             "1",        "The client of this channel is connected"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name--------------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *      Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int nothing;
} PRIVATE_DATA;





            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    gobj_write_user_data(gobj, "received", json_array());
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Keep what reached this gobj (event_name, kw owned)
 ***************************************************************************/
PRIVATE void record(hgobj gobj, const char *event_name, json_t *kw)
{
    json_t *received = gobj_read_user_data(gobj, "received");
    json_array_append_new(received, json_pack("{s:s, s:o}",
        "event", event_name,
        "kw", kw
    ));
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  An inter-event for the client of this channel
 ***************************************************************************/
PRIVATE int ac_send_iev(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_event_t iev_event = (gobj_event_t)(uintptr_t)kw_get_int(
        gobj, kw, "__iev_event__", 0, KW_REQUIRED|KW_EXTRACT
    );
    record(gobj, iev_event? iev_event : "", kw);
    return 0;
}

/***************************************************************************
 *  A frame for the agent at the other end: decoded as the agent would
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(
        gobj, kw, "gbuffer", 0, KW_REQUIRED|KW_EXTRACT
    );
    const char *iev_event = 0;
    json_t *iev_kw = gbuf? iev_create_from_gbuffer(gobj, &iev_event, gbuf, 1) : 0;
    if(iev_kw) {
        record(gobj, iev_event? iev_event : "", iev_kw);
    }
    // else Error already logged
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Asked to drop the connection
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    record(gobj, event, kw? kw : json_object());
    return 0;
}

/***************************************************************************
 *  EV_STOPPED
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_CC_PEER);

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_SEND_IEV,               ac_send_iev,        0},
        {EV_SEND_MESSAGE,           ac_send_message,    0},
        {EV_DROP,                   ac_drop,            0},
        {EV_STOPPED,                ac_stopped,         0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_SEND_IEV,               0},
        {EV_SEND_MESSAGE,           0},
        {EV_DROP,                   0},
        {EV_STOPPED,                0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // local methods
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        0,  // command_table
        s_user_trace_level,
        0   // gcflag_t
    );
    return __gclass__ ? 0 : -1;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_test_cc_peer(void)
{
    return create_gclass(C_TEST_CC_PEER);
}
