/***********************************************************************
 *          C_TEST_PUBLISH_STATS.C
 *
 *          GClass to test the publish-stats command of C_YUNO
 *
 *          A requester (a service) asks the yuno for the stats of one of its
 *          services every period; the yuno sends them from its periodic
 *          timer as the answers of a stats request (EV_MT_STATS_ANSWER, by
 *          EV_SEND_IEV to the requester), with the routing of the request
 *          reversed. Verified here, the yuno at timeout_periodic 100 ms and
 *          publish_stats_ttl 300 ms (main.c), this gobj the requester:
 *
 *              A. ttl=450: a few publications, then it ends ("publish-stats
 *                 ended", expired); each one an EV_MT_STATS_ANSWER with the
 *                 stats, the metadata of the request and its route reversed;
 *              C. no ttl: the attribute publish_stats_ttl ends it;
 *              B. ttl=-1: it does not end on its own; stop=1 ends it;
 *              D. a service that does not exist is refused;
 *              E. a requester that is not a service is refused.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>

#include "c_test_publish_stats.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define PERIOD  100     // ms, the yuno's timeout_periodic

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level of help"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-------json_fn---------description--*/
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,    cmd_help,       "Command's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
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
    hgobj timer;
    hgobj child;            // a requester that is not a service
    int phase;
    int received;           // publications of the current phase
    int bad;                // publications that are not what they must be
    int result;
} PRIVATE_DATA;

/***************************************************************************
 *              Framework Method Create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    priv->child = gobj_create_pure_child("not_a_service", C_TIMER, 0, gobj);
}

/***************************************************************************
 *              Framework Method Start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);

    return 0;
}

/***************************************************************************
 *              Framework Method Stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);

    return 0;
}

/***************************************************************************
 *              Framework Method Play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, PERIOD);

    return 0;
}

/***************************************************************************
 *  A test failure, logged. Returns -1.
 ***************************************************************************/
PRIVATE int fail(hgobj gobj, const char *what, int value)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST FAIL",
        "what",         "%s", what,
        "value",        "%d", value,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  publish-stats to the yuno, as a peer asks for it: with a route (the
 *  request's ievent stack) and a mark of its own in __md_iev__. Returns
 *  the result of the answer.
 ***************************************************************************/
PRIVATE int ask(hgobj gobj, hgobj requester, const char *stats_service, json_int_t ttl, BOOL with_ttl, BOOL stop)
{
    json_t *kw = json_pack("{s:s, s:I, s:b}",
        "stats_service", stats_service,
        "period", (json_int_t)PERIOD,
        "stop", stop
    );
    if(with_ttl) {
        json_object_set_new(kw, "ttl", json_integer(ttl));
    }
    kw_set_subdict_value(gobj, kw, "__md_iev__", "test_mark", json_integer(7));
    kw_set_subdict_value(gobj, kw, "__md_iev__", IEVENT_STACK_ID,
        json_pack("[{s:s, s:s, s:s, s:s, s:s, s:s}]",
            "dst_yuno", "",
            "dst_role", gobj_yuno_role(),
            "dst_service", "__yuno__",
            "src_yuno", "",
            "src_role", "a_peer",
            "src_service", gobj_name(gobj)
        )
    );
    json_t *jn_response = gobj_command(gobj_yuno(), "publish-stats", kw, requester);
    int result = (int)kw_get_int(gobj, jn_response, "result", -1, 0);
    JSON_DECREF(jn_response)
    return result;
}

/***************************************************************************
 *  EV_TIMEOUT -- the phases
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase++) {
        case 0:
            /*  A: its own ttl  */
            if(ask(gobj, gobj, "__yuno__", 450, TRUE, FALSE) != 0) {
                priv->result += fail(gobj, "A: publish-stats refused", 0);
            }
            priv->received = 0;
            set_timeout(priv->timer, 900);
            break;

        case 1:
            if(priv->received < 3 || priv->received > 6) {
                priv->result += fail(gobj, "A: publications in a ttl of 450 ms", priv->received);
            }
            /*  C: the ttl of the attribute (300 ms)  */
            if(ask(gobj, gobj, "__yuno__", 0, FALSE, FALSE) != 0) {
                priv->result += fail(gobj, "C: publish-stats refused", 0);
            }
            priv->received = 0;
            set_timeout(priv->timer, 700);
            break;

        case 2:
            if(priv->received < 1 || priv->received > 4) {
                priv->result += fail(gobj, "C: publications in publish_stats_ttl (300 ms)", priv->received);
            }
            /*  B: no automatic end  */
            if(ask(gobj, gobj, "__yuno__", -1, TRUE, FALSE) != 0) {
                priv->result += fail(gobj, "B: publish-stats refused", 0);
            }
            priv->received = 0;
            set_timeout(priv->timer, 900);
            break;

        case 3:
            if(priv->received < 6) {
                priv->result += fail(gobj, "B: publications with ttl -1", priv->received);
            }
            if(ask(gobj, gobj, "__yuno__", 0, FALSE, TRUE) != 0) {
                priv->result += fail(gobj, "B: stop refused", 0);
            }
            priv->received = 0;
            set_timeout(priv->timer, 400);
            break;

        case 4:
            if(priv->received != 0) {
                priv->result += fail(gobj, "B: publications after stop=1", priv->received);
            }
            /*  D and E  */
            if(ask(gobj, gobj, "no_such_service", 0, FALSE, FALSE) == 0) {
                priv->result += fail(gobj, "D: a service that does not exist was taken", 0);
            }
            if(ask(gobj, priv->child, "__yuno__", 0, FALSE, FALSE) == 0) {
                priv->result += fail(gobj, "E: a requester that is not a service was taken", 0);
            }
            if(priv->bad) {
                priv->result += fail(gobj, "publications that are not what they must be", priv->bad);
            }
            if(priv->result == 0) {
                gobj_log_info(gobj, 0,
                    "msgset", "%s", MSGSET_INFO,
                    "msg", "%s", "All publish-stats tests PASSED",
                    NULL
                );
            }
            gobj_log_info(gobj, 0,
                "msgset", "%s", MSGSET_INFO,
                "msg", "%s", "Exit to die",
                NULL
            );
            set_yuno_must_die();
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_SEND_IEV -- a publication: the answer of a stats request, with the
 *  stats of the yuno, the mark of the request, and its route reversed
 ***************************************************************************/
PRIVATE int ac_send_iev(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->received++;

    gobj_event_t iev_event = (gobj_event_t)(uintptr_t)kw_get_int(gobj, kw, "__iev_event__", 0, 0);
    json_t *jn_data = kw_get_dict_value(gobj, kw, "data", 0, 0);
    json_int_t mark = kw_get_int(gobj, kw, "__md_iev__`test_mark", 0, 0);
    json_t *jn_stack = kw_get_list(gobj, kw, "__md_iev__`" IEVENT_STACK_ID, 0, 0);
    json_t *jn_top = json_array_get(jn_stack, 0);
    const char *dst_service = kw_get_str(gobj, jn_top, "dst_service", "", 0);

    if(iev_event != EV_MT_STATS_ANSWER || !jn_data || mark != 7 ||
            strcmp(dst_service, gobj_name(gobj))!=0 ||
            kw_get_int(gobj, kw, "result", -1, 0) != 0) {
        priv->bad++;
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a publication",
            "event",        "%s", iev_event? iev_event : "(none)",
            "mark",         "%d", (int)mark,
            "dst_service",  "%s", dst_service,
            NULL
        );
    }

    KW_DECREF(kw)
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
 *              Command: help
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw)
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *              GClass
 ***************************************************************************/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
};

GOBJ_DEFINE_GCLASS(C_TEST_PUBLISH_STATS);

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

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,         0},
        {EV_SEND_IEV,               ac_send_iev,        0},
        {EV_STOPPED,                ac_stopped,         0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_SEND_IEV,               0},
        {EV_STOPPED,                0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  //lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    return __gclass__ ? 0 : -1;
}

/***************************************************************************
 *              Registration
 ***************************************************************************/
PUBLIC int register_c_test_publish_stats(void)
{
    return create_gclass(C_TEST_PUBLISH_STATS);
}
