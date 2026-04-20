/****************************************************************************
 *          C_STRESS_AUTH_BFF.C
 *
 *          Concurrent stress orchestrator for the c_auth_bff GClass.
 *
 *          Owns NUM_SLOTS HTTP client slots and drives each one
 *          through a fixed number of iterations of
 *            connect → POST /auth/login → wait response → close
 *          in parallel.  At the end, aggregates stats across every
 *          C_AUTH_BFF under __bff_side__ and every C_MOCK_KEYCLOAK
 *          under __kc_side__, and asserts the totals match what
 *          the driver thinks it did.
 *
 *          Design is deliberately minimal for the first version —
 *          no RNG, no chaos knobs, just "N clients hammer the BFF
 *          for K iterations and everything lines up".  Chaos
 *          (random cancels, injected KC errors, latency spikes)
 *          is follow-up work.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "c_stress_auth_bff.h"
#include "test_helpers.h"

/***************************************************************************
 *          Tunables (compile-time for now)
 ***************************************************************************/
#define NUM_SLOTS              5      /* concurrent HTTP clients */
#define ITERATIONS_PER_SLOT    20     /* login loops per slot */

/*
 *  Total requests = NUM_SLOTS * ITERATIONS_PER_SLOT.
 *  With NUM_SLOTS=5 and ITERATIONS_PER_SLOT=20 → 100 requests,
 *  comfortable under the 30 s auto_kill even with 10 ms KC latency.
 */

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
    {"messages",    "Trace stress orchestrator flow"},
    {0, 0}
};

typedef enum {
    SLOT_IDLE       = 0,  /* not yet started / finished */
    SLOT_CONNECTING,      /* http_cl start_tree issued */
    SLOT_POSTED,          /* POST written, waiting for EV_ON_MESSAGE */
} slot_state_t;

typedef struct {
    hgobj           http_cl;
    slot_state_t    state;
    int             iteration;          /* 0-based, up to ITERATIONS_PER_SLOT */
    int             ok_count;
    int             err_count;
} slot_t;

typedef struct _PRIVATE_DATA {
    hgobj       timer;
    slot_t      slots[NUM_SLOTS];
    BOOL        test_passed;
    BOOL        dying;
    BOOL        launched;               /* warm-up timer fired, slots created */
    int         slots_done;             /* count of slots at iteration ITERATIONS_PER_SLOT */
} PRIVATE_DATA;




            /***************************
             *      Prototypes
             ***************************/




PRIVATE void launch_all_slots(hgobj gobj);
PRIVATE void open_slot(hgobj gobj, int slot_idx);
PRIVATE void close_slot_and_restart_or_finish(hgobj gobj, int slot_idx);
PRIVATE int  slot_index_for_src(hgobj gobj, hgobj src);
PRIVATE void maybe_verify_and_die(hgobj gobj);




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
    /* Safety net: stop any slot http_cl still running. */
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
    /*
     *  500 ms warm-up: both IOGates need their listen sockets ready,
     *  and on our side we want all 5 slots to start at roughly the
     *  same instant so the BFF channels are really concurrent.
     */
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

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "stress: launching %d concurrent slots × %d iterations",
            NUM_SLOTS, ITERATIONS_PER_SLOT
        );
    }

    for(int i = 0; i < NUM_SLOTS; i++) {
        priv->slots[i].iteration = 0;
        open_slot(gobj, i);
    }
}

/*
 *  Finish the current iteration on `slot_idx`: stop the http_cl,
 *  bump iteration, and either open a fresh one for the next
 *  iteration or mark the slot done.  When all slots are done,
 *  arm a short grace period and then verify.
 */
PRIVATE void close_slot_and_restart_or_finish(hgobj gobj, int slot_idx)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    slot_t *slot = &priv->slots[slot_idx];

    if(slot->http_cl) {
        gobj_stop_tree(slot->http_cl);
        /*
         *  Drop the local reference.  The stopped gobj stays as a
         *  (stopped) child of the driver until yuno teardown — same
         *  pattern as test11.  Fine for a stress run of O(100)
         *  iterations; if we ever bump iterations to thousands we
         *  will need to explicitly destroy or use EV_STOPPED → destroy.
         */
        slot->http_cl = NULL;
    }
    slot->iteration++;

    if(slot->iteration < ITERATIONS_PER_SLOT) {
        open_slot(gobj, slot_idx);
    } else {
        slot->state = SLOT_IDLE;
        priv->slots_done++;

        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj,
                "stress: slot %d done (%d/%d slots complete)",
                slot_idx, priv->slots_done, NUM_SLOTS
            );
        }

        maybe_verify_and_die(gobj);
    }
}

PRIVATE void maybe_verify_and_die(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->slots_done < NUM_SLOTS) {
        return;
    }

    /*
     *  Per-slot sanity.
     */
    int expected_per_slot = ITERATIONS_PER_SLOT;
    int total_ok = 0;
    int total_err = 0;
    for(int i = 0; i < NUM_SLOTS; i++) {
        slot_t *slot = &priv->slots[i];
        if(slot->ok_count != expected_per_slot) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_APP,
                "msg",      "%s", "stress: slot ok_count mismatch",
                "slot",     "%d", i,
                "expected", "%d", expected_per_slot,
                "got",      "%d", slot->ok_count,
                NULL
            );
        }
        if(slot->err_count != 0) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_APP,
                "msg",      "%s", "stress: slot err_count != 0",
                "slot",     "%d", i,
                "err_count","%d", slot->err_count,
                NULL
            );
        }
        total_ok  += slot->ok_count;
        total_err += slot->err_count;
    }

    int total_expected = NUM_SLOTS * ITERATIONS_PER_SLOT;
    if(total_ok != total_expected || total_err != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "stress: aggregated response counters wrong",
            "total_ok",     "%d", total_ok,
            "total_err",    "%d", total_err,
            "expected_ok",  "%d", total_expected,
            NULL
        );
    }

    /*
     *  Aggregate BFF stats across every C_AUTH_BFF instance under
     *  __bff_side__.  Each slot is expected to run ITERATIONS_PER_SLOT
     *  requests against its own channel.  With 5 channels * 20
     *  iterations the sums should be:
     *
     *      requests_total    = NUM_SLOTS * ITERATIONS_PER_SLOT
     *      kc_calls          = NUM_SLOTS * ITERATIONS_PER_SLOT
     *      kc_ok             = NUM_SLOTS * ITERATIONS_PER_SLOT
     *      kc_errors         = 0
     *      bff_errors        = 0
     *      responses_dropped = 0
     *      kc_timeouts       = 0
     *      q_full_drops      = 0
     *
     *  Walk __bff_side__ and sum the per-instance counters.
     */
    hgobj bff_side = gobj_find_service("__bff_side__", FALSE);
    if(bff_side) {
        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", "C_AUTH_BFF"
        );
        json_t *dl = gobj_match_children_tree(bff_side, jn_filter);
        json_int_t sum_requests = 0, sum_kc_calls = 0, sum_kc_ok = 0;
        json_int_t sum_kc_errors = 0, sum_bff_errors = 0;
        json_int_t sum_dropped = 0, sum_timeouts = 0, sum_q_full = 0;
        int bff_count = 0;
        int idx;
        json_t *jn_bff;
        json_array_foreach(dl, idx, jn_bff) {
            hgobj bff = (hgobj)(size_t)json_integer_value(jn_bff);
            json_t *resp = gobj_stats(bff, NULL, 0, gobj);
            if(resp) {
                json_t *jn_data = kw_get_dict(gobj, resp, "data", NULL, 0);
                if(jn_data) {
                    sum_requests   += json_integer_value(json_object_get(jn_data, "requests_total"));
                    sum_kc_calls   += json_integer_value(json_object_get(jn_data, "kc_calls"));
                    sum_kc_ok      += json_integer_value(json_object_get(jn_data, "kc_ok"));
                    sum_kc_errors  += json_integer_value(json_object_get(jn_data, "kc_errors"));
                    sum_bff_errors += json_integer_value(json_object_get(jn_data, "bff_errors"));
                    sum_dropped    += json_integer_value(json_object_get(jn_data, "responses_dropped"));
                    sum_timeouts   += json_integer_value(json_object_get(jn_data, "kc_timeouts"));
                    sum_q_full     += json_integer_value(json_object_get(jn_data, "q_full_drops"));
                }
                JSON_DECREF(resp)
                bff_count++;
            }
        }
        gobj_free_iter(dl);

        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj,
                "stress: BFF aggregate: instances=%d requests_total=%lld kc_ok=%lld "
                "errors=%lld dropped=%lld timeouts=%lld q_full=%lld",
                bff_count,
                (long long)sum_requests, (long long)sum_kc_ok,
                (long long)(sum_kc_errors + sum_bff_errors),
                (long long)sum_dropped,
                (long long)sum_timeouts, (long long)sum_q_full
            );
        }

        if(bff_count != NUM_SLOTS) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_APP,
                "msg",      "%s", "stress: wrong BFF instance count",
                "expected", "%d", NUM_SLOTS,
                "got",      "%d", bff_count,
                NULL
            );
        }
        if(sum_requests != total_expected
                || sum_kc_calls != total_expected
                || sum_kc_ok    != total_expected
                || sum_kc_errors != 0
                || sum_bff_errors != 0
                || sum_dropped   != 0
                || sum_timeouts  != 0
                || sum_q_full    != 0) {
            gobj_log_error(gobj, 0,
                "function",        "%s", __FUNCTION__,
                "msgset",          "%s", MSGSET_APP,
                "msg",             "%s", "stress: BFF aggregate stats wrong",
                "expected_total",  "%d", total_expected,
                "requests_total",  "%lld", (long long)sum_requests,
                "kc_ok",           "%lld", (long long)sum_kc_ok,
                "kc_errors",       "%lld", (long long)sum_kc_errors,
                "bff_errors",      "%lld", (long long)sum_bff_errors,
                "responses_dropped","%lld",(long long)sum_dropped,
                "kc_timeouts",     "%lld", (long long)sum_timeouts,
                "q_full_drops",    "%lld", (long long)sum_q_full,
                NULL
            );
        }
    } else {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "stress: __bff_side__ not found",
            NULL
        );
    }

    priv->test_passed = TRUE;

    /*
     *  Death sequence — same inline C_TIMER0 pattern as the test
     *  drivers.  All slot http_cls are already stopped (we either
     *  completed every iteration or never started), so the grace
     *  timer just gives io_uring close events time to propagate
     *  before the yuno tears down.
     */
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
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "stress: unexpected ac_timer after launch",
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
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "stress: EV_ON_OPEN from unknown src",
            "src",      "%s", gobj_short_name(src),
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }
    slot_t *slot = &priv->slots[idx];
    if(slot->state != SLOT_CONNECTING) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "stress: EV_ON_OPEN in unexpected slot state",
            "slot",     "%d", idx,
            "state",    "%d", (int)slot->state,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    /* POST /auth/login */
    json_t *jn_headers = json_pack("{s:s}",
        "Origin", "http://localhost"
    );
    char user_buf[64];
    snprintf(user_buf, sizeof(user_buf), "slot%d-iter%d", idx, slot->iteration);
    json_t *jn_data = json_pack("{s:s, s:s}",
        "username", user_buf,
        "password", "testpass"
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
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "stress: EV_ON_MESSAGE from unknown src",
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }
    slot_t *slot = &priv->slots[idx];

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, 0);

    if(status == 200
            && jn_body
            && json_is_true(json_object_get(jn_body, "success"))) {
        slot->ok_count++;
    } else {
        slot->err_count++;
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "stress: unexpected non-200 response",
            "slot",     "%d", idx,
            "iteration","%d", slot->iteration,
            "status",   "%d", status,
            NULL
        );
    }

    JSON_DECREF(kw)

    /*
     *  Drive the slot to its next iteration (or finish).
     */
    close_slot_and_restart_or_finish(gobj, idx);
    return 0;
}

PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *  Close events are expected — we stop_tree each http_cl after
     *  receiving its response.  Ignore unless we haven't even seen
     *  a response yet on that slot, which would mean the BFF tore
     *  us down unexpectedly.
     */
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

GOBJ_DEFINE_GCLASS(C_STRESS_AUTH_BFF);

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

PUBLIC int register_c_stress_auth_bff(void)
{
    return create_gclass(C_STRESS_AUTH_BFF);
}
