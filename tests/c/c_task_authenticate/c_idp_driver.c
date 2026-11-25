/****************************************************************************
 *          C_IDP_DRIVER.C
 *
 *          Driver of test18_idp_keycloak: C_IDP_KEYCLOAK against the mock IdP,
 *          one command at a time. A command of that gclass answers later, by
 *          name: to the channel its request came through, found under the
 *          service of its iev stack. So the driver is that service, and
 *          C_IDP_CHANNEL, its child, the channel: it takes the EV_SEND_IEV
 *          and hands the answer to the driver.
 *
 *          The admin token lives 1 second in the answers of the mock (less
 *          than the skew of C_IDP_KEYCLOAK), so each command asks for one.
 *
 *            1. the token is longer than the gclass keeps: kc_unavailable,
 *               a WARNING;
 *            2. the connection of the first /token is cut: asked again, the
 *               command is answered 0 (client_credentials has no effect at
 *               the IdP: it used to end in kc_unavailable);
 *            3. the connection of every /token is cut: 3 /token, not one
 *               more, kc_unavailable, a WARNING;
 *            4. the connection of the first GET is cut: sent again, 0;
 *            5. the connection of every GET is cut: 3 GET, kc_unavailable, a
 *               WARNING (it used to go on for ever);
 *            6. the connection of a DELETE is cut: 1 DELETE, kc_unavailable,
 *               a WARNING: what changes something is not sent again;
 *            7. a token whose "expires_in" is a text: 0, no ERROR, and it
 *               lives 60 seconds;
 *            8. a list with a search of 600 bytes, with the token of step 7:
 *               the url carries it whole (it used to be cut, with no word);
 *        9-14. each after a set-kc-config, which forgets the token: a token
 *               whose "expires_in" is negative, one too big for a date, and
 *               one of 0: they live 60 seconds too, so the next command asks
 *               for none.
 *          15. the client leaves before its answer: the channel is closed
 *               when the answer comes, and the answer is dropped with a
 *               WARNING (it used to be sent to the closed channel);
 *          16. another client takes the channel before the answer (the
 *               channels of a gate are a pool): the answer of the first is
 *               dropped with a WARNING (it used to go to the second), and
 *               the second gets its own.
 *
 *          The answer of a request that must be dropped is never awaited:
 *          C_IDP_KEYCLOAK serves one request at a time, so the answer of
 *          the request queued after it (marked "sync") says that it is
 *          over. Each request carries its mark in its iev stack, and the
 *          answer brings it back: an answer marked "gone" is the bug.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include <yunetas.h>

#include "c_idp_driver.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TEST_NAME       "test18_idp_keycloak"
#define SEARCH_LEN      600
#define CHANNEL_NAME    "channel"
#define CHANNEL2_NAME   "channel2"

/*  What happens to the channel of the request before its answer  */
#define GONE_NONE       0
#define GONE_CLOSED     1   /* the client leaves: the channel is closed */
#define GONE_TAKEN      2   /* another client takes the channel */

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    const char *command;
    int         access_token_len;   /* the token of the mock, 0 its own */
    int         drop_token;         /* /token requests the mock drops */
    int         drop_admin;         /* /admin/ requests the mock drops */
    int         result;             /* result of the answer */
    const char *error_code;         /* error_code of the answer, NULL none */
    int         token_seen;         /* /token requests that must arrive */
    int         admin_seen;         /* /admin/ requests that must arrive */
    const char *token_body;         /* json of the /token answer, NULL: one that lives 1 s */
    BOOL        forget_token;       /* set-kc-config before the command */
    int         gone;               /* GONE_*: the answer of the command must be dropped */
} step_t;

/***************************************************************************
 *              Data
 ***************************************************************************/
#define SHORT_TOKEN "{\"access_token\": \"AT\", \"expires_in\": 1}"
PRIVATE const step_t steps[] = {
    /* 1 */
    {"get-idp-user", 8192, 0, 0, -1, "kc_unavailable", 1, 0, "{}", FALSE, GONE_NONE},
    /* 2 */
    {"get-idp-user", 0, 1, 0, 0, NULL, 2, 1, NULL, FALSE, GONE_NONE},
    /* 3 */
    {"get-idp-user", 0, 9, 0, -1, "kc_unavailable", 3, 0, NULL, FALSE, GONE_NONE},
    /* 4 */
    {"get-idp-user", 0, 0, 1, 0, NULL, 1, 2, NULL, FALSE, GONE_NONE},
    /* 5 */
    {"get-idp-user", 0, 0, 9, -1, "kc_unavailable", 1, 3, NULL, FALSE, GONE_NONE},
    /* 6 */
    {"delete-idp-user", 0, 0, 1, -1, "kc_unavailable", 1, 1, NULL, FALSE, GONE_NONE},
    /* 7 */
    {"get-idp-user", 0, 0, 0, 0, NULL, 1, 1, "{\"access_token\": \"AT\", \"expires_in\": \"1\"}", FALSE, GONE_NONE},
    /* 8 */
    {"list-idp-users", 0, 0, 0, 0, NULL, 0, 1, NULL, FALSE, GONE_NONE},
    /* 9-10 */
    {"get-idp-user", 0, 0, 0, 0, NULL, 1, 1, "{\"access_token\": \"AT\", \"expires_in\": -5}", TRUE, GONE_NONE},
    {"get-idp-user", 0, 0, 0, 0, NULL, 0, 1, NULL, FALSE, GONE_NONE},
    /* 11-12 */
    {"get-idp-user", 0, 0, 0, 0, NULL, 1, 1,
        "{\"access_token\": \"AT\", \"expires_in\": 9223372036854775807}", TRUE, GONE_NONE},
    {"get-idp-user", 0, 0, 0, 0, NULL, 0, 1, NULL, FALSE, GONE_NONE},
    /* 13-14 */
    {"get-idp-user", 0, 0, 0, 0, NULL, 1, 1, "{\"access_token\": \"AT\", \"expires_in\": 0}", TRUE, GONE_NONE},
    {"get-idp-user", 0, 0, 0, 0, NULL, 0, 1, NULL, FALSE, GONE_NONE},
    /* 15-16: two GETs each, the one dropped and the "sync" one  */
    {"get-idp-user", 0, 0, 0, 0, NULL, 0, 2, NULL, FALSE, GONE_CLOSED},
    {"get-idp-user", 0, 0, 0, 0, NULL, 0, 2, NULL, FALSE, GONE_TAKEN},
};
#define NSTEPS ((int)(sizeof(steps)/sizeof(steps[0])))

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-------------flag----default-description*/
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
    hgobj   gobj_idp;
    hgobj   gobj_mock;
    hgobj   gobj_channel;   /* the channel of the requests */
    hgobj   gobj_channel2;  /* another one, for the "sync" request of GONE_CLOSED */
    BOOL    dying;          /* TRUE once a death-timer is armed */
    int     step;           /* step whose answer is awaited; -1 before the first */
} PRIVATE_DATA;

GOBJ_DEFINE_GCLASS(C_IDP_CHANNEL);
GOBJ_DEFINE_GCLASS(C_IDP_DRIVER);
GOBJ_DEFINE_EVENT(EV_IDP_ANSWER);




            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);  // C_TIMER counts in seconds
    priv->gobj_channel = gobj_create_pure_child(CHANNEL_NAME, C_IDP_CHANNEL, 0, gobj);
    priv->gobj_channel2 = gobj_create_pure_child(CHANNEL2_NAME, C_IDP_CHANNEL, 0, gobj);
    priv->step = -1;
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
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout0(priv->timer, 500);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout0(priv->timer);
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  The kw of a command, with the iev stack of a request that came through
 *  `channel`, a child of this driver. `mark` goes in the stack, and comes
 *  back in the answer.
 ***************************************************************************/
PRIVATE json_t *build_command_kw(hgobj gobj, const char *channel, const char *mark)
{
    json_t *kw = json_pack("{s:s}", "user_id", "abc");
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(strcmp(steps[priv->step].command, "list-idp-users") == 0) {
        char search[SEARCH_LEN + 1];
        memset(search, 's', SEARCH_LEN);
        search[SEARCH_LEN] = '\0';
        json_object_set_new(kw, "search", json_string(search));
    }
    msg_iev_push_stack(gobj, kw, IEVENT_STACK_ID, json_pack(
        "{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "test_request",     mark,
        "input_service",    gobj_name(gobj),
        "input_channel",    channel,
        "src_yuno",         "",
        "src_role",         "test",
        "src_service",      gobj_name(gobj),
        "dst_yuno",         "",
        "dst_role",         "",
        "dst_service",      "idp"
    ));
    return kw;
}

/***************************************************************************
 *  A user_id that is a dot-segment is a walk in the path of the admin API
 *  (DELETE .../users/.. may be .../realms/R to the server that normalizes
 *  it): the command refuses it at once, and nothing goes to the IdP
 ***************************************************************************/
PRIVATE void check_dot_user_id(hgobj gobj, const char *user_id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = build_command_kw(gobj, CHANNEL_NAME, "step");
    json_object_set_new(kw, "user_id", json_string(user_id));
    json_t *jn_resp = gobj_command(priv->gobj_idp, "delete-idp-user", kw, gobj);
    const char *error_code = json_string_value(
        json_object_get(json_object_get(jn_resp, "data"), "error_code"));
    if(!error_code || strcmp(error_code, "invalid_user_id") != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", TEST_NAME ": a dot-segment taken as a user_id",
            "user_id",  "%s", user_id,
            NULL
        );
    }
    JSON_DECREF(jn_resp)
}

/***************************************************************************
 *  A command through `channel`, marked `mark`: it must answer later
 ***************************************************************************/
PRIVATE void post_command(hgobj gobj, const char *channel, const char *mark)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_resp = gobj_command(priv->gobj_idp, steps[priv->step].command,
        build_command_kw(gobj, channel, mark), gobj);
    if(jn_resp) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", TEST_NAME ": a command answered at once",
            "step",     "%d", priv->step + 1,
            "mark",     "%s", mark,
            NULL
        );
        JSON_DECREF(jn_resp)
    }
}

/***************************************************************************
 *  A step whose request loses its client before the answer: the request
 *  marked "gone", what happens to its channel, and the "sync" request
 *  whose answer ends the step.
 ***************************************************************************/
PRIVATE void post_gone_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj channel = priv->gobj_channel;
    json_int_t connection = gobj_read_integer_attr(channel, "connection");

    gobj_write_bool_attr(channel, "opened", TRUE);
    post_command(gobj, CHANNEL_NAME, "gone");

    if(steps[priv->step].gone == GONE_CLOSED) {
        /*  The client leaves, and nobody takes the channel  */
        gobj_write_bool_attr(channel, "opened", FALSE);
        post_command(gobj, CHANNEL2_NAME, "sync");
    } else {
        /*  The client leaves, and another one takes the channel: it is
         *  open again, with the next connection, and asks too  */
        gobj_write_integer_attr(channel, "connection", connection + 1);
        post_command(gobj, CHANNEL_NAME, "sync");
    }
}

/***************************************************************************
 *  The command of the current step
 ***************************************************************************/
PRIVATE void post_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const step_t *st = &steps[priv->step];

    if(priv->step == 0) {
        check_dot_user_id(gobj, ".");
        check_dot_user_id(gobj, "..");
    }

    gobj_write_integer_attr(priv->gobj_mock, "access_token_len", st->access_token_len);
    gobj_write_integer_attr(priv->gobj_mock, "drop_token_requests", st->drop_token);
    gobj_write_integer_attr(priv->gobj_mock, "drop_admin_requests", st->drop_admin);
    gobj_write_integer_attr(priv->gobj_mock, "token_seen", 0);
    gobj_write_integer_attr(priv->gobj_mock, "admin_seen", 0);
    gobj_write_integer_attr(priv->gobj_mock, "admin_status",
        strcmp(st->command, "delete-idp-user") == 0? 204 : 200);
    const char *token_body = st->token_body? st->token_body : SHORT_TOKEN;
    json_t *jn_token = anystring2json(token_body, strlen(token_body), FALSE);
    gobj_write_json_attr(priv->gobj_mock, "override_token_body", jn_token);
    JSON_DECREF(jn_token)

    if(st->forget_token) {
        json_t *jn_set = gobj_command(priv->gobj_idp, "set-kc-config",
            json_pack("{s:s}", "kc_realm", "test"), gobj);
        JSON_DECREF(jn_set)
    }

    if(st->gone != GONE_NONE) {
        post_gone_step(gobj);
        return;
    }

    json_t *jn_resp = gobj_command(priv->gobj_idp, st->command,
        build_command_kw(gobj, CHANNEL_NAME, "step"), gobj);
    if(jn_resp) {
        char *dump = json2uglystr(jn_resp);
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_APP,
            "msg",      "%s", TEST_NAME ": a command answered at once",
            "step",     "%d", priv->step + 1,
            "answer",   "%.300s", dump? dump : "",
            NULL
        );
        GBMEM_FREE(dump)
        JSON_DECREF(jn_resp)
    }
}

/***************************************************************************
 *  What the current step must leave
 ***************************************************************************/
PRIVATE void check_step(hgobj gobj, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const step_t *st = &steps[priv->step];

    int result = (int)json_integer_value(json_object_get(kw, "result"));
    const char *error_code = json_string_value(
        json_object_get(json_object_get(kw, "data"), "error_code"));
    int token_seen = (int)gobj_read_integer_attr(priv->gobj_mock, "token_seen");
    int admin_seen = (int)gobj_read_integer_attr(priv->gobj_mock, "admin_seen");
    BOOL code_ok = st->error_code?
        (error_code && strcmp(error_code, st->error_code) == 0) : !error_code;
    if(result != st->result || !code_ok ||
            token_seen != st->token_seen || admin_seen != st->admin_seen) {
        char *dump = json2uglystr(kw);
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": a step did not end as expected",
            "step",         "%d", priv->step + 1,
            "token_seen",   "%d", token_seen,
            "admin_seen",   "%d", admin_seen,
            "answer",       "%.400s", dump? dump : "",
            NULL
        );
        GBMEM_FREE(dump)
    }

    if(strcmp(st->command, "list-idp-users") == 0) {
        const char *url = gobj_read_str_attr(priv->gobj_mock, "last_admin_url");
        const char *search = strstr(url, "&search=");
        size_t len = search? strlen(search + strlen("&search=")) : 0;
        if(len != SEARCH_LEN) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset",   "%s", MSGSET_APP,
                "msg",      "%s", TEST_NAME ": the search of a list is cut",
                "len",      "%d", (int)len,
                NULL
            );
        }
    }
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Timer fired: the first step, or die
 ***************************************************************************/
PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->dying) {
        JSON_DECREF(kw)
        set_yuno_must_die();
        return 0;
    }

    priv->gobj_idp = gobj_find_service("idp", TRUE);
    hgobj idp_side = gobj_find_service("__idp_side__", TRUE);
    json_t *jn_filter = json_pack("{s:s}", "__gclass_name__", "C_MOCK_IDP");
    json_t *dl = gobj_match_children_tree(idp_side, jn_filter);
    priv->gobj_mock = (hgobj)(size_t)json_integer_value(json_array_get(dl, 0));
    gobj_free_iter(dl);

    priv->step = 0;
    post_step(gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The answer of a command, through the channel. Posted to itself: it
 *  arrives inside the task of C_IDP_KEYCLOAK.
 ***************************************************************************/
PRIVATE int ac_idp_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_stack = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, FALSE);
    const char *mark = json_string_value(json_object_get(jn_stack, "test_request"));
    if(!mark || strcmp(mark, "gone") == 0) {
        /*  The answer of a client that left: it must not come  */
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", TEST_NAME ": the answer of a client that left was sent",
            "step",         "%d", priv->step + 1,
            "channel",      "%s", gobj_name(src),
            "mark",         "%s", mark? mark : "",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    check_step(gobj, kw);

    priv->step++;
    if(priv->step < NSTEPS) {
        post_step(gobj);
    } else {
        gobj_stop_tree(gobj_find_service("__idp_side__", TRUE));
        priv->dying = TRUE;
        set_timeout0(priv->timer, 300);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Volatil child stopped: destroy it. The C_TIMER0 publishes one at each
 *  cancel, and is not volatil.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  C_IDP_CHANNEL: the answer of a command, to the driver on the next turn
 ***************************************************************************/
PRIVATE int ac_send_iev(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_post_event(gobj_parent(gobj), EV_IDP_ANSWER, kw, gobj);
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

PRIVATE const GMETHODS gmt_channel = {
    0
};

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_idp_driver(void)
{
    ev_action_t st_idle_channel[] = {
        {EV_SEND_IEV,       ac_send_iev,    0},
        {0, 0, 0}
    };
    states_t states_channel[] = {
        {ST_IDLE, st_idle_channel},
        {0, 0}
    };
    event_type_t event_types_channel[] = {
        {EV_SEND_IEV,       0},
        {0, 0}
    };
    /*  The two attributes of a C_CHANNEL that C_IDP_KEYCLOAK reads  */
    PRIVATE sdata_desc_t attrs_table_channel[] = {
        SDATA (DTP_BOOLEAN, "opened",       SDF_RD, "1",    "the channel is open"),
        SDATA (DTP_INTEGER, "connection",   SDF_RD, "1",    "the connection it holds"),
        SDATA_END()
    };
    if(!gclass_create(C_IDP_CHANNEL, event_types_channel, states_channel,
            &gmt_channel, 0, attrs_table_channel, sizeof(int), 0, 0, 0, 0)) {
        return -1;
    }

    ev_action_t st_idle[] = {
        {EV_IDP_ANSWER,     ac_idp_answer,  0},
        {EV_TIMEOUT,        ac_timer,       0},
        {EV_STOPPED,        ac_stopped,     0},
        {0, 0, 0}
    };
    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };
    event_type_t event_types[] = {
        {EV_IDP_ANSWER,     0},
        {EV_TIMEOUT,        0},
        {EV_STOPPED,        0},
        {0, 0}
    };
    if(!gclass_create(C_IDP_DRIVER, event_types, states, &gmt, 0, attrs_table,
            sizeof(PRIVATE_DATA), 0, 0, s_user_trace_level, 0)) {
        return -1;
    }
    return 0;
}
