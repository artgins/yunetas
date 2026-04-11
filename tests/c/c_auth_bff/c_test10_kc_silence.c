/****************************************************************************
 *          C_TEST10_KC_SILENCE.C
 *
 *          Driver for test10_kc_silence.
 *
 *          Reproduces the "Keycloak hangs forever" failure mode that
 *          would otherwise wedge an entire BFF channel, and validates
 *          the kc_timeout_ms / ac_kc_watchdog fix in the previous
 *          commit.
 *
 *          Setup
 *          =====
 *          - Mock Keycloak: latency_ms = 60000 (60 s).  Well beyond
 *            the test's auto_kill_time = 10 s — effectively "never
 *            responds" from the test's point of view.  The mock will
 *            sit with a pending slot and a live C_TIMER0 until
 *            shutdown; mt_stop of the mock drains both on teardown.
 *          - BFF: kc_timeout_ms = 500.  Short enough that the test
 *            runs in ~1 s, long enough that the POST and the BFF's
 *            internal plumbing settle before the watchdog fires.
 *
 *          Timeline
 *          ========
 *            t = 0      mt_play → 300 ms warm timer
 *            t = 300    warm fires → create http_cl, start_tree
 *                       phase: T10_INIT → T10_CONNECTING
 *            t ≈ 300    EV_ON_OPEN → POST /auth/login
 *                       phase: T10_CONNECTING → T10_POSTED
 *            t ≈ 300    BFF enqueues, forwards to mock KC
 *                       Mock KC arms a 60 s latency timer (that will
 *                       never fire within the test)
 *                       BFF arms a 500 ms kc_watchdog
 *            t ≈ 800    BFF kc_watchdog fires → ac_kc_watchdog
 *                         - st_kc_timeouts++
 *                         - send_error_response 504 to browser
 *                         - gobj_send_event EV_END_TASK to self
 *                       ac_end_task drains: stops priv->gobj_http
 *                       (which closes the BFF's outbound to mock KC),
 *                       clears active_browser_src, processing=FALSE
 *            t ≈ 800    Mock KC ac_on_close fires → cancels its own
 *                       pending 60 s timer, pending_cancelled++
 *            t ≈ 800    Driver EV_ON_MESSAGE with the BFF's 504 reply
 *                       phase: T10_POSTED → T10_VERIFIED
 *            t ≈ 900    death timer → set_yuno_must_die
 *
 *          Expected state at verify time
 *          =============================
 *          BFF:
 *              requests_total     = 1
 *              kc_calls           = 1   (outbound round-trip started)
 *              kc_ok              = 0   (KC never replied)
 *              kc_errors          = 0   (no non-2xx from KC — it
 *                                        went silent, not errored)
 *              bff_errors         = 1   (the 504 through
 *                                        send_error_response)
 *              responses_dropped  = 0   (browser_alive at 504 time)
 *              kc_timeouts        = 1   (THE signal the fix worked)
 *              q_full_drops       = 0
 *
 *          Mock KC:
 *              token_requests     = 1
 *              responses_sent     = 0   (latency timer never fired)
 *              deferred_responses = 1
 *              pending_cancelled  = 1   (BFF closed the outbound
 *                                        while the slot was pending)
 *
 *          Failure without the fix
 *          =======================
 *          Without ac_kc_watchdog / kc_timeout_ms, the POST would
 *          hang forever.  The test's auto_kill_time=10 s would fire
 *          and the yuno would shut down, but ctest would still treat
 *          that as a pass (yuno shut down cleanly, no unexpected
 *          errors logged).  So this test ALSO asserts a concrete
 *          observable signal: the 504 response MUST reach the
 *          driver within the test window, otherwise ac_on_message
 *          never runs, priv->test_passed stays FALSE, and the
 *          shutdown-time ac_on_close handler flags an error.
 *          That turns "silent hang" into a hard test failure.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "c_test10_kc_silence.h"
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
    T10_INIT          = 0,  /* waiting for warm-up timer */
    T10_CONNECTING,         /* http_cl started, waiting EV_ON_OPEN */
    T10_POSTED,             /* POST sent, waiting EV_ON_MESSAGE (504) */
    T10_VERIFIED            /* stats checked, death timer running */
} t10_phase_t;

typedef struct _PRIVATE_DATA {
    hgobj       timer;
    hgobj       gobj_http_cl;
    BOOL        test_passed;
    BOOL        dying;
    t10_phase_t phase;
} PRIVATE_DATA;




            /******************************
             *      Framework Methods
             ******************************/




PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    /* C_TIMER0 — millisecond precision; see test9 rationale. */
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);
    priv->phase = T10_INIT;
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
             *      Verification
             ***************************/




PRIVATE void verify_and_die(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const test_stat_expect_t bff_expected[] = {
        {"requests_total",      1},
        {"kc_calls",            1},
        {"kc_ok",               0},
        {"kc_errors",           0},
        {"bff_errors",          1},
        {"responses_dropped",   0},
        {"kc_timeouts",         1},
        {"q_full_drops",        0},
        {NULL, 0}
    };
    hgobj bff = test_helpers_find_bff(gobj);
    test_helpers_check_stats(gobj, bff, "test10_kc_silence[bff]", bff_expected);

    /*
     *  Mock KC: 1 request received, but the latency timer never fires
     *  within the test window (60 s vs our ~1 s).  Instead the BFF
     *  closes its outbound connection during ac_kc_watchdog →
     *  ac_end_task, and the mock KC's ac_on_close cancels the pending
     *  slot and bumps pending_cancelled.
     */
    const test_stat_expect_t kc_expected[] = {
        {"token_requests",      1},
        {"responses_sent",      0},
        {"deferred_responses",  1},
        {"pending_cancelled",   1},
        {NULL, 0}
    };
    hgobj kc = test_helpers_find_service_child(gobj, "__kc_side__", "C_MOCK_KEYCLOAK");
    test_helpers_check_stats(gobj, kc, "test10_kc_silence[mock-kc]", kc_expected);

    priv->test_passed = TRUE;
    priv->phase = T10_VERIFIED;

    /* Death sequence — same inline pattern as test9 (C_TIMER0). */
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
    case T10_INIT: {
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
        priv->phase = T10_CONNECTING;
        break;
    }

    default:
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test10_kc_silence: unexpected ac_timer phase",
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

    if(priv->phase != T10_CONNECTING) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test10_kc_silence: EV_ON_OPEN in unexpected phase",
            "phase",    "%d", (int)priv->phase,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "test10: connected to BFF, posting /auth/login (KC will go silent)");
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

    priv->phase = T10_POSTED;

    /*
     *  No local timer from here on — we expect the BFF's kc_watchdog
     *  to fire within ~500 ms and deliver a 504.  auto_kill_time=10 s
     *  is the outer safety net.
     */

    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase != T10_POSTED) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test10_kc_silence: EV_ON_MESSAGE in unexpected phase",
            "phase",    "%d", (int)priv->phase,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, 0);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "test10: BFF response status=%d (expecting 504)", status);
        if(jn_body) {
            gobj_trace_json(gobj, jn_body, "test10: BFF response body");
        }
    }

    if(status != 504) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test10_kc_silence: expected BFF 504, got different status",
            "expected", "%d", 504,
            "status",   "%d", status,
            NULL
        );
        goto end;
    }

    if(!jn_body || !json_is_object(jn_body)) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test10_kc_silence: BFF error body missing or not an object",
            NULL
        );
        goto end;
    }
    if(json_is_true(json_object_get(jn_body, "success"))) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test10_kc_silence: body.success must be false",
            NULL
        );
        goto end;
    }
    const char *got_error = kw_get_str(gobj, jn_body, "error", "", 0);
    if(empty_string(got_error)) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test10_kc_silence: body.error empty",
            NULL
        );
        goto end;
    }

    verify_and_die(gobj);
    JSON_DECREF(kw)
    return 0;

end:
    KW_DECREF(kw)
    priv->dying = TRUE;
    set_timeout0(priv->timer, 100);
    return 0;
}

PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  A close in T10_POSTED or later is the async tail of our own
     *  stop_tree after verify_and_die.  A close earlier would mean
     *  the BFF tore the connection down before the 504 body reached
     *  us, which is a real error — flag it so test_passed is not
     *  silently set by a close without a message.
     */
    if(priv->phase == T10_CONNECTING) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test10_kc_silence: unexpected EV_ON_CLOSE in T10_CONNECTING",
            NULL
        );
    } else if(priv->phase == T10_POSTED && !priv->test_passed) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test10_kc_silence: connection closed without 504 reply",
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

GOBJ_DEFINE_GCLASS(C_TEST10_KC_SILENCE);

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

PUBLIC int register_c_test10_kc_silence(void)
{
    return create_gclass(C_TEST10_KC_SILENCE);
}
