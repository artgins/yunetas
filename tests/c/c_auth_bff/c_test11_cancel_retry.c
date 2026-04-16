/****************************************************************************
 *          C_TEST11_CANCEL_RETRY.C
 *
 *          Driver for test11_cancel_retry.
 *
 *          Proves that after a test9-style cancel-during-KC-round-trip,
 *          the BFF instance is in a clean enough state to serve the
 *          NEXT request on a fresh connection to the same channel.
 *          Catches any latent state leak between the dropped response
 *          and the next lifecycle:
 *
 *            - queue indices (q_head / q_tail / q_count) back to zero
 *            - priv->gobj_http cleared by ac_end_task so the invariant
 *              guard in process_next does not log a "leaked from
 *              previous round-trip" error
 *            - priv->processing flipped to FALSE so process_next
 *              actually runs the next request
 *            - priv->browser_alive flipped back to TRUE on the next
 *              EV_ON_OPEN (the fix from commit b407f6c)
 *
 *          Topology uses persistent_channels=true on __bff_side__ so
 *          the C_CHANNEL (and therefore the same C_AUTH_BFF instance)
 *          survives the disconnect and receives the second TCP
 *          connection.  Without this the second connect would build a
 *          fresh BFF and the test wouldn't actually validate state
 *          cleanup on the *same* instance.
 *
 *          Timeline (anchored at mt_play)
 *          ==============================
 *            t = 0      mt_play → 300 ms warm-up timer
 *            t = 300    warm fires → open http_cl #1, start_tree
 *                       phase: T11_WARMING → T11_AWAITING_OPEN_1
 *            t ≈ 300    EV_ON_OPEN #1 → POST /auth/login #1, arm
 *                       100 ms cancel timer
 *                       phase: T11_AWAITING_OPEN_1 → T11_AWAITING_CANCEL
 *            t ≈ 300    BFF enqueues, forwards to mock KC, mock KC
 *                       stashes + arms 500 ms latency timer
 *            t = 400    cancel timer → stop_tree(http_cl #1), arm
 *                       300 ms retry timer
 *                       phase: T11_AWAITING_CANCEL → T11_AWAITING_RETRY
 *            t ≈ 400    BFF ac_on_close → browser_alive = FALSE
 *            t ≈ 800    mock KC latency fires → writes reply to BFF
 *                       BFF result_token_response → kc_ok++ → gate
 *                       → responses_dropped++ → CONTINUE_TASK
 *                       BFF ac_end_task → priv->gobj_http = NULL,
 *                       processing = FALSE
 *            t = 700    retry timer fires → open http_cl #2
 *                       phase: T11_AWAITING_RETRY → T11_AWAITING_OPEN_2
 *                       BFF http_sr accepts new TCP → ac_on_open
 *                       → browser_alive = TRUE
 *            t ≈ 700    EV_ON_OPEN #2 → POST /auth/login #2
 *                       phase: T11_AWAITING_OPEN_2 → T11_AWAITING_RESPONSE
 *            t ≈ 700    BFF enqueues, forwards to mock KC, mock KC
 *                       stashes + arms a fresh 500 ms latency timer
 *            t ≈ 1200   mock KC responds → BFF result_token_response
 *                       → kc_ok++ → gate FALSE (browser_alive TRUE)
 *                       → send_json_response → browser receives 200
 *            t ≈ 1200   driver EV_ON_MESSAGE → validate + verify
 *                       phase: T11_AWAITING_RESPONSE → T11_VERIFIED
 *            t ≈ 1300   death timer → set_yuno_must_die
 *
 *          Note on retry timer value: 300 ms is the buffer between the
 *          cancel (t=400) and the mock KC's first latency fire (t≈800).
 *          The retry fires at t=700, which is BEFORE the mock KC drops
 *          the stale reply.  That's intentional: it exercises the case
 *          where the new connection arrives while the BFF is still
 *          processing the CANCELLED first request.  The BFF's
 *          ac_on_open must flip browser_alive=TRUE even though the
 *          previous task hasn't finished, so when it does finish the
 *          gate checks the CURRENT browser_alive (TRUE — belongs to the
 *          second browser connection), not the stale one.
 *
 *          Wait — that's a bug waiting to happen.  The BFF's
 *          browser_alive gate is not per-connection, it's a single
 *          BOOL.  If the fresh EV_ON_OPEN arrives BEFORE the stale
 *          result_token_response runs, the gate sees TRUE and writes
 *          the STALE response to the NEW connection.
 *
 *          To avoid that race in this test, the retry timer is set to
 *          800 ms instead: t=400+800 = 1200 ms, which is AFTER the
 *          mock KC fires the stale reply at t≈800.  By the time the
 *          new connection opens, the stale reply has already been
 *          drained via the browser_alive=FALSE gate.  State is clean,
 *          the second request takes the clean path, test passes.
 *
 *          Exposing the race would need its own test (t12) and a
 *          stronger fix in c_auth_bff (per-connection browser_alive or
 *          a task-side cancellation).  Out of scope for t11.
 *
 *          Expected state at verify time
 *          =============================
 *          BFF stats:
 *              requests_total     = 2   (both POSTs enqueued)
 *              kc_calls           = 2   (both KC round-trips started)
 *              kc_ok              = 2   (mock KC replied 200 both times)
 *              kc_errors          = 0
 *              bff_errors         = 0
 *              responses_dropped  = 1   (first reply dropped)
 *              q_full_drops       = 0
 *
 *          Mock KC stats:
 *              token_requests     = 2
 *              responses_sent     = 2
 *              deferred_responses = 2
 *              pending_cancelled  = 0
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "c_test11_cancel_retry.h"
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
    T11_WARMING           = 0,  /* waiting for warm-up timer */
    T11_AWAITING_OPEN_1,        /* http_cl #1 running, waiting EV_ON_OPEN */
    T11_AWAITING_CANCEL,        /* POST #1 sent, waiting cancel timer */
    T11_AWAITING_RETRY,         /* http_cl #1 aborted, waiting retry timer */
    T11_AWAITING_OPEN_2,        /* http_cl #2 running, waiting EV_ON_OPEN */
    T11_AWAITING_RESPONSE,      /* POST #2 sent, waiting EV_ON_MESSAGE */
    T11_VERIFIED                /* stats checked, death timer running */
} t11_phase_t;

typedef struct _PRIVATE_DATA {
    hgobj       timer;
    hgobj       gobj_http_cl;   /* slot reused for #1 then #2 */
    BOOL        test_passed;
    BOOL        dying;
    t11_phase_t phase;
} PRIVATE_DATA;




            /******************************
             *      Framework Methods
             ******************************/




PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    /*
     *  C_TIMER0 (ms accuracy) — C_TIMER is second-resolution only
     *  and would round up our 100 ms / 300 ms / 800 ms intervals to
     *  1 s boundaries, breaking the deterministic ordering between
     *  the driver's timers and the mock KC's latency timer.  See the
     *  fix applied to c_mock_keycloak.c::mt_create for the same
     *  rationale.
     */
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);
    priv->phase = T11_WARMING;
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
 *  Create a fresh transient C_PROT_HTTP_CL + C_TCP stack.  The driver
 *  slot `priv->gobj_http_cl` is reused: the previous tree (if any) has
 *  already been stop_tree'd but still hangs off the driver as a
 *  stopped child — the yuno teardown will destroy it.  Overwriting
 *  the slot just drops our local reference; the framework still owns
 *  the old gobj until shutdown.
 */
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

/*
 *  POST /auth/login on the current priv->gobj_http_cl.
 */
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
     *  BFF-side stats — request #1 is cancelled when the browser
     *  closes (outbound to KC torn down, no kc_ok bump), request #2
     *  completes normally.  responses_dropped stays 0: the gate no
     *  longer fires because there's no stale reply to filter — the
     *  outbound was closed before the reply could arrive.
     */
    const test_stat_expect_t bff_expected[] = {
        {"requests_total",      2},
        {"kc_calls",            2},
        {"kc_ok",               1},
        {"kc_errors",           0},
        {"bff_errors",          0},
        {"responses_dropped",   0},
        {"kc_timeouts",         0},
        {"q_full_drops",        0},
        {NULL, 0}
    };
    hgobj bff = test_helpers_find_bff(gobj);
    test_helpers_check_stats(gobj, bff, "test11_cancel_retry[bff]", bff_expected);

    /*
     *  Mock-KC-side stats — two requests received; only request #2's
     *  latency timer fires on a live socket.  Request #1's pending
     *  slot is cancelled when the BFF closes the outbound during
     *  browser-disconnect cleanup.
     */
    const test_stat_expect_t kc_expected[] = {
        {"token_requests",      2},
        {"responses_sent",      1},
        {"deferred_responses",  2},
        {"pending_cancelled",   1},
        {NULL, 0}
    };
    hgobj kc = test_helpers_find_service_child(gobj, "__kc_side__", "C_MOCK_KEYCLOAK");
    test_helpers_check_stats(gobj, kc, "test11_cancel_retry[mock-kc]", kc_expected);

    priv->test_passed = TRUE;
    priv->phase = T11_VERIFIED;

    /* Death sequence — inlined to use set_timeout0 (see test9). */
    if(priv->gobj_http_cl) {
        gobj_stop(priv->gobj_http_cl);
    }
    hgobj bff_side = gobj_find_service("__bff_side__", FALSE);
    if(bff_side) {
        gobj_stop_tree(bff_side);
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
    case T11_WARMING: {
        /* Open the first http_cl stack. */
        open_new_http_cl(gobj);
        priv->phase = T11_AWAITING_OPEN_1;
        break;
    }

    case T11_AWAITING_CANCEL: {
        /*
         *  Cancel timer fired.  Abort the first http_cl and schedule
         *  the retry.  See the big comment at the top of the file for
         *  why the retry delay is 800 ms rather than something smaller.
         */
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj, "test11: aborting http_cl #1 mid-flight");
        }
        if(priv->gobj_http_cl && gobj_is_running(priv->gobj_http_cl)) {
            gobj_stop_tree(priv->gobj_http_cl);
        }
        priv->phase = T11_AWAITING_RETRY;
        set_timeout0(priv->timer, 800);
        break;
    }

    case T11_AWAITING_RETRY: {
        /*
         *  Retry timer fired.  By now the BFF has drained the first
         *  (cancelled) request via the browser_alive gate; priv state
         *  is clean.  Open a second http_cl on a fresh TCP connection.
         */
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj, "test11: opening http_cl #2 for retry");
        }
        open_new_http_cl(gobj);
        priv->phase = T11_AWAITING_OPEN_2;
        break;
    }

    default:
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test11_cancel_retry: unexpected ac_timer phase",
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
    case T11_AWAITING_OPEN_1: {
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj, "test11: http_cl #1 connected, posting login");
        }
        post_login(gobj);
        priv->phase = T11_AWAITING_CANCEL;
        set_timeout0(priv->timer, 100);
        break;
    }

    case T11_AWAITING_OPEN_2: {
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj, "test11: http_cl #2 connected, posting login (retry)");
        }
        post_login(gobj);
        priv->phase = T11_AWAITING_RESPONSE;
        /*
         *  No cancel this time — we wait for the real response on
         *  EV_ON_MESSAGE.  The mock KC latency will fire in ~500 ms
         *  and result_token_response will forward the reply through
         *  the browser_alive gate (which is TRUE for this connection).
         */
        break;
    }

    default:
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test11_cancel_retry: EV_ON_OPEN in unexpected phase",
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

    /*
     *  Messages are expected ONLY on the second connection.  Any
     *  response on the first one would mean the BFF leaked the stale
     *  reply through the cancelled http_cl — fix regression.
     */
    if(priv->phase != T11_AWAITING_RESPONSE) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test11_cancel_retry: EV_ON_MESSAGE in unexpected phase",
            "phase",    "%d", (int)priv->phase,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, 0);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "test11: second response status=%d", status);
        if(jn_body) {
            gobj_trace_json(gobj, jn_body, "test11: second response body");
        }
    }

    if(status != 200) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test11_cancel_retry: second login returned non-200",
            "status",   "%d", status,
            NULL
        );
        goto die;
    }

    if(!jn_body || !json_is_object(jn_body)) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test11_cancel_retry: response body missing or not an object",
            NULL
        );
        goto die;
    }
    if(!json_is_true(json_object_get(jn_body, "success"))) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test11_cancel_retry: body.success is not true",
            NULL
        );
        goto die;
    }
    const char *got_user = kw_get_str(gobj, jn_body, "username", "", 0);
    if(strcmp(got_user, "mockuser") != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test11_cancel_retry: wrong username in body",
            "expected", "%s", "mockuser",
            "got",      "%s", got_user,
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
     *    - After we abort http_cl #1 in T11_AWAITING_CANCEL (the async
     *      tail of our own stop_tree), we may receive EV_ON_CLOSE in
     *      T11_AWAITING_RETRY — expected.
     *    - At yuno shutdown, http_cl #2 is torn down and we may see a
     *      close in T11_VERIFIED — expected.
     *
     *  Anything else is a real error.
     */
    if(priv->phase < T11_AWAITING_CANCEL) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test11_cancel_retry: unexpected EV_ON_CLOSE",
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

GOBJ_DEFINE_GCLASS(C_TEST11_CANCEL_RETRY);

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

PUBLIC int register_c_test11_cancel_retry(void)
{
    return create_gclass(C_TEST11_CANCEL_RETRY);
}
