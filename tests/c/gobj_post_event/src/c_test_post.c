/***********************************************************************
 *          C_TEST_POST.C
 *
 *          Test of gobj_post_event(): an event sent to a gobj and
 *          delivered on the next cycle of the event loop.
 *
 *          The test runs as four phases chained one after another, each
 *          one checking a clause of the contract written in gobj.h:
 *
 *      1) Posting does NOT deliver. Three messages posted from mt_play()
 *         are still queued when mt_play() returns, they arrive in the
 *         order they were posted, each one with its own kw, and with the
 *         gobj itself as source. A fourth posted from the first action
 *         arrives after the three, not inside them.
 *
 *      2) A chain of posted events does not starve the event loop. The
 *         gobj posts itself for 200 milliseconds with a 1 millisecond
 *         periodic timer armed: if delivery drained the queue until empty
 *         instead of taking a snapshot per cycle, not one timeout would
 *         arrive in that window.
 *
 *      3) Destroying a gobj drops what it left posted, and a gobj that is
 *         being destroyed cannot post any more.
 *
 *      4) The queue has a ceiling, and reaching it is an error, not a
 *         yuno that grows until the node dies.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_post.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CHAIN_MS            200     // how long phase 2 keeps posting
#define CHAIN_PERIODIC_MS   1       // period of the timer that must be heard
#define MIN_PERIODICS       10      // starving the loop would give exactly 0
#define CAP_TRIES           50000   // more than any sane ceiling

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int start_phase2(hgobj gobj);
PRIVATE int run_phase3(hgobj gobj);
PRIVATE int run_phase4(hgobj gobj);
PRIVATE int check_source(hgobj gobj, gobj_event_t event, hgobj src);

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
    hgobj timer;                    // the periodic that phase 2 must keep hearing

    char order[8];                  // the letters of phase 1, in arrival order
    int order_n;

    uint64_t t_chain;               // phase 2 stops when this expires
    int ticks;                      // messages of the chain
    int periodics;                  // timeouts heard while the chain ran

    int cap_posted;                 // messages accepted before the ceiling
    int nops;                       // messages of the ceiling delivered

    BOOL post_on_destroy;           // only the child armed in phase 3 does it
    BOOL x_from_dead_child;         // the parent got what the dead child posted here
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);

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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->post_on_destroy) {
        /*
         *  Phase 3: a gobj under destruction must NOT be able to queue an
         *  event for a cycle it will not see. It is refused with an error,
         *  and that error is part of what this test pins down.
         */
        gobj_post_event(gobj, EV_TEST_X, 0, gobj);
    }
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout0(priv->timer);
    gobj_stop(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 *
 *  Phase 1 starts here, BEFORE the event loop is running. An event posted
 *  now must survive until the first cycle: that is why the loop delivers at
 *  the top of the cycle and not after the completions.
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    gobj_post_event(gobj, EV_TEST_A, json_pack("{s:i}", "n", 1), gobj);
    gobj_post_event(gobj, EV_TEST_B, json_pack("{s:i}", "n", 2), gobj);
    gobj_post_event(gobj, EV_TEST_C, json_pack("{s:i}", "n", 3), gobj);

    if(gobj_posted_events_size() != 3) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A posted event was delivered inline",
            "pending",      "%d", (int)gobj_posted_events_size(),
            NULL
        );
    }

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout0(priv->timer);

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  A posted event arrives with the gobj itself as source. Self-send is
 *  the whole contract: anything else would mean the queue is holding a
 *  pointer to a gobj that is not the one whose destruction purges it.
 ***************************************************************************/
PRIVATE int check_source(hgobj gobj, gobj_event_t event, hgobj src)
{
    if(src == gobj) {
        return 0;
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "A posted event did not arrive from itself",
        "event",        "%s", event,
        "src",          "%s", src?gobj_short_name(src):"(null)",
        NULL
    );
    return -1;
}

/***************************************************************************
 *  Record the letter of a phase 1 message and check the kw travelled whole.
 ***************************************************************************/
PRIVATE int note_arrival(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src, char letter, int n)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    check_source(gobj, event, src);     // Error already logged

    if(kw_get_int(gobj, kw, "n", 0, 0) != n) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The kw of a posted event did not arrive",
            "event",        "%s", event,
            "n",            "%d", (int)kw_get_int(gobj, kw, "n", 0, 0),
            "expected",     "%d", n,
            NULL
        );
    }

    if(priv->order_n < (int)sizeof(priv->order)-1) {
        priv->order[priv->order_n++] = letter;
    }

    return 0;
}

/***************************************************************************
 *  Phase 2: post to yourself without pause, with a periodic timer armed.
 ***************************************************************************/
PRIVATE int start_phase2(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->periodics = 0;
    priv->ticks = 0;
    priv->t_chain = start_msectimer(CHAIN_MS);

    set_timeout_periodic0(priv->timer, CHAIN_PERIODIC_MS);

    gobj_change_state(gobj, ST_CHAIN);

    return gobj_post_event(gobj, EV_TEST_TICK, 0, gobj);
}

/***************************************************************************
 *  Phase 3: what a destroyed gobj left posted must go with it.
 ***************************************************************************/
PRIVATE int run_phase3(hgobj gobj)
{
    size_t before = gobj_posted_events_size();

    hgobj child = gobj_create("child", C_TEST_POST, 0, gobj);
    if(!child) {
        // Error already logged
        return -1;
    }

    gobj_send_event(child, EV_TEST_ARM, 0, gobj);

    /*
     *  Two: one to itself, one to this gobj.
     */
    if(gobj_posted_events_size() != before + 2) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The child did not leave its event posted",
            "pending",      "%d", (int)gobj_posted_events_size(),
            NULL
        );
    }

    /*
     *  Its mt_destroy() tries to post one more, which must be refused.
     */
    gobj_destroy(child);

    /*
     *  One of the two is gone with it -- the one it had posted to itself --
     *  and the one it left for this gobj is still there, waiting.
     */
    if(gobj_posted_events_size() != before + 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The purge of a destroyed gobj took the wrong events",
            "pending",      "%d", (int)gobj_posted_events_size(),
            NULL
        );
    }

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 3 ok",
        NULL
    );

    return run_phase4(gobj);
}

/***************************************************************************
 *  Phase 4: the ceiling. Post until it is refused.
 ***************************************************************************/
PRIVATE int run_phase4(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_change_state(gobj, ST_CEILING);

    priv->cap_posted = 0;
    priv->nops = 0;

    while(priv->cap_posted < CAP_TRIES) {
        if(gobj_post_event(gobj, EV_TEST_NOP, 0, gobj) < 0) {
            // Error already logged: that refusal IS the ceiling
            break;
        }
        priv->cap_posted++;
    }

    if(priv->cap_posted >= CAP_TRIES) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The posted queue has no ceiling",
            "posted",       "%d", priv->cap_posted,
            NULL
        );
    }

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 4 ok",
        NULL
    );

    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Phase 1, first message: it also posts the fourth, which must NOT jump
 *  ahead of the two already queued.
 ***************************************************************************/
PRIVATE int ac_test_a(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    note_arrival(gobj, event, kw, src, 'A', 1);

    gobj_post_event(gobj, EV_TEST_D, json_pack("{s:i}", "n", 4), gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_test_b(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    note_arrival(gobj, event, kw, src, 'B', 2);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_test_c(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    note_arrival(gobj, event, kw, src, 'C', 3);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Phase 1 is over: check the order and go on.
 ***************************************************************************/
PRIVATE int ac_test_d(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    note_arrival(gobj, event, kw, src, 'D', 4);

    if(strcmp(priv->order, "ABCD") != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Posted events did not arrive in order",
            "order",        "%s", priv->order,
            NULL
        );
    }

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 1 ok",
        NULL
    );

    start_phase2(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Phase 2: one link of the chain.
 ***************************************************************************/
PRIVATE int ac_test_tick(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->ticks++;

    if(!test_msectimer(priv->t_chain)) {
        gobj_post_event(gobj, EV_TEST_TICK, 0, gobj);
        KW_DECREF(kw)
        return 0;
    }

    clear_timeout0(priv->timer);

    /*
     *  The chain held the loop for CHAIN_MS with a timer of 1 ms armed.
     *  Delivering until the queue is empty, instead of a snapshot per
     *  cycle, would have kept the loop inside the queue the whole time
     *  and not a single completion would have been seen.
     */
    if(priv->periodics < MIN_PERIODICS) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A chain of posted events starved the event loop",
            "periodics",    "%d", priv->periodics,
            "ticks",        "%d", priv->ticks,
            NULL
        );
    }

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 2 ok",
        NULL
    );

    run_phase3(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Phase 3, in the child: leave an event posted and let itself be killed.
 ***************************************************************************/
PRIVATE int ac_test_arm(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->post_on_destroy = TRUE;

    /*
     *  To ITSELF, with a kw that allocates: this one is dropped when the
     *  child is destroyed, and if the purge did not decref the kw the memory
     *  check at the end of the test says so.
     */
    gobj_post_event(gobj, EV_TEST_X, json_pack("{s:s}",
        "payload", "this event is never delivered"
    ), gobj);

    /*
     *  To its PARENT, with itself as source. The child is destroyed before
     *  the delivery, so this one must still arrive -- the destination still
     *  wants its event -- and it must arrive with src cleared.
     */
    gobj_post_event(gobj_parent(gobj), EV_TEST_X, 0, gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Phase 3, in the parent: what the child posted here before it died.
 *
 *  The child's own copy, posted to itself, is dropped with it and arrives
 *  nowhere. This one does arrive, because the destination is alive and still
 *  wants it -- with src cleared, because the source is gone.
 ***************************************************************************/
PRIVATE int ac_test_x(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src != NULL) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A destroyed source did not clear src",
            NULL
        );
    }
    priv->x_from_dead_child = TRUE;

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Phase 4: the queued messages of the ceiling arrive, all in one cycle.
 ***************************************************************************/
PRIVATE int ac_test_nop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->nops++;

    if(priv->nops == priv->cap_posted) {
        if(!priv->x_from_dead_child) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The event posted by the dead child never arrived",
                NULL
            );
        }
        set_yuno_must_die();
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  What phase 2 has to keep hearing.
 ***************************************************************************/
PRIVATE int ac_timeout_periodic(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->periodics++;

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A timer was cancelled. Part of the C_TIMER0 contract: it tells its owner
 *  when the timeout it was holding will not arrive.
 ***************************************************************************/
PRIVATE int ac_timer_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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
GOBJ_DEFINE_GCLASS(C_TEST_POST);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DEFINE_STATE(ST_CHAIN);
GOBJ_DEFINE_STATE(ST_CEILING);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_TEST_A);
GOBJ_DEFINE_EVENT(EV_TEST_B);
GOBJ_DEFINE_EVENT(EV_TEST_C);
GOBJ_DEFINE_EVENT(EV_TEST_D);
GOBJ_DEFINE_EVENT(EV_TEST_TICK);
GOBJ_DEFINE_EVENT(EV_TEST_ARM);
GOBJ_DEFINE_EVENT(EV_TEST_X);
GOBJ_DEFINE_EVENT(EV_TEST_NOP);

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
        {EV_TEST_A,                 ac_test_a,              0},
        {EV_TEST_B,                 ac_test_b,              0},
        {EV_TEST_C,                 ac_test_c,              0},
        {EV_TEST_D,                 ac_test_d,              0},
        {EV_TEST_ARM,               ac_test_arm,            0},
        {EV_TEST_X,                 ac_test_x,              0},
        {EV_TIMEOUT_PERIODIC,       ac_timeout_periodic,    0},
        {EV_STOPPED,                ac_timer_stopped,       0},
        {0,0,0}
    };
    ev_action_t st_chain[] = {
        {EV_TEST_TICK,              ac_test_tick,           0},
        {EV_TIMEOUT_PERIODIC,       ac_timeout_periodic,    0},
        {EV_STOPPED,                ac_timer_stopped,       0},
        {0,0,0}
    };
    ev_action_t st_ceiling[] = {
        {EV_TEST_NOP,               ac_test_nop,            0},
        {EV_TEST_X,                 ac_test_x,              0},
        {EV_TIMEOUT_PERIODIC,       ac_timeout_periodic,    0},
        {EV_STOPPED,                ac_timer_stopped,       0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,       st_idle},
        {ST_CHAIN,      st_chain},
        {ST_CEILING,    st_ceiling},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_TEST_A,                 0},
        {EV_TEST_B,                 0},
        {EV_TEST_C,                 0},
        {EV_TEST_D,                 0},
        {EV_TEST_TICK,              0},
        {EV_TEST_ARM,               0},
        {EV_TEST_X,                 0},
        {EV_TEST_NOP,               0},
        {EV_TIMEOUT_PERIODIC,       0},
        {EV_STOPPED,                0},
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
PUBLIC int register_c_test_post(void)
{
    return create_gclass(C_TEST_POST);
}
