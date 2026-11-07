/***********************************************************************
 *          C_TEST_CLI_OPERATOR.C
 *
 *          GClass to test how C_IEVENT_CLI takes the operator of a request
 *          of its server
 *
 *          A controlcenter that forwards a command of an operator to an
 *          agent says who asks in `__username__`. With `verify_operator`
 *          the agent does not believe it: the request carries the
 *          operator's token (`__operator_jwt__`), which the client verifies
 *          against the jwks of its yuno's C_AUTHZ, and the token's email is
 *          the `__username__`. A request with no token is of
 *          `service_principal`. The token is taken out of the request in
 *          every case: no command sees it.
 *
 *          The server is raw: a websocket gate (C_IOGATE -> C_CHANNEL ->
 *          C_WEBSOCKET -> C_TCP, no C_IEVENT_SRV) whose frames this gclass
 *          writes by hand. Its identity card must say
 *          `knows_operator_identity` (the controlcenter gives the token only
 *          to such an agent). Then the ACK, and seven commands `whoami` to
 *           this service, each saying `__username__` liar@example.com:
 *
 *              0. a good token of alice@example.com
 *              1. a token signed with another key
 *              2. an expired token
 *              3. a token whose email is not verified
 *              4. no token
 *              5. a token that is not a string
 *              6. a good token of bob@example.com, a user of this yuno's
 *                 authz that is disabled
 *
 *          With verify_operator (test_c_ievent_cli_operator): 0 is run as
 *          alice@example.com, 4 as the service principal
 *          (the-controlcenter), and 1, 2, 3, 5 and 6 are answered with an
 *          error and never reach the command (ONE warning each). Without it
 *          (OPERATOR_UNVERIFIED): the seven are run as liar@example.com, as
 *          up to 7.30.5. In both, no command sees the token, and no log
 *          line carries one (main.c looks for them in every line).
 *
 *          The seven answers come back to the raw server, in order; the
 *          seventh ends the test.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <time.h>
#include <jwt.h>
#include "c_test_cli_operator.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define N_REQUESTS  7
#define TEST_ISS    "https://idp.test.example/realms/test"
#define LIAR        "liar@example.com"
#define ALICE       "alice@example.com"
#define BOB         "bob@example.com"     // a user of the authz, disabled
#define PRINCIPAL   "the-controlcenter"

/*
 *  The key of the issuer (its jwks is in main.c, the same `k`), and
 *  another one: base64url of 32 ASCII bytes each
 */
PRIVATE const char *GOOD_JWK =
    "{\"kty\":\"oct\",\"k\":\"eXVuZXRhLW9wZXJhdG9yLXRlc3Qta2V5LTAxMjM0NTY\","
    "\"alg\":\"HS256\",\"use\":\"sig\"}";
PRIVATE const char *OTHER_JWK =
    "{\"kty\":\"oct\",\"k\":\"YW5vdGhlci1rZXktbm90LW9mLXRoZS1pc3N1ZXItMDE\","
    "\"alg\":\"HS256\",\"use\":\"sig\"}";

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE char *make_token(const char *jwk_json, const char *email, BOOL verified, int ttl);
PRIVATE int send_requests(hgobj gobj, hgobj channel);
PRIVATE int send_frame(hgobj gobj, hgobj channel, gobj_event_t event, json_t *kw);
PRIVATE json_t *frame_kw(const char *msg_type);
PRIVATE void check(hgobj gobj, const char *what, BOOL ok);
PRIVATE void check_answers(hgobj gobj);

PRIVATE json_t *cmd_whoami(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
int test_cli_operator_failed = 0;
char *test_tokens[N_REQUESTS] = {0};    // main.c looks for them in the log

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "whoami",           0,          0,              cmd_whoami,     "Who asks, as the command sees it"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
PRIVATE const trace_level_t s_user_trace_level[16] = {
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_input_side;  // the raw server
    hgobj gobj_cli;         // the C_IEVENT_CLI
    BOOL card_seen;
    json_t *jn_ran;         // [{username, saw_token}] of each whoami run
    json_t *jn_answers;     // [{result, comment}] of each answer, in order
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

    priv->jn_ran = json_array();
    priv->jn_answers = json_array();
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->jn_ran)
    JSON_DECREF(priv->jn_answers)
    for(int i=0; i<N_REQUESTS; i++) {
        GBMEM_FREE(test_tokens[i])
    }
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    priv->gobj_cli = gobj_find_service("cli", TRUE);
    gobj_subscribe_event(priv->gobj_cli, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_cli);
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *  Who asks, as a command of the agent sees it: its __username__, and
 *  whether the token reached it (it must not)
 ***************************************************************************/
PRIVATE json_t *cmd_whoami(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *username = kw_get_str(gobj, kw, "__username__", "", 0);
    BOOL saw_token = json_object_get(kw, "__operator_jwt__")? TRUE : FALSE;
    json_array_append_new(priv->jn_ran, json_pack("{s:s, s:b}",
        "username", username,
        "saw_token", saw_token
    ));

    return msg_iev_build_response(gobj,
        0,
        json_sprintf("%s: %s", gobj_yuno_role_plus_name(), username),
        0,
        0,
        kw  // owned
    );
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  A token of `jwk_json`'s key, of the test issuer. Return is gbmem, yours
 ***************************************************************************/
PRIVATE char *make_token(const char *jwk_json, const char *email, BOOL verified, int ttl)
{
    jwk_set_t *jwk_set = jwks_create(jwk_json);
    const jwk_item_t *key = jwk_set? jwks_item_get(jwk_set, 0) : NULL;
    if(!key) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "test key not made",
            NULL
        );
        if(jwk_set) {
            jwks_free(jwk_set);
        }
        return NULL;
    }

    jwt_builder_t *b = jwt_builder_new();
    if(!b || jwt_builder_setkey(b, JWT_ALG_HS256, key) != 0) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "test token builder not made",
            NULL
        );
        if(b) {
            jwt_builder_free(b);
        }
        jwks_free(jwk_set);
        return NULL;
    }

    jwt_value_t jval;
    time_t now = time(NULL);
    jwt_set_SET_STR(&jval, "iss", TEST_ISS);
    jwt_builder_claim_set(b, &jval);
    jwt_set_SET_STR(&jval, "email", email);
    jwt_builder_claim_set(b, &jval);
    jwt_set_SET_BOOL(&jval, "email_verified", verified? 1 : 0);
    jwt_builder_claim_set(b, &jval);
    jwt_set_SET_INT(&jval, "iat", (jwt_long_t)now);
    jwt_builder_claim_set(b, &jval);
    jwt_set_SET_INT(&jval, "exp", (jwt_long_t)(now + ttl));
    jwt_builder_claim_set(b, &jval);

    char *token = jwt_builder_generate(b);
    jwt_builder_free(b);
    jwks_free(jwk_set);
    if(!token) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "test token not generated",
            NULL
        );
    }
    return token;
}

/***************************************************************************
 *  The kw of a frame of the server to the client's own service
 ***************************************************************************/
PRIVATE json_t *frame_kw(const char *msg_type)
{
    json_t *kw = json_object();
    json_t *jn_routing = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "dst_yuno", "",
        "dst_role", gobj_yuno_role(),
        "dst_service", "cli",
        "src_yuno", "",
        "src_role", "raw_server",
        "src_service", "raw",
        "user", "",
        "host", ""
    );
    msg_iev_push_stack(0, kw, IEVENT_STACK_ID, jn_routing);
    msg_iev_set_msg_type(0, kw, msg_type);
    return kw;
}

/***************************************************************************
 *  A frame written by hand, to the channel of the client
 ***************************************************************************/
PRIVATE int send_frame(hgobj gobj, hgobj channel, gobj_event_t event, json_t *kw)
{
    gbuffer_t *gbuf = iev_create_to_gbuffer(gobj, event, kw);
    if(!gbuf) {
        // Error already logged
        return -1;
    }
    return gobj_send_event(
        channel,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
}

/***************************************************************************
 *  The ACK, and the seven requests
 ***************************************************************************/
PRIVATE int send_requests(hgobj gobj, hgobj channel)
{
    json_t *kw_ack = frame_kw("__answer__");
    json_object_set_new(kw_ack, "result", json_integer(0));
    send_frame(gobj, channel, EV_IDENTITY_CARD_ACK, kw_ack);

    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    jwt_set_alloc(malloc_func, free_func);
    test_tokens[0] = make_token(GOOD_JWK, ALICE, TRUE, 3600);
    test_tokens[1] = make_token(OTHER_JWK, ALICE, TRUE, 3600);
    test_tokens[2] = make_token(GOOD_JWK, ALICE, TRUE, -3600);
    test_tokens[3] = make_token(GOOD_JWK, ALICE, FALSE, 3600);
    test_tokens[6] = make_token(GOOD_JWK, BOB, TRUE, 3600);

    for(int i=0; i<N_REQUESTS; i++) {
        json_t *kw_command = frame_kw("__command__");
        json_object_set_new(kw_command, "service", json_string(gobj_name(gobj)));
        json_object_set_new(kw_command, "__command__", json_string("whoami"));
        json_object_set_new(kw_command, "__username__", json_string(LIAR));
        if((i < 4 || i == 6) && test_tokens[i]) {
            json_object_set_new(kw_command, "__operator_jwt__", json_string(test_tokens[i]));
        } else if(i == 5) {
            json_object_set_new(kw_command, "__operator_jwt__", json_integer(5));
        }
        send_frame(gobj, channel, EV_MT_COMMAND, kw_command);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void check(hgobj gobj, const char *what, BOOL ok)
{
    if(!ok) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL",
            "what",         "%s", what,
            NULL
        );
        test_cli_operator_failed++;
    }
}

/***************************************************************************
 *  What ran, and what was answered
 ***************************************************************************/
PRIVATE void check_answers(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    check(gobj, "the identity card says knows_operator_identity", priv->card_seen);

    size_t idx; json_t *jn_ran;
    json_array_foreach(priv->jn_ran, idx, jn_ran) {
        check(gobj, "no command sees the token of the operator",
            !kw_get_bool(gobj, jn_ran, "saw_token", FALSE, 0));
    }

#ifdef OPERATOR_UNVERIFIED
    check(gobj, "unverified: the seven requests ran", json_array_size(priv->jn_ran) == N_REQUESTS);
    json_array_foreach(priv->jn_ran, idx, jn_ran) {
        check(gobj, "unverified: each as the __username__ the server says",
            strcmp(kw_get_str(gobj, jn_ran, "username", "", 0), LIAR)==0);
    }
#else
    BOOL two_ran = json_array_size(priv->jn_ran) == 2;
    check(gobj, "verified: two requests ran (the good token, and no token)", two_ran);
    if(two_ran) {
        check(gobj, "verified: the good token ran as its email, not as the server says",
            strcmp(kw_get_str(gobj, json_array_get(priv->jn_ran, 0), "username", "", 0), ALICE)==0);
        check(gobj, "verified: the request with no token ran as the service principal",
            strcmp(kw_get_str(gobj, json_array_get(priv->jn_ran, 1), "username", "", 0), PRINCIPAL)==0);
    }

    const int refused[N_REQUESTS] = {0, 1, 1, 1, 0, 1, 1};
    json_t *jn_answer;
    json_array_foreach(priv->jn_answers, idx, jn_answer) {
        int result = (int)kw_get_int(gobj, jn_answer, "result", 0, 0);
        if(refused[idx]) {
            check(gobj, "verified: a bad token is answered with an error",
                result < 0 && strstr(kw_get_str(gobj, jn_answer, "comment", "", 0), "operator"));
        } else {
            check(gobj, "verified: a good request is answered", result == 0);
        }
    }
#endif
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  A frame of the client to the raw server: its identity card (answered
 *  with the ACK and the requests), or an answer
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
    hgobj channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
    gobj_event_t iev_event = 0;
    json_t *iev_kw = 0;
    if(gbuf) {
        gbuffer_incref(gbuf);
        iev_kw = iev_create_from_gbuffer(gobj, &iev_event, gbuf, 0);
    }
    if(iev_event == EV_IDENTITY_CARD && channel) {
        priv->card_seen = kw_get_bool(gobj, iev_kw, "knows_operator_identity", FALSE, 0);
        send_requests(gobj, channel);

    } else if(iev_event == EV_MT_COMMAND_ANSWER) {
        json_array_append_new(priv->jn_answers, json_pack("{s:I, s:s}",
            "result", kw_get_int(gobj, iev_kw, "result", -1, 0),
            "comment", kw_get_str(gobj, iev_kw, "comment", "", 0)
        ));
        if(json_array_size(priv->jn_answers) == N_REQUESTS) {
            check_answers(gobj);
            set_yuno_must_die();
        }
    }

    KW_DECREF(iev_kw)
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The answer to a request is read by the raw server from the frame
 *  (ac_on_message): it is public here only so that the frame is taken.
 *  Never sent to this gobj.
 ***************************************************************************/
PRIVATE int ac_unexpected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    check(gobj, "an answer sent to the tester itself", FALSE);
    KW_DECREF(kw)
    return -1;
}

/***************************************************************************
 *  The transport and the client open and close
 ***************************************************************************/
PRIVATE int ac_on_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    KW_DECREF(kw)
    return 0;
}




/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_CLI_OPERATOR);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_ON_OPEN,                ac_on_event,                0},
        {EV_ON_CLOSE,               ac_on_event,                0},
        {EV_ON_OPEN_ERROR,          ac_on_event,                0},
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {EV_MT_COMMAND_ANSWER,      ac_unexpected,              0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_ON_OPEN_ERROR,          0},
        {EV_ON_MESSAGE,             0},
        {EV_MT_COMMAND_ANSWER,      EVF_PUBLIC_EVENT},
        {EV_STOPPED,                0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test_cli_operator(void)
{
    return create_gclass(C_TEST_CLI_OPERATOR);
}
