/***********************************************************************
 *          C_TEST_QIOGATE_STATS.C
 *
 *          GClass to test the queue gauges of C_QIOGATE
 *
 *          C_QIOGATE said `msgs_in_queue` and `pending_acks` only from its
 *          own mt_stats, and a C_MQIOGATE asks each child with
 *          build_stats(), which reads the SDF_STATS attrs and never calls
 *          the child's mt_stats: through `stats-yuno service=__output_side__`
 *          the size of a persistent link's queue could not be read. They
 *          are SDF_STATS gauges now, read live (mt_reading).
 *
 *          Tasks
 *          - Start __output_side__: a C_MQIOGATE (broadcast) with two
 *            C_QIOGATE, whose peer does not listen
 *          - Send N_MSGS messages to the C_MQIOGATE: N_MSGS in each queue
 *          - The stats of the C_MQIOGATE say msgs_in_queue N_MSGS for each
 *            child, and pending_acks 0; so does each child's attr
 *          - The stats of a C_QIOGATE, and of its C_IOGATE, asked directly
 *            (what `stats-yuno service=<it>` does) are the response
 *            envelope, result 0 and the data in `data`: up to 7.25.21 both
 *            answered the bare data, and stats-yuno said -1
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_qiogate_stats.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define N_MSGS      5

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void check(hgobj gobj, const char *what, BOOL ok);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
int test_qiogate_stats_failed = 0;

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
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj gobj_output_side;
    int phase;
} PRIVATE_DATA;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_output_side);
    set_timeout(priv->timer, 300);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void check(hgobj gobj, const char *what, BOOL ok)
{
    if(!ok) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL",
            "what",         "%s", what,
            NULL
        );
        test_qiogate_stats_failed++;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The phases: send the messages, then read the stats
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->phase++) {
        case 0:
            for(int i = 0; i < N_MSGS; i++) {
                gobj_send_event(
                    priv->gobj_output_side,
                    EV_SEND_MESSAGE,
                    json_pack("{s:s, s:i}", "id", "1", "n", i),
                    gobj
                );
            }
            set_timeout(priv->timer, 300);
            break;

        default:
            {
                json_t *jn_stats = gobj_stats(priv->gobj_output_side, "", 0, gobj);
                json_t *jn_data = kw_get_dict(gobj, jn_stats, "data", 0, 0);
                int children = 0;
                const char *child_name; json_t *jn_child;
                json_object_foreach(jn_data, child_name, jn_child) {
                    children++;
                    check(gobj, "the C_MQIOGATE stats say msgs_in_queue of each child",
                        json_integer_value(json_object_get(jn_child, "msgs_in_queue")) == N_MSGS
                    );
                    check(gobj, "and its pending_acks",
                        json_object_get(jn_child, "pending_acks") &&
                        json_integer_value(json_object_get(jn_child, "pending_acks")) == 0
                    );
                    hgobj child = gobj_find_service(child_name, FALSE);
                    check(gobj, "the child's attr msgs_in_queue",
                        child && gobj_read_integer_attr(child, "msgs_in_queue") == N_MSGS
                    );
                }
                check(gobj, "two C_QIOGATE children in the stats", children == 2);

                json_t *jn_q = gobj_stats(gobj_find_service("output-q1", TRUE), "", 0, gobj);
                check(gobj, "the stats of a C_QIOGATE are the envelope: result 0",
                    json_object_get(jn_q, "result") && kw_get_int(gobj, jn_q, "result", -1, 0) == 0
                );
                check(gobj, "and their data say msgs_in_queue",
                    kw_get_int(gobj, jn_q, "data`msgs_in_queue", -1, 0) == N_MSGS
                );
                check(gobj, "and the data of its C_IOGATE below",
                    json_object_get(kw_get_dict(gobj, jn_q, "data", 0, 0), "txMsgs") != NULL
                );
                JSON_DECREF(jn_q)

                json_t *jn_io = gobj_stats(gobj_find_service("output-iogate-q1", TRUE), "", 0, gobj);
                check(gobj, "the stats of a C_IOGATE are the envelope: result 0",
                    json_object_get(jn_io, "result") && kw_get_int(gobj, jn_io, "result", -1, 0) == 0
                );
                check(gobj, "and their data say txMsgs",
                    json_object_get(kw_get_dict(gobj, jn_io, "data", 0, 0), "txMsgs") != NULL
                );
                JSON_DECREF(jn_io)

                /*
                 *  view-channels of the C_MQIOGATE asks each child with the
                 *  same kw: kw_incref(), so a kw carrying a gbuffer keeps
                 *  one reference of it per child (with json_incref, up to
                 *  7.25.4, the second child's answer dropped it twice: "BAD
                 *  gbuf_decref()")
                 */
                gbuffer_t *gbuf = gbuffer_create(32, 32);
                gbuffer_append_string(gbuf, "carried along");
                json_t *jn_vc = gobj_command(priv->gobj_output_side, "view-channels",
                    json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf), gobj);
                check(gobj, "view-channels of the C_MQIOGATE, a kw with a gbuffer",
                    kw_get_int(gobj, jn_vc, "result", -1, 0) >= 0
                );
                JSON_DECREF(jn_vc)

                if(test_qiogate_stats_failed) {
                    gobj_trace_json(gobj, jn_stats, "the stats of __output_side__");
                }
                JSON_DECREF(jn_stats)

                gobj_stop_tree(priv->gobj_output_side);
                set_yuno_must_die();
            }
            break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The output side's own events: the peer does not listen, nothing opens
 ***************************************************************************/
PRIVATE int ac_on_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_QIOGATE_STATS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
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

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_ON_OPEN,                ac_on_event,                0},
        {EV_ON_CLOSE,               ac_on_event,                0},
        {EV_ON_MESSAGE,             ac_on_event,                0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_ON_MESSAGE,             0},
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
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test_qiogate_stats(void)
{
    return create_gclass(C_TEST_QIOGATE_STATS);
}
