/***********************************************************************
 *          C_TEST_DISPATCH.C
 *
 *          Test of what the kernel does when a gobj is destroyed while an
 *          event is being dispatched to it, or published by it.
 *
 *          gobj_send_event() and gobj_publish_event() look at their gobj
 *          again when the action, or a subscriber, returns: to print the
 *          return trace, and to stop the publication when the publisher is
 *          gone. Up to 7.26.9 they looked at the gobj's MEMORY for that,
 *          which is freed memory by then. They look at a frame of their
 *          own now, that gobj_destroy() marks.
 *
 *          Phase 1 -- a subscriber destroys the PARENT of the gobj that is
 *          publishing. That is the rule broken: the publisher's tree goes
 *          from under its own publication. It is an ERROR, with a stack,
 *          the destroy still happens, the publication stops, and nothing
 *          reads the freed publisher (run it under ASan to see the last).
 *
 *          Phase 2 -- what is NOT an error, with the machine trace on (the
 *          return trace of gobj_send_event() is what read the freed gobj):
 *            - a gobj that destroys itself in its own action;
 *            - a gobj that publishes its last event, then destroys itself;
 *            - a gobj destroyed by its host in its EV_STOPPED, the stop
 *              contract.
 *
 *          Phase 3 -- a subscriber of EV_STATE_CHANGED destroys the gobj
 *          whose FSM is moving it to the state of an event, before that
 *          event's action. The destroy is the ERROR of phase 1, and the
 *          action is not run on the destroyed gobj (an ERROR too): up to
 *          7.26.9 it ran, on freed memory.
 *
 *          Phase 4 -- a gobj that ends itself in its own mt_start, inside
 *          gobj_start_tree(), in its own mt_stop, inside gobj_stop_tree(),
 *          and in its own mt_play, which then fails. The walks do not go
 *          down into the children of the gobj that is gone, and the failed
 *          play does not write into it (up to 7.26.9 both read or wrote
 *          freed memory). No ERROR: each one is the gobj's own code.
 *
 *          Phase 5 -- a gobj destroyed by an inner dispatch while an outer
 *          one is still running on it. Both are an ERROR, and the outer code
 *          is not read by the kernel:
 *            - a publisher that destroys ITSELF, in an action a subscriber
 *              sent it from inside its publication (its tree is publishing);
 *            - a gobj whose action sends to a peer, which sends it back an
 *              event whose action destroys it (one of its actions is still
 *              running) -- C_TASK's end inside a job's action, up to 7.26.9.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_node.h"
#include "c_test_dispatch.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int run_phase1(hgobj gobj);
PRIVATE int run_phase2(hgobj gobj);
PRIVATE int run_phase3(hgobj gobj);
PRIVATE int run_phase4(hgobj gobj);
PRIVATE int run_phase5(hgobj gobj);
PRIVATE int check_gone(hgobj gobj, const char *name, const char *phase);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "Subscriber of output-events"),
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
    int phase;
    int byes;           // EV_TEST_BYE received in phase 2
    int farewells;      // EV_STOPPED received in phase 2
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
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }
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
    return 0;
}

/***************************************************************************
 *      Framework Method play
 *
 *  The phases run in an action, as any work of a yuno does: there is a
 *  dispatch in progress for this gobj while they run.
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    gobj_post_event(gobj, EV_TEST_RUN, 0, gobj);
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




/***************************************************************************
 *  A gobj that must be gone is not among the children any more.
 ***************************************************************************/
PRIVATE int check_gone(hgobj gobj, const char *name, const char *phase)
{
    if(!gobj_child_by_name(gobj, name)) {
        return 0;
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "A gobj that destroyed itself is still there",
        "phase",        "%s", phase,
        "name",         "%s", name,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  Phase 1: a subscriber destroys the parent of the publisher.
 *
 *      driver
 *        ├── host
 *        │     └── pub     publishes EV_TEST_PING to: driver, witness
 *        └── witness
 *
 *  The driver, first subscriber, destroys `host` (and `pub` with it). The
 *  witness, second subscriber, must not hear the event: its publisher is
 *  gone.
 ***************************************************************************/
PRIVATE int run_phase1(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 1;

    hgobj host = gobj_create("host", C_TEST_NODE, 0, gobj);
    hgobj pub = gobj_create("pub", C_TEST_NODE,
        json_pack("{s:I}", "subscriber", (json_int_t)(uintptr_t)gobj),
        host
    );
    hgobj witness = gobj_create("witness", C_TEST_NODE, 0, gobj);
    if(!host || !pub || !witness) {
        // Error already logged
        return -1;
    }
    gobj_subscribe_event(pub, EV_TEST_PING, 0, witness);

    gobj_send_event(pub, EV_TEST_FIRE, 0, gobj);

    check_gone(gobj, "host", "phase 1");    // Error already logged

    if(gobj_read_integer_attr(witness, "pings") != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A destroyed publisher went on publishing",
            "pings",        "%d", (int)gobj_read_integer_attr(witness, "pings"),
            NULL
        );
    }
    gobj_destroy(witness);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 1 ok",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Phase 2: the destroys that are legal. Not one ERROR may come out of
 *  here, and the machine trace is on for every one of them.
 ***************************************************************************/
PRIVATE int run_phase2(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 2;

    hgclass gclass_node = gclass_find_by_name(C_TEST_NODE);
    gobj_set_gclass_trace(gclass_node, "machine", TRUE);

    /*
     *  Its own action destroys it
     */
    hgobj self = gobj_create("self", C_TEST_NODE, 0, gobj);
    gobj_send_event(self, EV_TEST_DIE, 0, gobj);
    check_gone(gobj, "self", "phase 2");    // Error already logged

    /*
     *  Its last publication, then its own destroy
     */
    hgobj bye = gobj_create("bye", C_TEST_NODE, 0, gobj);
    gobj_send_event(bye, EV_TEST_SAY_BYE, 0, gobj);
    check_gone(gobj, "bye", "phase 2");     // Error already logged

    /*
     *  Its host destroys it in its EV_STOPPED
     */
    hgobj farewell = gobj_create("farewell", C_TEST_NODE, 0, gobj);
    gobj_send_event(farewell, EV_TEST_STOP_ME, 0, gobj);
    check_gone(gobj, "farewell", "phase 2"); // Error already logged

    gobj_set_gclass_trace(gclass_node, "machine", FALSE);

    if(priv->byes != 1 || priv->farewells != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A last publication did not arrive",
            "byes",         "%d", priv->byes,
            "farewells",    "%d", priv->farewells,
            NULL
        );
    }

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 2 ok",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Phase 3: the change of state before an action destroys the gobj.
 ***************************************************************************/
PRIVATE int run_phase3(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 3;

    /*
     *  Its parent hears all it publishes, EV_STATE_CHANGED included
     */
    hgobj mover = gobj_create("mover", C_TEST_NODE, 0, gobj);
    if(!mover) {
        // Error already logged
        return -1;
    }

    gobj_send_event(mover, EV_TEST_MOVE, 0, gobj);
    check_gone(gobj, "mover", "phase 3");   // Error already logged

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 3 ok",
        NULL
    );
    return 0;
}




/***************************************************************************
 *  Phase 4: gobjs that end themselves in their own framework methods.
 *
 *      tree_start              gobj_start_tree()
 *        ├── starter           destroys itself in its mt_start
 *        │     └── grandchild
 *        └── sibling           must be started all the same
 *
 *      tree_stop               gobj_stop_tree()
 *        ├── stopper           destroys itself in its mt_stop
 *        │     └── grandchild
 *        └── sibling           must be stopped all the same
 *
 *      player                  destroys itself in its mt_play, and fails
 ***************************************************************************/
PRIVATE int run_phase4(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 4;

    hgobj tree_start = gobj_create("tree_start", C_TEST_NODE, 0, gobj);
    hgobj starter = gobj_create("starter", C_TEST_NODE,
        json_pack("{s:b}", "die_on_start", 1),
        tree_start
    );
    gobj_create("grandchild", C_TEST_NODE, 0, starter);
    hgobj sibling = gobj_create("sibling", C_TEST_NODE, 0, tree_start);

    gobj_start_tree(tree_start);
    check_gone(tree_start, "starter", "phase 4");     // Error already logged
    if(!gobj_is_running(sibling)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The walk stopped at the gobj that destroyed itself in its mt_start",
            NULL
        );
    }
    gobj_stop_tree(tree_start);
    gobj_destroy(tree_start);

    hgobj tree_stop = gobj_create("tree_stop", C_TEST_NODE, 0, gobj);
    hgobj stopper = gobj_create("stopper", C_TEST_NODE,
        json_pack("{s:b}", "die_on_stop", 1),
        tree_stop
    );
    gobj_create("grandchild", C_TEST_NODE, 0, stopper);
    sibling = gobj_create("sibling", C_TEST_NODE, 0, tree_stop);

    gobj_start_tree(tree_stop);
    gobj_stop_tree(tree_stop);
    check_gone(tree_stop, "stopper", "phase 4");      // Error already logged
    if(gobj_is_running(sibling)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The walk stopped at the gobj that destroyed itself in its mt_stop",
            NULL
        );
    }
    gobj_destroy(tree_stop);

    hgobj player = gobj_create("player", C_TEST_NODE,
        json_pack("{s:b}", "die_on_play", 1),
        gobj
    );
    gobj_start(player);
    gobj_play(player);
    check_gone(gobj, "player", "phase 4");            // Error already logged

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 4 ok",
        NULL
    );
    return 0;
}

/***************************************************************************
 *  Phase 5: an inner dispatch destroys a gobj that an outer one is running
 *  on.
 ***************************************************************************/
PRIVATE int run_phase5(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->phase = 5;

    /*
     *  A subscriber asks the publisher to die, and the publisher's own
     *  action destroys it, inside its publication
     */
    hgobj reentrant = gobj_create("reentrant", C_TEST_NODE, 0, gobj);
    gobj_send_event(reentrant, EV_TEST_FIRE, 0, gobj);
    check_gone(gobj, "reentrant", "phase 5"); // Error already logged

    /*
     *  relay ---EV_TEST_BOUNCE--> peer ---EV_TEST_DIE--> relay
     *  (ac_relay still running)
     */
    hgobj peer = gobj_create("peer", C_TEST_NODE, 0, gobj);
    hgobj relay = gobj_create("relay", C_TEST_NODE,
        json_pack("{s:I}", "peer", (json_int_t)(uintptr_t)peer),
        gobj
    );
    gobj_send_event(relay, EV_TEST_RELAY, 0, gobj);
    check_gone(gobj, "relay", "phase 5");   // Error already logged
    gobj_destroy(peer);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 5 ok",
        NULL
    );
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_run(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)

    run_phase1(gobj);
    run_phase2(gobj);
    run_phase3(gobj);
    run_phase4(gobj);
    run_phase5(gobj);

    set_yuno_must_die();
    return 0;
}

/***************************************************************************
 *  Phase 1: destroy the publisher's parent, from inside its publication.
 *  Phase 5: ask the publisher to destroy itself, from inside it.
 ***************************************************************************/
PRIVATE int ac_ping(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    KW_DECREF(kw)

    if(priv->phase == 1) {
        gobj_destroy(gobj_parent(src));
    } else {
        gobj_send_event(src, EV_TEST_DIE, 0, gobj);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_bye(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->byes++;

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Phase 3: destroy the gobj whose state is changing.
 ***************************************************************************/
PRIVATE int ac_state_changed(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    gobj_destroy(src);
    return 0;
}

/***************************************************************************
 *  The host of the stop contract: destroy it in its EV_STOPPED.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->farewells++;

    KW_DECREF(kw)
    gobj_destroy(src);
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
GOBJ_DEFINE_GCLASS(C_TEST_DISPATCH);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_TEST_RUN);

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
        {EV_TEST_RUN,               ac_run,                 0},
        {EV_TEST_PING,              ac_ping,                0},
        {EV_TEST_BYE,               ac_bye,                 0},
        {EV_STOPPED,                ac_stopped,             0},
        {EV_STATE_CHANGED,          ac_state_changed,       0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_TEST_RUN,               0},
        {EV_TEST_PING,              0},
        {EV_TEST_BYE,               0},
        {EV_STOPPED,                0},
        {EV_STATE_CHANGED,          0},
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
PUBLIC int register_c_test_dispatch(void)
{
    return create_gclass(C_TEST_DISPATCH);
}
