/****************************************************************************
 *          C_TEST8_QUEUE_FULL.C
 *
 *          Driver for test8_queue_full: pipeline 4 POSTs on one TCP
 *          connection and verify that the BFF's queue overflow branch
 *          rejects the 4th with HTTP 503 while the first three are
 *          served normally after their staged KC round-trips.
 *
 *          Setup
 *          =====
 *          - BFF pending_queue_size = 2 (smallest interesting case
 *            that still exercises q_count > 0 during processing).
 *          - Mock KC latency_ms = 300 (slow enough to hold processing
 *            busy so the queue fills, fast enough that three KC
 *            round-trips complete well within auto_kill).
 *
 *          Expected flow
 *          =============
 *            POST 1 → enqueue (q=1) → dequeue (q=0) → processing=TRUE
 *                   → task started, KC round-trip in flight
 *            POST 2 → enqueue (q=1) → process_next gated on
 *                     processing, stays queued
 *            POST 3 → enqueue (q=2) → stays queued (q_max_seen=2)
 *            POST 4 → enqueue check: q=2 >= queue_size=2 → REJECT
 *                     → send_error_response 503 synchronously,
 *                       no KC round-trip
 *
 *          After the 4 POSTs are on the wire (all synchronous in
 *          ac_on_open), the driver just waits for responses.  The
 *          BFF writes the 503 immediately (from the synchronous
 *          rejection path) and then drains POST 1 → POST 2 → POST 3
 *          via three back-to-back 300 ms KC rounds.  On the wire,
 *          order is likely: 503, 200, 200, 200 — c_prot_http_sr
 *          writes replies as they come, not in strict HTTP/1.1
 *          pipelining order.  The driver does not assume order; it
 *          counts 200s and 503s separately and verifies totals.
 *
 *          Expected state at verify time
 *          =============================
 *          BFF stats:
 *              requests_total    = 3   (POST 4 never got past the
 *                                       q_count check, so it never
 *                                       incremented this counter)
 *              kc_calls          = 3
 *              kc_ok             = 3
 *              kc_errors         = 0
 *              bff_errors        = 1   (the 503 through
 *                                       send_error_response)
 *              responses_dropped = 0
 *              kc_timeouts       = 0
 *              q_full_drops      = 1   (THE signal that the overflow
 *                                       branch fired)
 *
 *          Mock KC stats:
 *              token_requests     = 3
 *              responses_sent     = 3
 *              deferred_responses = 3
 *              pending_cancelled  = 0
 *
 *          The combination of kc_calls=3 + q_full_drops=1 is a
 *          sufficient proxy for "queue reached 2 at some point":
 *          three simultaneously in-flight requests could only have
 *          coexisted if q_count actually reached queue_size=2.
 *
 *          Note on expected error log
 *          ==========================
 *          send_error_response emits a gobj_log_error on every call
 *          (no silent errors).  For the 503 path, that error is:
 *            msg="BFF error response"
 *            error="Server busy, retry in a moment"
 *          Declared to set_expected_results in main_test8_queue_full.c.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "c_test8_queue_full.h"
#include "test_helpers.h"

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
#define EXPECTED_RESPONSES   4      /* 3× 200 + 1× 503 */

PRIVATE sdata_desc_t attrs_table[] = {
SDATA (DTP_STRING,    "bff_url",       SDF_RD, "http://127.0.0.1:18801/", "BFF URL"),
SDATA (DTP_POINTER,   "user_data",     0,      0,      "user data"),
SDATA_END()
};

enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"messages",    "Trace test driver flow"},
    {0, 0}
};

typedef enum {
    T8_INIT        = 0,  /* waiting for warm-up timer */
    T8_CONNECTING,       /* http_cl started, waiting EV_ON_OPEN */
    T8_AWAITING,         /* 4 POSTs sent, counting responses */
    T8_GRACE,            /* all 4 received, waiting for mock KC to settle */
    T8_VERIFIED          /* stats checked, death timer running */
} t8_phase_t;

typedef struct _PRIVATE_DATA {
    hgobj       timer;
    hgobj       gobj_http_cl;
    BOOL        test_passed;
    BOOL        dying;
    t8_phase_t  phase;
    int         responses_received;
    int         ok_count;
    int         err_count;
} PRIVATE_DATA;




            /******************************
             *      Framework Methods
             ******************************/




PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    /* C_TIMER0 for ms precision, same rationale as test9 / test10. */
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);
    priv->phase = T8_INIT;
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
    if(priv->gobj_http_cl && gobj_is_running(priv->gobj_http_cl)) {
        gobj_stop_tree(priv->gobj_http_cl);
    }
    return 0;
}

PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    set_timeout0(priv->timer, 300);
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




/*
 *  POST /auth/login with a distinct username tag so the BFF traces
 *  can tell the four pipelined requests apart.  The BFF does not
 *  validate the username/password against anything — the mock KC
 *  always replies with its own configured identity regardless of
 *  what the BFF forwarded.
 */
PRIVATE void post_login_numbered(hgobj gobj, int n)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char user_buf[64];
    snprintf(user_buf, sizeof(user_buf), "testuser-%d", n);

    json_t *jn_headers = json_pack("{s:s}",
        "Origin", "http://localhost"
    );
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
    gobj_send_event(priv->gobj_http_cl, EV_SEND_MESSAGE, query, gobj);
}

PRIVATE void verify_and_die(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Response counts: 3× 200 and 1× 503.
     */
    if(priv->ok_count != 3) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test8_queue_full: wrong number of 200 responses",
            "expected", "%d", 3,
            "got",      "%d", priv->ok_count,
            NULL
        );
    }
    if(priv->err_count != 1) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test8_queue_full: wrong number of 503 responses",
            "expected", "%d", 1,
            "got",      "%d", priv->err_count,
            NULL
        );
    }

    /*
     *  BFF stats.  requests_total counts successful enqueues only
     *  (POST 4 never made it past the q_count check).  q_full_drops
     *  is the primary signal the overflow branch fired.
     */
    const test_stat_expect_t bff_expected[] = {
        {"requests_total",      3},
        {"kc_calls",            3},
        {"kc_ok",               3},
        {"kc_errors",           0},
        {"bff_errors",          1},
        {"responses_dropped",   0},
        {"kc_timeouts",         0},
        {"q_full_drops",        1},
        {NULL, 0}
    };
    hgobj bff = test_helpers_find_bff(gobj);
    test_helpers_check_stats(gobj, bff, "test8_queue_full[bff]", bff_expected);

    /*
     *  Mock KC stats.  Three requests crossed the wire to the mock,
     *  all got deferred 300 ms, all responded.
     */
    const test_stat_expect_t kc_expected[] = {
        {"token_requests",      3},
        {"responses_sent",      3},
        {"deferred_responses",  3},
        {"pending_cancelled",   0},
        {NULL, 0}
    };
    hgobj kc = test_helpers_find_service_child(gobj, "__kc_side__", "C_MOCK_KEYCLOAK");
    test_helpers_check_stats(gobj, kc, "test8_queue_full[mock-kc]", kc_expected);

    priv->test_passed = TRUE;
    priv->phase = T8_VERIFIED;

    /* Death sequence (C_TIMER0 inline, same pattern as test9 / test10). */
    if(priv->gobj_http_cl) {
        gobj_stop_tree(priv->gobj_http_cl);
    }
    priv->dying = TRUE;
    set_timeout0(priv->timer, 100);
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

    switch(priv->phase) {
    case T8_INIT: {
        const char *bff_url = gobj_read_str_attr(gobj, "bff_url");
        priv->gobj_http_cl = gobj_create(
            gobj_name(gobj),
            C_PROT_HTTP_CL,
            json_pack("{s:s}", "url", bff_url),
            gobj
        );
        gobj_set_bottom_gobj(
            priv->gobj_http_cl,
            gobj_create_pure_child(
                gobj_name(gobj),
                C_TCP,
                json_pack("{s:s}", "url", bff_url),
                priv->gobj_http_cl
            )
        );
        gobj_start_tree(priv->gobj_http_cl);
        priv->phase = T8_CONNECTING;
        break;
    }

    case T8_GRACE: {
        /*
         *  All 4 responses received, plus a short grace window to
         *  let the BFF's last ac_end_task / process_next housekeeping
         *  settle before we snapshot stats.
         */
        verify_and_die(gobj);
        break;
    }

    default:
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test8_queue_full: unexpected ac_timer phase",
            "phase",    "%d", (int)priv->phase,
            NULL
        );
        break;
    }

    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase != T8_CONNECTING) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test8_queue_full: EV_ON_OPEN in unexpected phase",
            "phase",    "%d", (int)priv->phase,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "test8: connected to BFF, pipelining 4 POSTs");
    }

    /*
     *  Pipeline four POST requests on the same TCP connection.  The
     *  BFF's c_prot_http_sr parser splits them into four separate
     *  EV_ON_MESSAGE events, each processed synchronously in the
     *  BFF's ac_on_message.  With pending_queue_size=2 the 4th hits
     *  the overflow branch and receives 503 immediately, while
     *  POSTs 1/2/3 wait for their staged KC round-trips.
     */
    post_login_numbered(gobj, 1);
    post_login_numbered(gobj, 2);
    post_login_numbered(gobj, 3);
    post_login_numbered(gobj, 4);

    priv->phase = T8_AWAITING;

    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase != T8_AWAITING) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test8_queue_full: EV_ON_MESSAGE in unexpected phase",
            "phase",    "%d", (int)priv->phase,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, 0);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "test8: response #%d status=%d",
            priv->responses_received + 1, status
        );
    }

    priv->responses_received++;

    if(status == 200) {
        if(!jn_body || !json_is_true(json_object_get(jn_body, "success"))) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_APP_ERROR,
                "msg",      "%s", "test8_queue_full: 200 response with success!=true",
                NULL
            );
        } else {
            priv->ok_count++;
        }
    } else if(status == 503) {
        if(!jn_body || json_is_true(json_object_get(jn_body, "success"))) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_APP_ERROR,
                "msg",      "%s", "test8_queue_full: 503 response with success!=false",
                NULL
            );
        } else {
            priv->err_count++;
        }
    } else {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test8_queue_full: unexpected response status",
            "status",   "%d", status,
            NULL
        );
    }

    if(priv->responses_received >= EXPECTED_RESPONSES) {
        /*
         *  All 4 responses landed — arm a short grace period before
         *  verifying stats, same reason as test10_kc_silence's grace:
         *  the last KC round-trip's ac_end_task → process_next
         *  housekeeping may still be in flight even though we've
         *  received the response bytes on the driver side.
         */
        priv->phase = T8_GRACE;
        set_timeout0(priv->timer, 100);
    }

    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  A close before we've seen all 4 responses means the BFF tore
     *  the connection down mid-stream — real bug in the queue path.
     */
    if(priv->phase == T8_AWAITING && priv->responses_received < EXPECTED_RESPONSES) {
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_APP_ERROR,
            "msg",                  "%s", "test8_queue_full: connection closed before all responses received",
            "responses_received",   "%d", priv->responses_received,
            "expected",             "%d", EXPECTED_RESPONSES,
            NULL
        );
        priv->dying = TRUE;
        set_timeout0(priv->timer, 100);
    }

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

GOBJ_DEFINE_GCLASS(C_TEST8_QUEUE_FULL);

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

PUBLIC int register_c_test8_queue_full(void)
{
    return create_gclass(C_TEST8_QUEUE_FULL);
}
