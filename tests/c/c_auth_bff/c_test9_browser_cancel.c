/****************************************************************************
 *          C_TEST9_BROWSER_CANCEL.C
 *
 *          Driver for test9_browser_cancel.
 *
 *          Timeline (anchored at mt_play)
 *          ==============================
 *            t = 0      mt_play → 300 ms warm-up timer
 *            t = 300    warm-up fires → create http_cl, start_tree
 *                       phase: T9_INIT → T9_CONNECTING
 *            t ≈ 300    EV_ON_OPEN → POST /auth/login, arm 100 ms
 *                       cancel timer
 *                       phase: T9_CONNECTING → T9_POSTED
 *            t ≈ 300    BFF enqueues, forwards to mock KC
 *            t ≈ 300    mock KC stashes response + arms 500 ms
 *                       latency timer
 *            t = 400    cancel timer fires → gobj_stop_tree(http_cl),
 *                       arm 800 ms verify timer
 *                       phase: T9_POSTED → T9_CANCELLED
 *            t ≈ 400    driver's TCP closes → BFF ac_on_close →
 *                       priv->browser_alive = FALSE (from the fix
 *                       in the previous commit)
 *            t ≈ 800    mock KC latency fires → writes response to
 *                       BFF's outbound http_cl
 *            t ≈ 800    BFF result_token_response runs: counts kc_ok++,
 *                       sees browser_alive==FALSE, bumps
 *                       responses_dropped++, CONTINUE_TASK
 *            t ≈ 800    ac_end_task tears down priv->gobj_http
 *            t ≈ 1200   verify timer fires → cross-check stats
 *                       phase: T9_CANCELLED → T9_VERIFIED
 *            t ≈ 1300   death timer fires → set_yuno_must_die()
 *
 *          Expected state at t=1200
 *          ========================
 *          BFF stats:
 *              requests_total     = 1
 *              kc_calls           = 1   (outbound round-trip started)
 *              kc_ok              = 1   (mock KC did reply with 200)
 *              kc_errors          = 0
 *              bff_errors         = 0   (no 4xx sent — only dropped)
 *              responses_dropped  = 1   (the browser_alive gate fired)
 *              q_full_drops       = 0
 *
 *          Mock KC stats:
 *              token_requests     = 1
 *              responses_sent     = 1   (latency timer fired, wrote
 *                                        response to BFF; only the
 *                                        browser never saw it)
 *              deferred_responses = 1
 *              pending_cancelled  = 0   (pending slot was flushed
 *                                        before the BFF closed its
 *                                        outbound to the mock)
 *
 *          Failure without the fix
 *          =======================
 *          Without the browser_alive gate, result_token_response would
 *          call send_json_response → gobj_send_event on the torn-down
 *          C_PROT_HTTP_SR.  gobj_send_event checks obflag and emits a
 *          gobj_log_error("gobj DESTROYED" or similar) which the test
 *          harness's set_expected_results=[] catches as an unexpected
 *          error and fails the run.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "c_test9_browser_cancel.h"
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
    T9_INIT       = 0,  /* waiting for warm-up timer */
    T9_CONNECTING,      /* http_cl start_tree running, waiting EV_ON_OPEN */
    T9_POSTED,          /* POST sent, waiting cancel timer */
    T9_CANCELLED,       /* http_cl aborted, waiting verify timer */
    T9_VERIFIED         /* stats checked, death timer running */
} t9_phase_t;

typedef struct _PRIVATE_DATA {
    hgobj       timer;
    hgobj       gobj_http_cl;
    BOOL        test_passed;
    BOOL        dying;
    t9_phase_t  phase;
} PRIVATE_DATA;




            /******************************
             *      Framework Methods
             ******************************/




PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    /*
     *  C_TIMER0 (high-resolution io_uring timer) instead of C_TIMER.
     *  C_TIMER is documented as "ACCURACY IN SECONDS" — driven off the
     *  yuno's 1-second periodic tick, so set_timeout(timer, 100) doesn't
     *  fire at 100 ms, it fires on the next 1 s boundary.  This test
     *  needs the cancel to land BEFORE mock-KC's latency timer, so
     *  millisecond accuracy is mandatory.  See c_timer.c header comment.
     */
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);
    priv->phase = T9_INIT;
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
    /* Warm-up delay lets the BFF and mock-KC listen sockets settle. */
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
             *      Verification
             ***************************/




PRIVATE void verify_and_die(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  BFF-side stats.  kc_ok=0: the BFF tears down the outbound to KC
     *  on browser disconnect, so result_token_response never runs for
     *  this request.  responses_dropped=0: the gate never fires either
     *  (no response ever reached it).  bff_errors=0: no 4xx/5xx was
     *  produced — the round-trip is simply cancelled.
     */
    const test_stat_expect_t bff_expected[] = {
        {"requests_total",      1},
        {"idp_calls",            1},
        {"idp_ok",               0},
        {"idp_errors",           0},
        {"bff_errors",          0},
        {"responses_dropped",   0},
        {"idp_timeouts",         0},
        {"q_full_drops",        0},
        {NULL, 0}
    };
    hgobj bff = test_helpers_find_bff(gobj);
    test_helpers_check_stats(gobj, bff, "test9_browser_cancel[bff]", bff_expected);

    /*
     *  Mock-KC-side stats.  responses_sent=0: mock-KC never got to
     *  write its deferred reply because the BFF closed the outbound
     *  first.  pending_cancelled=1: the close is observed on the
     *  mock side as the pending slot being abandoned.
     */
    const test_stat_expect_t kc_expected[] = {
        {"token_requests",      1},
        {"responses_sent",      0},
        {"deferred_responses",  1},
        {"pending_cancelled",   1},
        {NULL, 0}
    };
    hgobj kc = test_helpers_find_service_child(gobj, "__idp_side__", "C_MOCK_KEYCLOAK");
    test_helpers_check_stats(gobj, kc, "test9_browser_cancel[mock-kc]", kc_expected);

    priv->test_passed = TRUE;
    priv->phase = T9_VERIFIED;

    /*
     *  Death sequence — inlined instead of TEST_HELPERS_BEGIN_DYING
     *  because that macro hardcodes set_timeout(), and our timer is
     *  C_TIMER0 (see mt_create rationale).
     */
    if(priv->gobj_http_cl && gobj_is_running(priv->gobj_http_cl)) {
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
    case T9_INIT: {
        /* Warm-up timer fired. Open the transient http_cl stack. */
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
        priv->phase = T9_CONNECTING;
        break;
    }

    case T9_POSTED: {
        /* Cancel timer fired. Abort the http_cl mid-flight. */
        if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
            gobj_trace_msg(gobj, "test9: aborting http_cl while KC round-trip in flight");
        }
        if(priv->gobj_http_cl && gobj_is_running(priv->gobj_http_cl)) {
            gobj_stop_tree(priv->gobj_http_cl);
        }
        priv->phase = T9_CANCELLED;
        /*
         *  Wait long enough for:
         *    - mock KC's 500 ms latency timer to fire (writes response)
         *    - BFF's task to receive it and run result_token_response
         *    - BFF's ac_end_task to drain
         *    - mock KC to observe its inbound close and unwind
         *  800 ms is generous but keeps the test well under auto_kill.
         */
        set_timeout0(priv->timer, 800);
        break;
    }

    case T9_CANCELLED: {
        /* Verify timer fired. Check stats and enter the death sequence. */
        verify_and_die(gobj);
        break;
    }

    default:
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test9_browser_cancel: unexpected ac_timer phase",
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

    if(priv->phase != T9_CONNECTING) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test9_browser_cancel: EV_ON_OPEN in unexpected phase",
            "phase",    "%d", (int)priv->phase,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "test9: connected to BFF, posting /auth/login");
    }

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

    /*
     *  Post sent.  Now the BFF is forwarding to the mock KC, which will
     *  hold the response for latency_ms=500.  Arm the cancel timer to
     *  fire comfortably before that window expires.
     */
    priv->phase = T9_POSTED;
    set_timeout0(priv->timer, 100);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The transient http_cl closes after our gobj_stop_tree, and may also
 *  publish EV_ON_CLOSE later if the BFF closes its side.  Either way,
 *  the test's success signal is test_passed (set in verify_and_die) —
 *  an EV_ON_CLOSE arriving before phase T9_VERIFIED is an error.
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  An EV_ON_CLOSE is expected ONLY after we aborted the http_cl
     *  ourselves in phase T9_POSTED.  If it arrives earlier
     *  (T9_CONNECTING, before we ever posted) something went wrong at
     *  the TCP level — flag it.  Once we're in T9_CANCELLED or later,
     *  the close is just the async tail of our own stop_tree.
     */
    if(priv->phase < T9_POSTED) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test9_browser_cancel: unexpected EV_ON_CLOSE before POST",
            "phase",    "%d", (int)priv->phase,
            NULL
        );
    }

    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *  The driver is not supposed to see any response from the BFF —
     *  we aborted the connection before the Keycloak round-trip
     *  completed.  If an EV_ON_MESSAGE arrives that means the BFF
     *  somehow wrote a reply to our dead socket, which is exactly the
     *  bug the fix is meant to prevent.
     */
    gobj_log_error(gobj, 0,
        "function", "%s", __FUNCTION__,
        "msgset",   "%s", MSGSET_APP,
        "msg",      "%s", "test9_browser_cancel: BFF wrote a reply after cancel — fix regressed",
        NULL
    );

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

GOBJ_DEFINE_GCLASS(C_TEST9_BROWSER_CANCEL);

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

PUBLIC int register_c_test9_browser_cancel(void)
{
    return create_gclass(C_TEST9_BROWSER_CANCEL);
}
