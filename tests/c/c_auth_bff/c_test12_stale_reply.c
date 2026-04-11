/****************************************************************************
 *          C_TEST12_STALE_REPLY.C
 *
 *          Driver for test12_stale_reply.
 *
 *          Reproduces deterministically the cross-user token leak
 *          that test11_cancel_retry sidestepped with a long retry
 *          delay.  With persistent_channels=true + a short retry
 *          delay, the second connection lands on the SAME BFF
 *          instance WHILE a stale task from the first connection is
 *          still waiting for Keycloak — and without the generation
 *          fix in the previous commit, the stale reply would be
 *          forwarded to the new socket, delivering user A's token
 *          to browser B.
 *
 *          Timeline
 *          ========
 *            t = 0      mt_play → 300 ms warm timer
 *            t = 300    warm fires → open http_cl #1
 *                       phase: T12_INIT → T12_AWAITING_OPEN_1
 *            t ≈ 300    EV_ON_OPEN #1 → POST /auth/login #1, arm 50 ms
 *                       cancel timer
 *                       phase: T12_AWAITING_OPEN_1 → T12_AWAITING_CANCEL
 *            t ≈ 300    BFF enqueues with gen=1, dispatches task to KC
 *                       Mock KC arms 500 ms latency (slot for A's reply)
 *            t = 350    cancel fires → gobj_stop_tree(http_cl #1), arm
 *                       50 ms retry
 *                       phase: T12_AWAITING_CANCEL → T12_AWAITING_RETRY
 *            t ≈ 350    BFF ac_on_close → browser_alive_gen = 0
 *            t = 400    retry fires → open http_cl #2 on the SAME
 *                       persistent channel
 *                       phase: T12_AWAITING_RETRY → T12_AWAITING_OPEN_2
 *            t ≈ 400    BFF ac_on_open #2 → browser_alive_gen = 2
 *                       (browser_gen_counter bumped from 1 to 2)
 *            t ≈ 400    EV_ON_OPEN #2 → POST /auth/login #2
 *                       phase: T12_AWAITING_OPEN_2 → T12_AWAITING_RESPONSE
 *            t ≈ 400    BFF enqueues with gen=2, process_next gated
 *                       on processing (task 1 still in flight)
 *            t ≈ 800    Mock KC's first latency timer fires →
 *                       reply for A arrives at BFF → result_token_response
 *                       → task_gen=1 vs browser_alive_gen=2 → MISMATCH
 *                       → responses_dropped++ → CONTINUE_TASK
 *                       → ac_end_task → process_next dispatches task B
 *                       → Mock KC arms fresh 500 ms latency
 *            t ≈ 1300   Mock KC's second latency fires → reply for B
 *                       → task_gen=2 matches browser_alive_gen=2
 *                       → forward → driver receives response
 *                       phase: T12_AWAITING_RESPONSE → T12_VERIFIED
 *            t ≈ 1400   death timer → set_yuno_must_die
 *
 *          Asserted expected state
 *          =======================
 *          Driver observable:
 *              responses_received == 1   (THE signal the fix works —
 *                                          without it the stale A
 *                                          reply would count as a
 *                                          second response)
 *
 *          BFF stats:
 *              requests_total     = 2
 *              kc_calls           = 2
 *              kc_ok              = 2   (both KC calls succeeded)
 *              kc_errors          = 0
 *              bff_errors         = 0   (no send_error_response)
 *              responses_dropped  = 1   (A's reply dropped by the
 *                                         gen mismatch gate — the
 *                                         direct observable of the
 *                                         fix working)
 *              kc_timeouts        = 0
 *              q_full_drops       = 0
 *
 *          Mock KC stats:
 *              token_requests     = 2
 *              responses_sent     = 2   (BOTH replies wrote fully to
 *                                         the BFF side — the fix
 *                                         is entirely on the BFF;
 *                                         the mock doesn't care)
 *              deferred_responses = 2
 *              pending_cancelled  = 0
 *
 *          Failure mode without the fix
 *          ============================
 *          Pre-fix code had a single BOOL browser_alive.  The
 *          sequence would go:
 *            - A closes → browser_alive = FALSE
 *            - B opens  → browser_alive = TRUE
 *            - A's stale reply arrives → gate sees TRUE → forward
 *              to priv->active_browser_src (the same c_prot_http_sr
 *              gobj under persistent_channels) → written to the
 *              current TCP (B's) → driver receives A's response
 *              on B's connection.
 *
 *          The driver's ac_on_message would then see 2 responses on
 *          http_cl #2 — this file flags that as a hard error
 *          ("cross-user token leak detected") and also asserts
 *          responses_dropped == 1 on the BFF stats.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "c_test12_stale_reply.h"
#include "test_helpers.h"

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
    {"messages",    "Trace test driver flow"},
    {0, 0}
};

typedef enum {
    T12_INIT             = 0,
    T12_AWAITING_OPEN_1,        /* http_cl #1 started, waiting EV_ON_OPEN */
    T12_AWAITING_CANCEL,        /* POST #1 sent, waiting cancel timer */
    T12_AWAITING_RETRY,         /* cancelled, waiting retry timer */
    T12_AWAITING_OPEN_2,        /* http_cl #2 started, waiting EV_ON_OPEN */
    T12_AWAITING_RESPONSE,      /* POST #2 sent, waiting for the ONE reply */
    T12_VERIFIED
} t12_phase_t;

typedef struct _PRIVATE_DATA {
    hgobj       timer;
    hgobj       gobj_http_cl;   /* slot reused for #1 then #2 */
    BOOL        test_passed;
    BOOL        dying;
    t12_phase_t phase;
    int         responses_on_conn_2;    /* must end at exactly 1 */
} PRIVATE_DATA;




            /******************************
             *      Framework Methods
             ******************************/




PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);
    priv->phase = T12_INIT;
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




PRIVATE void open_new_http_cl(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
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
}

PRIVATE void post_login(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_headers = json_pack("{s:s}",
        "Origin", "http://localhost"
    );
    json_t *jn_data = json_pack("{s:s, s:s}",
        "username", "testuser",
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
     *  Driver-side: exactly ONE response on connection #2.  A count of
     *  2 means the stale reply for A's task leaked onto B's socket —
     *  the fix has regressed and we have a cross-user token leak.
     */
    if(priv->responses_on_conn_2 != 1) {
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_APP_ERROR,
            "msg",                  "%s", "test12_stale_reply: cross-user token leak — more than one reply on conn 2",
            "responses_on_conn_2",  "%d", priv->responses_on_conn_2,
            "expected",             "%d", 1,
            NULL
        );
    }

    /*
     *  BFF-side: two full round-trips, the first one dropped by the
     *  gen-mismatch gate.  responses_dropped == 1 is the direct
     *  observable of the fix working.
     */
    const test_stat_expect_t bff_expected[] = {
        {"requests_total",      2},
        {"kc_calls",            2},
        {"kc_ok",               2},
        {"kc_errors",           0},
        {"bff_errors",          0},
        {"responses_dropped",   1},
        {"kc_timeouts",         0},
        {"q_full_drops",        0},
        {NULL, 0}
    };
    hgobj bff = test_helpers_find_bff(gobj);
    test_helpers_check_stats(gobj, bff, "test12_stale_reply[bff]", bff_expected);

    /*
     *  Mock-KC-side: both requests reached the mock and both latency
     *  timers fired and wrote their replies to the BFF.  pending_cancelled
     *  stays 0 because by the time A's outbound c_tcp closes (during
     *  ac_end_task after the drop), the latency timer has already
     *  fired.
     */
    const test_stat_expect_t kc_expected[] = {
        {"token_requests",      2},
        {"responses_sent",      2},
        {"deferred_responses",  2},
        {"pending_cancelled",   0},
        {NULL, 0}
    };
    hgobj kc = test_helpers_find_service_child(gobj, "__kc_side__", "C_MOCK_KEYCLOAK");
    test_helpers_check_stats(gobj, kc, "test12_stale_reply[mock-kc]", kc_expected);

    priv->test_passed = TRUE;
    priv->phase = T12_VERIFIED;

    /* Death sequence — inlined C_TIMER0 pattern. */
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
    case T12_INIT: {
        open_new_http_cl(gobj);
        priv->phase = T12_AWAITING_OPEN_1;
        break;
    }

    case T12_AWAITING_CANCEL: {
        /*
         *  Cancel the first http_cl while A's KC round-trip is still
         *  in flight at the BFF (mock KC is holding the response in
         *  its latency slot).
         */
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj, "test12: aborting http_cl #1 (conn A)");
        }
        if(priv->gobj_http_cl && gobj_is_running(priv->gobj_http_cl)) {
            gobj_stop_tree(priv->gobj_http_cl);
        }
        priv->phase = T12_AWAITING_RETRY;
        /*
         *  Short retry delay — the whole point of this test is to
         *  open conn B BEFORE the mock KC's latency timer fires (at
         *  ~500 ms after POST A).  50 ms leaves plenty of headroom.
         */
        set_timeout0(priv->timer, 50);
        break;
    }

    case T12_AWAITING_RETRY: {
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj, "test12: opening http_cl #2 (conn B) while A's reply is still pending");
        }
        open_new_http_cl(gobj);
        priv->phase = T12_AWAITING_OPEN_2;
        break;
    }

    default:
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test12_stale_reply: unexpected ac_timer phase",
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

    switch(priv->phase) {
    case T12_AWAITING_OPEN_1: {
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj, "test12: http_cl #1 connected (conn A), posting login");
        }
        post_login(gobj);
        priv->phase = T12_AWAITING_CANCEL;
        /*
         *  Cancel 50 ms after POST A: well within the mock KC's
         *  500 ms latency window, so A's KC reply has NOT arrived
         *  yet when we abort.
         */
        set_timeout0(priv->timer, 50);
        break;
    }

    case T12_AWAITING_OPEN_2: {
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj, "test12: http_cl #2 connected (conn B), posting login");
        }
        post_login(gobj);
        priv->phase = T12_AWAITING_RESPONSE;
        /*
         *  No local timer — wait for B's reply to come back through
         *  ac_on_message.  Mock KC will arm a fresh 500 ms latency
         *  for this task AFTER ac_end_task drains A's dropped reply.
         */
        break;
    }

    default:
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test12_stale_reply: EV_ON_OPEN in unexpected phase",
            "phase",    "%d", (int)priv->phase,
            NULL
        );
        break;
    }

    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase != T12_AWAITING_RESPONSE && priv->phase != T12_VERIFIED) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test12_stale_reply: EV_ON_MESSAGE in unexpected phase",
            "phase",    "%d", (int)priv->phase,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    priv->responses_on_conn_2++;

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, 0);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "test12: conn B response #%d status=%d",
            priv->responses_on_conn_2, status
        );
    }

    /*
     *  Any SECOND response would be A's stale reply leaking onto
     *  B's socket — the exact cross-user token leak this test is
     *  here to catch.  Flag it hard.
     */
    if(priv->responses_on_conn_2 > 1) {
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_APP_ERROR,
            "msg",                  "%s", "test12_stale_reply: CROSS-USER TOKEN LEAK — second reply on conn B",
            "responses_on_conn_2",  "%d", priv->responses_on_conn_2,
            NULL
        );
        JSON_DECREF(kw)
        priv->dying = TRUE;
        set_timeout0(priv->timer, 100);
        return 0;
    }

    /*
     *  First response on conn B — this must be B's own reply, with
     *  status 200 and success=true.
     */
    if(status != 200) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test12_stale_reply: conn B non-200",
            "status",   "%d", status,
            NULL
        );
        goto die;
    }
    if(!jn_body || !json_is_object(jn_body)
            || !json_is_true(json_object_get(jn_body, "success"))) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test12_stale_reply: conn B body missing or success!=true",
            NULL
        );
        goto die;
    }

    verify_and_die(gobj);
    JSON_DECREF(kw)
    return 0;

die:
    KW_DECREF(kw)
    priv->dying = TRUE;
    set_timeout0(priv->timer, 100);
    return 0;
}

PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Expected closes:
     *    - After we abort http_cl #1 (phase T12_AWAITING_RETRY+)
     *    - At yuno shutdown (phase T12_VERIFIED+)
     *  An earlier close is an error.
     */
    if(priv->phase < T12_AWAITING_CANCEL) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test12_stale_reply: unexpected EV_ON_CLOSE",
            "phase",    "%d", (int)priv->phase,
            NULL
        );
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

GOBJ_DEFINE_GCLASS(C_TEST12_STALE_REPLY);

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

PUBLIC int register_c_test12_stale_reply(void)
{
    return create_gclass(C_TEST12_STALE_REPLY);
}
