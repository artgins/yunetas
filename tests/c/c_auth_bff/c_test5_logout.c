/****************************************************************************
 *          C_TEST5_LOGOUT.C
 *
 *          Driver for test5_logout (happy path POST /auth/logout).
 *
 *          Mock Keycloak /logout returns 204 No Content (spec).  The
 *          BFF counts any 2xx as kc_ok (see the stats fix in the
 *          previous commit) and responds 200 to the browser with
 *          cleared Set-Cookie headers (Max-Age=0).
 *
 *          Response shape (see c_auth_bff.c::result_kc_logout):
 *              { "success": true }
 *
 *          No username/email/expires_in in the logout response.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "c_test5_logout.h"
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
        gobj_trace_msg(gobj, "test5: connected to BFF, posting /auth/logout with Cookie header");
    }

    json_t *jn_headers = json_pack("{s:s, s:s}",
        "Origin", "http://localhost",
        "Cookie", "refresh_token=fake-refresh-token-for-test-5"
    );
    json_t *jn_data = json_object();   /* empty body; /auth/logout reads the cookie */
    json_t *query = json_pack("{s:s, s:s, s:o, s:o}",
        "method",   "POST",
        "resource", "/auth/logout",
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
        gobj_trace_msg(gobj, "test5: BFF response status=%d", status);
        if(jn_body) {
            gobj_trace_json(gobj, jn_body, "test5: BFF response body");
        }
    }

    if(status != 200) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test5_logout: BFF returned non-200",
            "status",   "%d", status,
            NULL
        );
        goto end;
    }

    if(!jn_body || !json_is_object(jn_body)) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test5_logout: BFF response body missing or not an object",
            NULL
        );
        goto end;
    }
    if(!json_is_true(json_object_get(jn_body, "success"))) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test5_logout: body.success is not true",
            NULL
        );
        goto end;
    }

    /*
     *  /auth/logout: KC returns 204, BFF stats fix (previous commit)
     *  makes that count as kc_ok.  BFF itself returns 200 to the
     *  browser regardless (cookies cleared client-side).
     */
    const test_stat_expect_t expected[] = {
        {"requests_total", 1},
        {"kc_calls",       1},
        {"kc_ok",          1},
        {"kc_errors",      0},
        {"bff_errors",     0},
        {"q_full_drops",   0},
        {NULL, 0}
    };
    hgobj bff = test_helpers_find_bff(gobj);
    test_helpers_check_stats(gobj, bff, "test5_logout", expected);

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
            "msg",      "%s", "test5_logout: connection closed before success",
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

GOBJ_DEFINE_GCLASS(C_TEST5_LOGOUT);

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

PUBLIC int register_c_test5_logout(void)
{
    return create_gclass(C_TEST5_LOGOUT);
}
