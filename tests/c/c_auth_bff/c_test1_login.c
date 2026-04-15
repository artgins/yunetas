/****************************************************************************
 *          C_TEST1_LOGIN.C
 *
 *          Driver for test1_login (happy path POST /auth/login).
 *
 *          Sequence
 *          ========
 *            mt_play
 *              → set a 500 ms timer (give the listen stacks time to
 *                open their sockets)
 *
 *            EV_TIMEOUT
 *              → create a transient C_PROT_HTTP_CL + C_TCP stack pointing
 *                at http://127.0.0.1:BFF_PORT/
 *              → subscribe to its EV_ON_OPEN / EV_ON_MESSAGE / EV_ON_CLOSE
 *              → gobj_start_tree(http_cl)  (initiates the TCP connect)
 *
 *            EV_ON_OPEN   (TCP/HTTP connected to the BFF)
 *              → POST /auth/login  { "username": ..., "password": ... }
 *
 *            EV_ON_MESSAGE (BFF responded)
 *              → expect status 200
 *              → expect body.success == true
 *              → expect body.username == "mockuser"  (set on the mock KC)
 *              → expect body.email    == "mockuser@example.com"
 *              → cross-check BFF's gobj_stats:
 *                  requests_total == 1
 *                  kc_calls       == 1
 *                  kc_ok          == 1
 *                  kc_errors      == 0
 *                  bff_errors     == 0
 *              → set_yuno_must_die
 *
 *          Any deviation fires a gobj_log_error, which the self-contained
 *          test harness (set_expected_results + test_json in main) treats
 *          as a test failure.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "c_test1_login.h"

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-------------flag----default-description*/
SDATA (DTP_STRING,    "bff_url",       SDF_RD, "http://127.0.0.1:18801/", "BFF URL"),
SDATA (DTP_POINTER,   "user_data",     0,      0,      "user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"messages",    "Trace test driver flow"},
    {0, 0}
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj   timer;
    hgobj   gobj_http_cl;   /* transient HTTP client → BFF */
    BOOL    test_passed;
    BOOL    dying;          /* TRUE once a death-timer is armed */
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 *
 *  Safety net: if the test exited via a path that left the transient
 *  http_cl tree running (e.g. failure before ac_on_message), stop it here
 *  so the framework doesn't walk into "Destroying a RUNNING gobj" while
 *  shutting the yuno down.
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);

    if(priv->gobj_http_cl && gobj_is_running(priv->gobj_http_cl)) {
        gobj_stop_tree(priv->gobj_http_cl);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /* Give the __bff_side__ and __kc_side__ listen sockets a moment */
    set_timeout(priv->timer, 500);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    return 0;
}




            /***************************
             *      Helpers
             ***************************/




/***************************************************************************
 *  Cross-check a subset of C_AUTH_BFF's stats counters.  Each mismatch
 *  logs a gobj_log_error and the test will fail at cleaning() time.
 ***************************************************************************/
PRIVATE void check_bff_stats(hgobj gobj, hgobj bff)
{
    json_t *resp = gobj_stats(bff, NULL, 0, gobj);
    if(!resp) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test1_login: gobj_stats(bff) returned NULL",
            NULL
        );
        return;
    }
    json_t *jn_data = kw_get_dict(gobj, resp, "data", NULL, 0);
    if(!jn_data) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test1_login: stats response has no 'data' dict",
            NULL
        );
        JSON_DECREF(resp)
        return;
    }

    struct {
        const char *name;
        json_int_t  expected;
    } cases[] = {
        {"requests_total", 1},
        {"kc_calls",       1},
        {"kc_ok",          1},
        {"kc_errors",      0},
        {"bff_errors",     0},
        {"q_full_drops",   0},
        {NULL, 0}
    };

    for(int i = 0; cases[i].name; i++) {
        json_int_t got = json_integer_value(
            json_object_get(jn_data, cases[i].name));
        if(got != cases[i].expected) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_APP_ERROR,
                "msg",      "%s", "test1_login: BFF stat counter mismatch",
                "stat",     "%s", cases[i].name,
                "expected", "%lld", (long long)cases[i].expected,
                "got",      "%lld", (long long)got,
                NULL
            );
        }
    }

    JSON_DECREF(resp)
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Timer fired: two roles, distinguished by priv->dying.
 *
 *  Initial role (dying == FALSE)
 *      Open the transient HTTP client stack and start it.
 *
 *  Death role (dying == TRUE)
 *      The 100 ms grace period after stopping the http_cl has elapsed,
 *      so the io_uring close events have had time to propagate to the
 *      BFF and mock-KC server sides.  Now we can safely die.
 ***************************************************************************/
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

/***************************************************************************
 *  Client connected: POST /auth/login.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "test1: connected to BFF, posting /auth/login");
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

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Response arrived from the BFF: validate it, cross-check stats, die.
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj bff_side = NULL;

    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, 0);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "test1: BFF response status=%d", status);
        if(jn_body) {
            gobj_trace_json(gobj, jn_body, "test1: BFF response body");
        }
    }

    /* Status check */
    if(status != 200) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test1_login: BFF returned non-200",
            "status",   "%d", status,
            NULL
        );
        goto end;
    }

    /* Body shape: must have success=true and the mock-KC-supplied claims */
    if(!jn_body || !json_is_object(jn_body)) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test1_login: BFF response body missing or not an object",
            NULL
        );
        goto end;
    }
    if(!json_is_true(json_object_get(jn_body, "success"))) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test1_login: body.success is not true",
            NULL
        );
        goto end;
    }
    const char *got_user = kw_get_str(gobj, jn_body, "username", "", 0);
    const char *got_mail = kw_get_str(gobj, jn_body, "email",    "", 0);
    if(strcmp(got_user, "mockuser") != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test1_login: wrong username in body",
            "expected", "%s", "mockuser",
            "got",      "%s", got_user,
            NULL
        );
        goto end;
    }
    if(strcmp(got_mail, "mockuser@example.com") != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test1_login: wrong email in body",
            "expected", "%s", "mockuser@example.com",
            "got",      "%s", got_mail,
            NULL
        );
        goto end;
    }

    /*
     *  Cross-check BFF stats.  Locate the C_AUTH_BFF instance by walking
     *  __bff_side__ for the first child of gclass C_AUTH_BFF — same
     *  filter the C_AUTH_BFF_YUNO::view-bff-status aggregator uses.
     */
    bff_side = gobj_find_service("__bff_side__", FALSE);
    if(bff_side) {
        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", "C_AUTH_BFF"
        );
        json_t *dl = gobj_match_children_tree(bff_side, jn_filter);
        if(dl && json_array_size(dl) > 0) {
            hgobj bff = (hgobj)(size_t)json_integer_value(
                json_array_get(dl, 0));
            check_bff_stats(gobj, bff);
        } else {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_APP_ERROR,
                "msg",      "%s", "test1_login: no C_AUTH_BFF found under __bff_side__",
                NULL
            );
        }
        gobj_free_iter(dl);
    } else {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test1_login: __bff_side__ service not found",
            NULL
        );
    }

    priv->test_passed = TRUE;

end:
    KW_DECREF(kw)

    /*
     *  Stop the transient http_cl + c_tcp stack now (while the io_uring
     *  loop is still spinning) so c_tcp::mt_stop's try_to_stop_yevents()
     *  can cancel any in-flight reads/writes.  Don't gobj_destroy here:
     *  the framework will destroy this child during yuno teardown.
     *
     *  Then arm a 100 ms death-timer instead of dying immediately, so
     *  the io_uring close events have time to propagate from the test
     *  client to the BFF and mock-KC server-side prot_http_sr stacks.
     *  Otherwise their c_tcps would still be RUNNING when the framework
     *  destroys them and we'd get "Destroying a RUNNING gobj" errors.
     */
    if(priv->gobj_http_cl) {
        gobj_stop(priv->gobj_http_cl);
    }
    if(bff_side) {
        gobj_stop_tree(bff_side);
    }

    priv->dying = TRUE;
    set_timeout(priv->timer, 100);
    return 0;
}

/***************************************************************************
 *  Connection closed.  If test_passed is still FALSE, something went
 *  wrong before we got a response.
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->test_passed) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP_ERROR,
            "msg",      "%s", "test1_login: connection closed before success",
            NULL
        );

        if(priv->gobj_http_cl) {
            gobj_stop_tree(priv->gobj_http_cl);
        }

        priv->dying = TRUE;
        set_timeout(priv->timer, 100);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Volatil child stopped: destroy it.
 ***************************************************************************/
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
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST1_LOGIN);

/***************************************************************************
 *
 ***************************************************************************/
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
        0,              // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,              // authz_table
        0,              // command_table
        s_user_trace_level,
        0               // gcflag_t
    );
    if(!__gclass__) {
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test1_login(void)
{
    return create_gclass(C_TEST1_LOGIN);
}
