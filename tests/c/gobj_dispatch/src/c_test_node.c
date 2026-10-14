/***********************************************************************
 *          C_TEST_NODE.C
 *
 *          A gobj that test_gobj_dispatch drives from outside:
 *
 *          - EV_TEST_FIRE      publishes EV_TEST_PING
 *          - EV_TEST_PING      counted in "pings" (a witness of a publication)
 *          - EV_TEST_DIE       destroys itself, from its own action
 *          - EV_TEST_SAY_BYE   publishes EV_TEST_BYE, then destroys itself
 *          - EV_TEST_STOP_ME   publishes EV_STOPPED: the farewell of the stop
 *                              contract, after which its host may destroy it
 *          - EV_TEST_MOVE      its FSM moves it to ST_TEST_MOVED before the
 *                              action runs, publishing EV_STATE_CHANGED
 *          - EV_TEST_RELAY     sends EV_TEST_BOUNCE to its "peer", from
 *                              inside its own action
 *          - EV_TEST_BOUNCE    sends EV_TEST_DIE back to the sender
 *
 *          and, by attribute, it ends itself in its own mt_start, mt_stop or
 *          mt_play, as a volatile C_TASK whose first job fails does in its
 *          mt_start.
 *
 *          None of its actions touches the gobj after a publication or a
 *          destroy: the gobj may be gone by then, and what this test checks
 *          is that the KERNEL does not touch it either.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_node.h"

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
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "Subscriber of output-events"),
SDATA (DTP_INTEGER,     "pings",            SDF_RD,             "0",        "EV_TEST_PING received"),
SDATA (DTP_BOOLEAN,     "die_on_start",     SDF_RD,             "0",        "Destroy itself in its mt_start"),
SDATA (DTP_BOOLEAN,     "die_on_stop",      SDF_RD,             "0",        "Destroy itself in its mt_stop"),
SDATA (DTP_BOOLEAN,     "die_on_play",      SDF_RD,             "0",        "Destroy itself in its mt_play, which then fails"),
SDATA (DTP_POINTER,     "peer",             0,                  0,          "Where EV_TEST_RELAY sends EV_TEST_BOUNCE"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
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
    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);
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
    if(gobj_read_bool_attr(gobj, "die_on_start")) {
        gobj_stop(gobj);
        gobj_destroy(gobj);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    if(gobj_read_bool_attr(gobj, "die_on_stop")) {
        gobj_stop_children(gobj);
        gobj_destroy(gobj);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    if(gobj_read_bool_attr(gobj, "die_on_play")) {
        gobj_pause(gobj);
        gobj_stop(gobj);
        gobj_destroy(gobj);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Publish, and touch nothing of yourself afterwards.
 ***************************************************************************/
PRIVATE int ac_fire(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return gobj_publish_event(gobj, EV_TEST_PING, 0);
}

/***************************************************************************
 *  A witness: count what reaches it.
 ***************************************************************************/
PRIVATE int ac_ping(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_write_integer_attr(gobj, "pings", gobj_read_integer_attr(gobj, "pings") + 1);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The self-destroy at the end of the work, from its own action.
 ***************************************************************************/
PRIVATE int ac_die(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    gobj_destroy(gobj);
    return 0;
}

/***************************************************************************
 *  The end of its own publication, then the self-destroy.
 ***************************************************************************/
PRIVATE int ac_say_bye(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    gobj_publish_event(gobj, EV_TEST_BYE, 0);
    gobj_destroy(gobj);
    return 0;
}

/***************************************************************************
 *  The stop contract: EV_STOPPED is the last thing it does with itself.
 ***************************************************************************/
PRIVATE int ac_stop_me(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return gobj_publish_event(gobj, EV_STOPPED, 0);
}

/***************************************************************************
 *  An action that is still running when its gobj is destroyed further in:
 *  it touches nothing of the gobj afterwards.
 ***************************************************************************/
PRIVATE int ac_relay(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return gobj_send_event(gobj_read_pointer_attr(gobj, "peer"), EV_TEST_BOUNCE, 0, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_bounce(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return gobj_send_event(src, EV_TEST_DIE, 0, gobj);
}

/***************************************************************************
 *  Must not run when the change of state destroyed the gobj.
 ***************************************************************************/
PRIVATE int ac_move(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "The action of a gobj destroyed by its change of state ran",
        NULL
    );

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
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_NODE);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DEFINE_STATE(ST_TEST_MOVED);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_TEST_FIRE);
GOBJ_DEFINE_EVENT(EV_TEST_PING);
GOBJ_DEFINE_EVENT(EV_TEST_DIE);
GOBJ_DEFINE_EVENT(EV_TEST_SAY_BYE);
GOBJ_DEFINE_EVENT(EV_TEST_BYE);
GOBJ_DEFINE_EVENT(EV_TEST_STOP_ME);
GOBJ_DEFINE_EVENT(EV_TEST_MOVE);
GOBJ_DEFINE_EVENT(EV_TEST_RELAY);
GOBJ_DEFINE_EVENT(EV_TEST_BOUNCE);

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_TEST_FIRE,              ac_fire,                0},
        {EV_TEST_PING,              ac_ping,                0},
        {EV_TEST_DIE,               ac_die,                 0},
        {EV_TEST_SAY_BYE,           ac_say_bye,             0},
        {EV_TEST_STOP_ME,           ac_stop_me,             0},
        {EV_TEST_MOVE,              ac_move,                ST_TEST_MOVED},
        {EV_TEST_RELAY,             ac_relay,               0},
        {EV_TEST_BOUNCE,            ac_bounce,              0},
        {0,0,0}
    };
    ev_action_t st_test_moved[] = {
        {EV_TEST_DIE,               ac_die,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,       st_idle},
        {ST_TEST_MOVED, st_test_moved},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_TEST_PING,              EVF_OUTPUT_EVENT},
        {EV_TEST_BYE,               EVF_OUTPUT_EVENT},
        {EV_STOPPED,                EVF_OUTPUT_EVENT},

        {EV_TEST_FIRE,              0},
        {EV_TEST_DIE,               0},
        {EV_TEST_SAY_BYE,           0},
        {EV_TEST_STOP_ME,           0},
        {EV_TEST_MOVE,              0},
        {EV_TEST_RELAY,             0},
        {EV_TEST_BOUNCE,            0},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0, // local methods
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        0, // command_table
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_test_node(void)
{
    return create_gclass(C_TEST_NODE);
}
