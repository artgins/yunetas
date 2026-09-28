/***********************************************************************
 *          C_TEST_QIOGATE.C
 *
 *          Test of C_QIOGATE ignoring repeated messages.
 *
 *          A queue link delivers at least once: a lost ack makes the
 *          sender resend, and a gate that forwards through a C_QIOGATE
 *          queues the message twice. With `repeated_key` set, C_QIOGATE
 *          does not queue a message whose `repeated_field` is not above
 *          (or, in mode "equal", is the same as) the last one it passed
 *          for its key. The phases, run one after another on the first
 *          cycle of the loop, each with its own queue:
 *
 *      1) Default configuration: every message is queued, repeated or
 *         not, and the stats carry nothing new. The current behaviour.
 *
 *      2) repeated_key 'id', repeated_field 'seq' (mode not_newer): a
 *         repeat and an older one are not queued, a message without the
 *         field is queued and counted as unchecked, a numeric key works.
 *
 *      3) The same gate stopped and started: the last value of each key
 *         is rebuilt from the queue, so the first repeat after a restart,
 *         which is when upstream resends, is not queued either.
 *
 *      4) Mode equal on a path ('data`tm'): only the same value is a
 *         repeat, an older one passes.
 *
 *      5) A wrong repeated_mode is an error at create and leaves the
 *         default behaviour.
 *
 *      6) What the filter costs: the same messages through a gate
 *         without it and a gate with it, timed and logged, not checked.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <time.h>

#include "c_test_qiogate.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#ifndef PERF_MSGS
#define PERF_MSGS       50000   // -DPERF_MSGS=N for a longer measure
#endif
#define PERF_KEYS       1000

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
    hgobj bottom;                   // the bottom of the gates: never opened
    char path[PATH_MAX];            // where the queues are
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

    priv->bottom = gobj_create_pure_child("bottom", C_TIMER0, 0, gobj);

    build_path(priv->path, sizeof(priv->path), getenv("HOME"), "tests_yuneta", "c_qiogate_repeated", NULL);
    rmrdir(priv->path);

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
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  A gate with its own queue, and whatever configuration the phase adds
 ***************************************************************************/
PRIVATE hgobj create_gate(hgobj gobj, const char *name, json_t *kw_extra)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = json_pack("{s:s, s:s, s:s, s:b}",
        "tranger_path", priv->path,
        "tranger_database", name,
        "topic_name", "msgs",
        "disable_alert", 1
    );
    if(kw_extra) {
        json_object_update_new(kw, kw_extra);
    }
    hgobj gate = gobj_create(name, C_QIOGATE, kw, gobj);
    gobj_set_bottom_gobj(gate, priv->bottom);
    gobj_start(gate);
    return gate;
}

PRIVATE void destroy_gate(hgobj gate)
{
    gobj_stop(gate);
    gobj_destroy(gate);
}

/***************************************************************************
 *  Messages of a phase, json [[key, seq], ...]; a seq of -1 means no seq
 ***************************************************************************/
PRIVATE void send_seqs(hgobj gobj, hgobj gate, const char *spec)
{
    json_t *list = legalstring2json(spec, TRUE);
    size_t idx;
    json_t *pair;
    json_array_foreach(list, idx, pair) {
        json_t *kw = json_object();
        json_object_set(kw, "id", json_array_get(pair, 0));
        json_int_t seq = json_integer_value(json_array_get(pair, 1));
        if(seq >= 0) {
            json_object_set_new(kw, "seq", json_integer(seq));
        }
        json_object_set_new(kw, "value", json_real(1.5));
        gobj_send_event(gate, EV_SEND_MESSAGE, kw, gobj);
    }
    JSON_DECREF(list)
}

/***************************************************************************
 *  A stat of the gate, or -1 when the gate does not give it
 ***************************************************************************/
PRIVATE json_int_t gate_stat(hgobj gobj, hgobj gate, const char *stat)
{
    json_t *stats = gobj_stats(gate, 0, 0, gobj);
    json_t *v = json_object_get(stats, stat);
    json_int_t n = v? json_integer_value(v) : -1;
    JSON_DECREF(stats)
    return n;
}

PRIVATE int expect_stat(hgobj gobj, hgobj gate, const char *phase, const char *stat, json_int_t expected)
{
    json_int_t got = gate_stat(gobj, gate, stat);
    if(got != expected) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Unexpected stat of the gate",
            "phase",        "%s", phase,
            "stat",         "%s", stat,
            "got",          "%ld", (long)got,
            "expected",     "%ld", (long)expected,
            NULL
        );
        return -1;
    }
    return 0;
}

PRIVATE void phase_ok(hgobj gobj, const char *msg)
{
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", msg,
        NULL
    );
}

/***************************************************************************
 *  Time PERF_MSGS messages through a gate, in ms
 ***************************************************************************/
PRIVATE double time_messages(hgobj gobj, hgobj gate)
{
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    char key[16];
    for(int i = 0; i < PERF_MSGS; i++) {
        snprintf(key, sizeof(key), "K%03d", i % PERF_KEYS);
        json_t *kw = json_pack("{s:s, s:I, s:f}",
            "id", key,
            "seq", (json_int_t)(i / PERF_KEYS + 1),
            "value", 1.5
        );
        gobj_send_event(gate, EV_SEND_MESSAGE, kw, gobj);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (double)(t1.tv_sec - t0.tv_sec) * 1000.0 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The phases
 ***************************************************************************/
PRIVATE int ac_test_run(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    const char *messages = "[[\"A\",1],[\"A\",2],[\"A\",2],[\"A\",1],[\"B\",1],[\"A\",3],[\"C\",-1],[7,1],[7,1]]";

    /*
     *  1) Default: everything is queued, the stats are the ones of always
     */
    hgobj gate = create_gate(gobj, "q_default", NULL);
    send_seqs(gobj, gate, messages);
    int ret = 0;
    ret += expect_stat(gobj, gate, "default", "msgs_in_queue", 9);
    ret += expect_stat(gobj, gate, "default", "repeated_msgs", -1);
    destroy_gate(gate);
    if(ret == 0) {
        phase_ok(gobj, "phase 1 ok: default queues everything");
    }

    /*
     *  2) not_newer on seq
     */
    gate = create_gate(gobj, "q_seq", json_pack("{s:s, s:s}",
        "repeated_key", "id",
        "repeated_field", "seq"
    ));
    send_seqs(gobj, gate, messages);
    ret = 0;
    ret += expect_stat(gobj, gate, "not_newer", "msgs_in_queue", 6);
    ret += expect_stat(gobj, gate, "not_newer", "repeated_msgs", 3);
    ret += expect_stat(gobj, gate, "not_newer", "repeated_unchecked", 1);
    ret += expect_stat(gobj, gate, "not_newer", "repeated_keys", 3);
    if(ret == 0) {
        phase_ok(gobj, "phase 2 ok: repeats and older ones are not queued");
    }

    /*
     *  3) Stop and start: the state comes back from the queue
     */
    gobj_stop(gate);
    gobj_start(gate);
    send_seqs(gobj, gate, "[[\"A\",3],[\"A\",4],[\"B\",1],[7,2]]");
    ret = 0;
    ret += expect_stat(gobj, gate, "restart", "msgs_in_queue", 8);
    ret += expect_stat(gobj, gate, "restart", "repeated_msgs", 5);
    ret += expect_stat(gobj, gate, "restart", "repeated_keys", 3);
    destroy_gate(gate);
    if(ret == 0) {
        phase_ok(gobj, "phase 3 ok: a restart keeps ignoring repeats");
    }

    /*
     *  4) equal, on a path
     */
    gate = create_gate(gobj, "q_equal", json_pack("{s:s, s:s, s:s}",
        "repeated_key", "id",
        "repeated_field", "data`tm",
        "repeated_mode", "equal"
    ));
    int tms[] = {10, 10, 9, 9, 11};
    for(size_t i = 0; i < ARRAY_SIZE(tms); i++) {
        gobj_send_event(gate, EV_SEND_MESSAGE,
            json_pack("{s:s, s:{s:i}}", "id", "A", "data", "tm", tms[i]),
            gobj
        );
    }
    ret = 0;
    ret += expect_stat(gobj, gate, "equal", "msgs_in_queue", 3);
    ret += expect_stat(gobj, gate, "equal", "repeated_msgs", 2);
    destroy_gate(gate);
    if(ret == 0) {
        phase_ok(gobj, "phase 4 ok: equal ignores only the same value");
    }

    /*
     *  5) A wrong mode: error, and the default behaviour
     */
    gate = create_gate(gobj, "q_bad_mode", json_pack("{s:s, s:s}",
        "repeated_key", "id",
        "repeated_mode", "bogus"
    ));
    send_seqs(gobj, gate, messages);
    ret = 0;
    ret += expect_stat(gobj, gate, "bad mode", "msgs_in_queue", 9);
    ret += expect_stat(gobj, gate, "bad mode", "repeated_msgs", -1);
    destroy_gate(gate);
    if(ret == 0) {
        phase_ok(gobj, "phase 5 ok: a wrong mode keeps the default");
    }

    /*
     *  6) What it costs
     */
    gate = create_gate(gobj, "q_perf_off", NULL);
    double ms_off = time_messages(gobj, gate);
    destroy_gate(gate);

    gate = create_gate(gobj, "q_perf_on", json_pack("{s:s, s:s}",
        "repeated_key", "id",
        "repeated_field", "seq"
    ));
    double ms_on = time_messages(gobj, gate);
    ret = expect_stat(gobj, gate, "perf", "repeated_msgs", 0);
    destroy_gate(gate);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "phase 6: cost of the filter",
        "messages",     "%d", PERF_MSGS,
        "keys",         "%d", PERF_KEYS,
        "msgs_s_off",   "%ld", (long)(ms_off > 0? PERF_MSGS * 1000.0 / ms_off : 0),
        "msgs_s_on",    "%ld", (long)(ms_on > 0? PERF_MSGS * 1000.0 / ms_on : 0),
        NULL
    );

    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}




                    /***************************
                     *                  FSM
                     ***************************/




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
GOBJ_DEFINE_GCLASS(C_TEST_QIOGATE);

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
        {EV_TEST_RUN,               ac_test_run,            0},
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
PUBLIC int register_c_test_qiogate(void)
{
    return create_gclass(C_TEST_QIOGATE);
}
