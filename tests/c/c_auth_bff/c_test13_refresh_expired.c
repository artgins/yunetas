/****************************************************************************
 *          C_TEST13_REFRESH_EXPIRED.C
 *
 *          Driver for test13_refresh_expired.
 *
 *          Sanity check for the action-aware Keycloak error mapping in
 *          c_auth_bff.c::result_kc_login.  The /auth/refresh path must
 *          NOT report "invalid_credentials" when Keycloak rejects the
 *          refresh_token — the user did not type anything wrong, their
 *          session just timed out.  The BFF must instead return error_code
 *          "session_expired" so the GUI can render "please log in again"
 *          instead of "wrong username or password".
 *
 *          Topology mirrors c_test4_refresh: warm-up timer → transient
 *          C_PROT_HTTP_CL → POST /auth/refresh with Cookie header → assert
 *          the BFF response → cross-check stats → graceful death.
 *
 *          The mock Keycloak is configured with return_status=401 in
 *          main_test13_refresh_expired.c, so the /token endpoint answers
 *          with an RFC-6749 invalid_grant envelope for every request.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "c_test13_refresh_expired.h"
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

typedef struct _PRIVATE_DATA {
    hgobj   timer;
    hgobj   gobj_http_cl;
    BOOL    test_passed;
    BOOL    dying;
} PRIVATE_DATA;




            /******************************
             *      Framework Methods
             ******************************/




PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
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
    set_timeout(priv->timer, 500);
    return 0;
}

PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    clear_timeout(priv->timer);
    return 0;
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

    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj,
            "test13: connected to BFF, posting /auth/refresh "
            "(expect KC 401 → BFF 401 session_expired)");
    }

    /*
     *  The BFF reads the refresh token from the Cookie header, not the
     *  body.  The value is meaningless to the mock — what matters is
     *  that the BFF forwards the request to Keycloak, which is scripted
     *  to return 401 invalid_grant.
     */
    json_t *jn_headers = json_pack("{s:s, s:s}",
        "Origin", "http://localhost",
        "Cookie", "refresh_token=fake-refresh-token-for-test-13"
    );
    json_t *jn_data = json_object();   /* {} */
    json_t *query = json_pack("{s:s, s:s, s:o, s:o}",
        "method",   "POST",
        "resource", "/auth/refresh",
        "headers",  jn_headers,
        "data",     jn_data
    );
    gobj_send_event(priv->gobj_http_cl, EV_SEND_MESSAGE, query, gobj);

    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, 0);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "test13: BFF response status=%d", status);
        if(jn_body) {
            gobj_trace_json(gobj, jn_body, "test13: BFF response body");
        }
    }

    /*
     *  A refresh that fails with Keycloak's invalid_grant must surface
     *  as HTTP 401 session_expired — NOT 400 and NOT invalid_credentials.
     *  The 401 is the same status as test2_kc_401 (login with bad
     *  password), but the error_code must differ to let the GUI show
     *  "please log in again" instead of "wrong credentials".
     */
    if(status != 401) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test13_refresh_expired: expected BFF 401, got different status",
            "expected", "%d", 401,
            "status",   "%d", status,
            NULL
        );
        goto end;
    }

    if(!jn_body || !json_is_object(jn_body)) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test13_refresh_expired: BFF error body missing or not an object",
            NULL
        );
        goto end;
    }
    if(json_is_true(json_object_get(jn_body, "success"))) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test13_refresh_expired: body.success must be false",
            NULL
        );
        goto end;
    }
    const char *got_code = kw_get_str(gobj, jn_body, "error_code", "", 0);
    if(strcmp(got_code, "session_expired") != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test13_refresh_expired: body.error_code mismatch",
            "expected", "%s", "session_expired",
            "got",      "%s", got_code,
            NULL
        );
        goto end;
    }
    /*
     *  Explicitly make sure the refresh-path failure is NOT being
     *  misreported as a bad-credentials error from the login path.
     *  This is the regression guard for the action-aware mapping.
     */
    if(strcmp(got_code, "invalid_credentials") == 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test13_refresh_expired: refresh wrongly mapped to invalid_credentials",
            NULL
        );
        goto end;
    }
    const char *got_error = kw_get_str(gobj, jn_body, "error", "", 0);
    if(empty_string(got_error)) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test13_refresh_expired: body.error fallback empty",
            NULL
        );
        goto end;
    }

    /*
     *  Stats: one KC round-trip, it failed, so we should see
     *  kc_calls=1, kc_ok=0, kc_errors=1, bff_errors=1 — same shape
     *  as test2_kc_401 (the difference is only which error_code
     *  went back to the browser).
     */
    const test_stat_expect_t expected[] = {
        {"requests_total", 1},
        {"idp_calls",       1},
        {"idp_ok",          0},
        {"idp_errors",      1},
        {"bff_errors",     1},
        {"q_full_drops",   0},
        {NULL, 0}
    };
    hgobj bff = test_helpers_find_bff(gobj);
    test_helpers_check_stats(gobj, bff, "test13_refresh_expired", expected);

    priv->test_passed = TRUE;

end:
    KW_DECREF(kw)
    TEST_HELPERS_BEGIN_DYING(priv);
    return 0;
}

PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->test_passed) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test13_refresh_expired: connection closed before success",
            NULL
        );
        TEST_HELPERS_BEGIN_DYING(priv);
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

GOBJ_DEFINE_GCLASS(C_TEST13_REFRESH_EXPIRED);

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

PUBLIC int register_c_test13_refresh_expired(void)
{
    return create_gclass(C_TEST13_REFRESH_EXPIRED);
}
