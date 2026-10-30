/****************************************************************************
 *          C_TEST21_BIG_TOKEN.C
 *
 *          Driver for test21_big_token: five POST /auth/login on one
 *          connection, each answered by the mock KC with another token.
 *          A browser keeps a cookie whose name and value take at most
 *          4096 bytes, so:
 *            1. an access_token of 4084 bytes (with "access_token", 4096)
 *               fits: same checks as test1_login, its claims read;
 *            2. one of 4085 bytes (4097) is refused by its size, 500
 *               token_too_large, the error saying its size;
 *            3. a refresh_token of 4084 bytes (with "refresh_token",
 *               4097) is refused the same way;
 *            4. one of 4083 bytes (4096) fits;
 *            5. a success with no access_token is refused, 502
 *               auth_unexpected_error.
 *          Each refusal has ONE log (the ERROR of the answer), and no
 *          WARNING that a payload could not be read (main_test21 captures
 *          the warnings too).
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
 *            EV_ON_MESSAGE (first answer)
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
 *              → set the mock KC's next token and POST /auth/login again
 *                on the same connection
 *
 *            EV_ON_MESSAGE (answers 2 to 5)
 *              → check it, set the next token and POST again; after the
 *                fifth, stop the stacks; set_yuno_must_die 100 ms later
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

#include "c_test21_big_token.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define COOKIE_MAX  4096    /* name + value of a cookie a browser keeps */
#define AT_FITS     (COOKIE_MAX - (int)sizeof("access_token") + 1)     /* 4084 */
#define RT_FITS     (COOKIE_MAX - (int)sizeof("refresh_token") + 1)    /* 4083 */

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-------------flag----default-description*/
SDATA (DTP_STRING,    "bff_url",       SDF_RD, "http://127.0.0.1:18880/", "BFF URL"),
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
    int     answers;        /* answers of the BFF received */
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

    /* Give the __bff_side__ and __idp_side__ listen sockets a moment */
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
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test21_big_token: gobj_stats(bff) returned NULL",
            NULL
        );
        return;
    }
    json_t *jn_data = kw_get_dict(gobj, resp, "data", NULL, 0);
    if(!jn_data) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test21_big_token: stats response has no 'data' dict",
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
        {"idp_calls",       1},
        {"idp_ok",          1},
        {"idp_errors",      0},
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
                "msgset",   "%s", MSGSET_APP,
                "msg",      "%s", "test21_big_token: BFF stat counter mismatch",
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
 *  POST /auth/login
 ***************************************************************************/
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

/***************************************************************************
 *  The mock KC of __idp_side__: its next answer has an access_token of
 *  `at_len` bytes and a refresh_token of `rt_len` (0: as it is made), or
 *  is `override_body` when not NULL (owned)
 ***************************************************************************/
PRIVATE void set_mock_answer(hgobj gobj, json_int_t at_len, json_int_t rt_len, json_t *override_body)
{
    hgobj idp_side = gobj_find_service("__idp_side__", FALSE);
    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", "C_MOCK_KEYCLOAK"
    );
    json_t *dl = idp_side? gobj_match_children_tree(idp_side, jn_filter) : NULL;
    if(!idp_side) {
        JSON_DECREF(jn_filter)
    }
    if(dl && json_array_size(dl) > 0) {
        hgobj mock = (hgobj)(size_t)json_integer_value(json_array_get(dl, 0));
        gobj_write_integer_attr(mock, "access_token_len", at_len);
        gobj_write_integer_attr(mock, "refresh_token_len", rt_len);
        if(override_body) {
            gobj_write_json_attr(mock, "override_body", override_body);
        }
    } else {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test21_big_token: no C_MOCK_KEYCLOAK found under __idp_side__",
            NULL
        );
    }
    gobj_free_iter(dl);
    JSON_DECREF(override_body)
}

/***************************************************************************
 *  Client connected: POST /auth/login.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_msg(gobj, "test1: connected to BFF, posting /auth/login");
    }

    post_login(gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  An answer that refuses: its status, its error_code, and a text its
 *  error must carry (NULL: not checked)
 ***************************************************************************/
PRIVATE void check_refusal(hgobj gobj, json_t *kw, int answer,
    int expected_status, const char *expected_code, const char *expected_text)
{
    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, 0);
    const char *error_code = jn_body? kw_get_str(gobj, jn_body, "error_code", "", 0) : "";
    const char *error = jn_body? kw_get_str(gobj, jn_body, "error", "", 0) : "";

    if(status != expected_status || strcmp(error_code, expected_code) != 0 ||
            (expected_text && !strstr(error, expected_text))) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "test21_big_token: an answer was not the refusal expected",
            "answer",       "%d", answer,
            "status",       "%d", status,
            "error_code",   "%s", error_code,
            "error",        "%s", error,
            "expected",     "%s", expected_code,
            NULL
        );
    }
}

/***************************************************************************
 *  An answer that must be a success
 ***************************************************************************/
PRIVATE void check_success(hgobj gobj, json_t *kw, int answer)
{
    int status = (int)kw_get_int(gobj, kw, "response_status_code", -1, 0);
    json_t *jn_body = kw_get_dict(gobj, kw, "body", NULL, 0);

    if(status != 200 || !jn_body || !json_is_true(json_object_get(jn_body, "success"))) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "test21_big_token: an answer was not a success",
            "answer",       "%d", answer,
            "status",       "%d", status,
            NULL
        );
    }
}

/***************************************************************************
 *  Response arrived from the BFF: validate it, cross-check stats, die.
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj bff_side = NULL;

    priv->answers++;
    switch(priv->answers) {
        case 1:
            break;
        case 2:
            check_refusal(gobj, kw, 2, 500, "token_too_large", "access_token 4085 bytes");
            set_mock_answer(gobj, 0, RT_FITS + 1, NULL);
            post_login(gobj);
            KW_DECREF(kw)
            return 0;
        case 3:
            check_refusal(gobj, kw, 3, 500, "token_too_large", "refresh_token 4084 bytes");
            set_mock_answer(gobj, 0, RT_FITS, NULL);
            post_login(gobj);
            KW_DECREF(kw)
            return 0;
        case 4:
            check_success(gobj, kw, 4);
            set_mock_answer(gobj, 0, 0, json_pack("{s:s, s:i}",
                "token_type",   "Bearer",
                "expires_in",   300
            ));
            post_login(gobj);
            KW_DECREF(kw)
            return 0;
        default:
            check_refusal(gobj, kw, priv->answers, 502, "auth_unexpected_error", NULL);
            bff_side = gobj_find_service("__bff_side__", FALSE);
            goto end;
    }

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
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test21_big_token: BFF returned non-200",
            "status",   "%d", status,
            NULL
        );
        goto end;
    }

    /* Body shape: must have success=true and the mock-KC-supplied claims */
    if(!jn_body || !json_is_object(jn_body)) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test21_big_token: BFF response body missing or not an object",
            NULL
        );
        goto end;
    }
    if(!json_is_true(json_object_get(jn_body, "success"))) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test21_big_token: body.success is not true",
            NULL
        );
        goto end;
    }
    const char *got_user = kw_get_str(gobj, jn_body, "username", "", 0);
    const char *got_mail = kw_get_str(gobj, jn_body, "email",    "", 0);
    if(strcmp(got_user, "mockuser") != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test21_big_token: wrong username in body",
            "expected", "%s", "mockuser",
            "got",      "%s", got_user,
            NULL
        );
        goto end;
    }
    if(strcmp(got_mail, "mockuser@example.com") != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test21_big_token: wrong email in body",
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
                "msgset",   "%s", MSGSET_APP,
                "msg",      "%s", "test21_big_token: no C_AUTH_BFF found under __bff_side__",
                NULL
            );
        }
        gobj_free_iter(dl);
    } else {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test21_big_token: __bff_side__ service not found",
            NULL
        );
    }

    priv->test_passed = TRUE;

    /*
     *  The second login, on the same connection: an access_token one byte
     *  over what a browser keeps
     */
    set_mock_answer(gobj, AT_FITS + 1, 0, NULL);
    post_login(gobj);
    KW_DECREF(kw)
    return 0;

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
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", "test21_big_token: connection closed before success",
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
GOBJ_DEFINE_GCLASS(C_TEST21_BIG_TOKEN);

/***************************************************************************
 *
 ***************************************************************************/
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
PUBLIC int register_c_test21_big_token(void)
{
    return create_gclass(C_TEST21_BIG_TOKEN);
}
