/****************************************************************************
 *          C_PERF_AUTH_BFF.C
 *
 *          Concurrent throughput benchmark for c_auth_bff.
 *
 *          Drives NUM_SLOTS parallel HTTP client slots through
 *          ITERATIONS_PER_SLOT sequential login round-trips each,
 *          against a BFF configured with NUM_SLOTS parallel
 *          channels and a mock Keycloak set to latency_ms=0 so
 *          the bottleneck is the BFF + parser + task + event loop
 *          code path rather than the simulated upstream.
 *
 *          At the end prints a #TIME line in the standard perf_c_*
 *          format so the result sits next to perf_c_tcp / perf_c_tcps
 *          in performance/c/README.md.
 *
 *          Design is a direct descendant of c_stress_auth_bff.c —
 *          same NUM_SLOTS concurrent model, same event flow, just
 *          without the per-slot error verification (errors would
 *          still land as gobj_log_error and fail the run, but we
 *          don't check counts) and with the time measurement
 *          wrapped around the launch / completion.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "c_perf_auth_bff.h"
#include "test_helpers.h"

/***************************************************************************
 *          Tunables
 ***************************************************************************/
#define NUM_SLOTS              5
#define ITERATIONS_PER_SLOT    200     /* → 1000 total round-trips */

/***************************************************************************
 *          Global time measurement
 ***************************************************************************/
PUBLIC time_measure_t perf_time_measure = {0};

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
SDATA (DTP_STRING,    "bff_url",       SDF_RD, "http://127.0.0.1:18801/", "BFF URL"),
SDATA (DTP_POINTER,   "user_data",     0,      0,      "user data"),
SDATA_END()
};

enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"messages",    "Trace perf orchestrator flow"},
    {0, 0}
};

typedef enum {
    SLOT_IDLE       = 0,
    SLOT_CONNECTING,
    SLOT_POSTED,
} slot_state_t;

typedef struct {
    hgobj           http_cl;
    slot_state_t    state;
    int             iteration;
} slot_t;

typedef struct _PRIVATE_DATA {
    hgobj       timer;
    slot_t      slots[NUM_SLOTS];
    BOOL        test_passed;
    BOOL        dying;
    BOOL        launched;
    int         slots_done;
    int         total_responses;
} PRIVATE_DATA;




            /***************************
             *      Prototypes
             ***************************/




PRIVATE void launch_all_slots(hgobj gobj);
PRIVATE void open_slot(hgobj gobj, int slot_idx);
PRIVATE void close_slot_and_restart_or_finish(hgobj gobj, int slot_idx);
PRIVATE int  slot_index_for_src(hgobj gobj, hgobj src);
PRIVATE void maybe_finish_and_die(hgobj gobj);




            /******************************
             *      Framework Methods
             ******************************/




PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);
}

PRIVATE void mt_destroy(hgobj gobj)
{
}

PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gobj_start(priv->timer);
    return 0;
}

PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gobj_stop(priv->timer);
    for(int i = 0; i < NUM_SLOTS; i++) {
        if(priv->slots[i].http_cl && gobj_is_running(priv->slots[i].http_cl)) {
            gobj_stop_tree(priv->slots[i].http_cl);
        }
    }
    return 0;
}

PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    set_timeout0(priv->timer, 500);
    return 0;
}

PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    clear_timeout0(priv->timer);
    return 0;
}




            /***************************
             *      Helpers
             ***************************/




PRIVATE int slot_index_for_src(hgobj gobj, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    for(int i = 0; i < NUM_SLOTS; i++) {
        if(priv->slots[i].http_cl == src) {
            return i;
        }
    }
    return -1;
}

PRIVATE void open_slot(hgobj gobj, int slot_idx)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    slot_t *slot = &priv->slots[slot_idx];

    const char *bff_url = gobj_read_str_attr(gobj, "bff_url");

    slot->http_cl = gobj_create(
        gobj_name(gobj),
        C_PROT_HTTP_CL,
        json_pack("{s:s}", "url", bff_url),
        gobj
    );
    gobj_set_bottom_gobj(
        slot->http_cl,
        gobj_create_pure_child(
            gobj_name(gobj),
            C_TCP,
            json_pack("{s:s}", "url", bff_url),
            slot->http_cl
        )
    );
    slot->state = SLOT_CONNECTING;
    gobj_start_tree(slot->http_cl);
}

PRIVATE void launch_all_slots(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->launched) return;
    priv->launched = TRUE;

    /*
     *  Start the clock RIGHT BEFORE firing the first slots so the
     *  timing covers the whole run but not the warm-up / register
     *  phase.  Pair-matched with MT_PRINT_TIME in maybe_finish_and_die.
     */
    MT_START_TIME(perf_time_measure)

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "perf: launching %d slots × %d iterations = %d round-trips",
            NUM_SLOTS, ITERATIONS_PER_SLOT,
            NUM_SLOTS * ITERATIONS_PER_SLOT
        );
    }

    for(int i = 0; i < NUM_SLOTS; i++) {
        priv->slots[i].iteration = 0;
        open_slot(gobj, i);
    }
}

PRIVATE void close_slot_and_restart_or_finish(hgobj gobj, int slot_idx)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    slot_t *slot = &priv->slots[slot_idx];

    if(slot->http_cl) {
        gobj_stop_tree(slot->http_cl);
        slot->http_cl = NULL;
    }
    slot->iteration++;

    if(slot->iteration < ITERATIONS_PER_SLOT) {
        open_slot(gobj, slot_idx);
    } else {
        slot->state = SLOT_IDLE;
        priv->slots_done++;
        maybe_finish_and_die(gobj);
    }
}

PRIVATE void maybe_finish_and_die(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->slots_done < NUM_SLOTS) {
        return;
    }

    /*
     *  Stop the clock and let cleaning() print the result.  We set
     *  the count here so the PRINT_TIME line shows ops/sec against
     *  the real number of round-trips the run performed.
     */
    MT_SET_COUNT(perf_time_measure, priv->total_responses)

    if(priv->total_responses != NUM_SLOTS * ITERATIONS_PER_SLOT) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_APP_ERROR,
            "msg",              "%s", "perf: total responses mismatch",
            "expected",         "%d", NUM_SLOTS * ITERATIONS_PER_SLOT,
            "got",              "%d", priv->total_responses,
            NULL
        );
    }

    priv->test_passed = TRUE;
    priv->dying = TRUE;
    set_timeout0(priv->timer, 200);
}




            /***************************
             *      Actions
             ***************************/




PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->dying) {
        JSON_DECREF(kw)
        set_yuno_must_die();
        return 0;
    }

    if(!priv->launched) {
        launch_all_slots(gobj);
    } else {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "perf: unexpected ac_timer after launch",
            NULL
        );
    }

    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int idx = slot_index_for_src(gobj, src);
    if(idx < 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "perf: EV_ON_OPEN from unknown src",
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }
    slot_t *slot = &priv->slots[idx];

    json_t *jn_headers = json_pack("{s:s}",
        "Origin", "http://localhost"
    );
    json_t *jn_data = json_pack("{s:s, s:s}",
        "username", "perfuser",
        "password", "perfpass"
    );
    json_t *query = json_pack("{s:s, s:s, s:o, s:o}",
        "method",   "POST",
        "resource", "/auth/login",
        "headers",  jn_headers,
        "data",     jn_data
    );
    gobj_send_event(slot->http_cl, EV_SEND_MESSAGE, query, gobj);

    slot->state = SLOT_POSTED;

    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int idx = slot_index_for_src(gobj, src);
    if(idx < 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "perf: EV_ON_MESSAGE from unknown src",
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    if(status != 200) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "perf: non-200 response",
            "slot",     "%d", idx,
            "status",   "%d", status,
            NULL
        );
    } else {
        priv->total_responses++;
    }

    JSON_DECREF(kw)
    close_slot_and_restart_or_finish(gobj, idx);
    return 0;
}

PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *      FSM
                     ***************************/




PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
};

GOBJ_DEFINE_GCLASS(C_PERF_AUTH_BFF);

PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,        ac_timer,       0},
        {EV_ON_OPEN,        ac_on_open,     0},
        {EV_ON_MESSAGE,     ac_on_message,  0},
        {EV_ON_CLOSE,       ac_on_close,    0},
        {EV_STOPPED,        ac_stopped,     0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,        0},
        {EV_ON_OPEN,        0},
        {EV_ON_MESSAGE,     0},
        {EV_ON_CLOSE,       0},
        {EV_STOPPED,        0},
        {0, 0}
    };

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,
        0,
        s_user_trace_level,
        0
    );
    if(!__gclass__) {
        return -1;
    }
    return 0;
}

PUBLIC int register_c_perf_auth_bff(void)
{
    return create_gclass(C_PERF_AUTH_BFF);
}
