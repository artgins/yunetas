/****************************************************************************
 *          test_authz_token_age.c
 *
 *          The age of a login token (C_AUTHZ `max_token_age`).
 *
 *          Yuneta's own limit, whatever the identity provider lets a token
 *          live: a login token opens a session only if it was issued (its
 *          `iat`) no more than `max_token_age` seconds ago. The users of
 *          `token_age_exempt_users` are not checked.
 *
 *          test_authz_token_age (max_token_age one hour, old-agent exempt):
 *            1. alice, issued a minute ago: a session
 *            2. alice, issued two hours ago: refused, "The token is too
 *               old", ONE warning
 *            3. alice, no iat: refused, "The token has no iat", ONE warning
 *            4. old-agent, issued ten years ago: a session (exempt)
 *            5. bob, issued 59 minutes ago: a session
 *            6. the local method verify_jwt (no audience: a login token),
 *               alice issued two hours ago: refused, no log
 *            7. verify_jwt, alice issued a minute ago: good
 *          test_authz_token_age_unlimited (max_token_age 0, the default):
 *            the same seven, all good, no warning.
 *          In both, no log line carries a token of the test, and every
 *          session opened is closed (EV_ON_CLOSE) before the end.
 *
 *          A real C_AUTHZ service over a temp tranger store, its role and
 *          users seeded by initial_load. The logins are gobj_authenticate()
 *          on it, each from its own peer (a child of this driver, which
 *          holds what C_AUTHZ writes in the src of a session), from a timer
 *          action inside the running loop.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <jwt.h>
#include <yunetas.h>

#ifdef TOKEN_AGE_UNLIMITED
    #define APP         "test_authz_token_age_unlimited"
    #define MAX_AGE     "0"
    #define STORE       "/tmp/test_authz_token_age_unlimited_store"
#else
    #define APP         "test_authz_token_age"
    #define MAX_AGE     "3600"
    #define STORE       "/tmp/test_authz_token_age_store"
#endif
#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DOC         "the age of a login token in C_AUTHZ"
#define APP_DATETIME    ""

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define TEST_ISS        "https://idp.test.example/realms/test"
#define ALICE           "alice@example.com"
#define BOB             "bob@example.com"
#define OLD_AGENT       "old-agent@example.com"
#define MAX_TOKENS      8

/*
 *  The key of the test issuer (its jwks is in the config, the same `k`):
 *  base64url of 32 ASCII bytes. A test key, good for nothing else.
 */
PRIVATE const char *LOGIN_JWK =
    "{\"kty\":\"oct\",\"k\":\"eXVuZXRhLW9wZXJhdG9yLXRlc3Qta2V5LTAxMjM0NTY\","
    "\"alg\":\"HS256\",\"use\":\"sig\"}";

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int s_result = 0;       /* accumulated check result, read after entry_point */
PRIVATE int s_too_old = 0;      /* warnings "Authentication failed: token too old" */
PRIVATE int s_warnings = 0;     /* logs of priority WARNING and up */
PRIVATE int s_token_logged = 0; /* log lines that carry a token of the test */
PRIVATE char *s_tokens[MAX_TOKENS] = {0};

GOBJ_DEFINE_GCLASS(C_TEST_TOKEN_AGE);

typedef struct {
    hgobj   timer;
    BOOL    dying;
} PRIVATE_DATA;

/***************************************************************
 *              Config (single quotes -> double at runtime)
 ***************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '" APP "',                                     \n\
        'tags': ['test', 'yunetas']                                 \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'work_dir': '/tmp',                                         \n\
        'console_log_handlers': {},                                 \n\
        'daemon_log_handlers': {}                                   \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'autoplay': true,                                           \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'service_descriptor': {},                                   \n\
        'realm_owner': 'test',                                      \n\
        'realm_id':    'test',                                      \n\
        'trace_levels': {}                                          \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'checked',                                      \n\
            'gclass': 'C_TEST_TOKEN_AGE',                           \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true                                        \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'authz',                                        \n\
            'gclass': 'C_AUTHZ',                                    \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'tranger_path': '" STORE "',                        \n\
                'master': true,                                     \n\
                'max_token_age': " MAX_AGE ",                       \n\
                'token_age_exempt_users': ['" OLD_AGENT "'],        \n\
                'jwks': [                                           \n\
                    {                                               \n\
                        'kid': '" TEST_ISS "',                      \n\
                        'kty': 'oct',                               \n\
                        'alg': 'HS256',                             \n\
                        'k': 'eXVuZXRhLW9wZXJhdG9yLXRlc3Qta2V5LTAxMjM0NTY' \n\
                    }                                               \n\
                ],                                                  \n\
                'initial_load': {                                   \n\
                    'roles': [                                      \n\
                        {                                           \n\
                            'id': 'operator',                       \n\
                            'description': 'may log in checked',    \n\
                            'realm_id': '*',                        \n\
                            'service': 'checked'                    \n\
                        }                                           \n\
                    ],                                              \n\
                    'users': [                                      \n\
                        {'id': '" ALICE "', 'roles': ['roles^operator^users']}, \n\
                        {'id': '" BOB "', 'roles': ['roles^operator^users']}, \n\
                        {'id': '" OLD_AGENT "', 'roles': ['roles^operator^users']} \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE void check_int(const char *name, int got, int expected)
{
    if(got != expected) {
        printf("FAIL %-60s got %d expected %d\n", name, got, expected);
        s_result += -1;
    } else {
        printf("ok   %-60s (%d)\n", name, got);
    }
}

PRIVATE void check_str(const char *name, const char *got, const char *expected)
{
    if(strcmp(got, expected)!=0) {
        printf("FAIL %-60s got '%s' expected '%s'\n", name, got, expected);
        s_result += -1;
    } else {
        printf("ok   %-60s ('%s')\n", name, got);
    }
}

/*
 *  Counts the warnings (and up), those of a token too old, and the lines
 *  that carry a token of the test: a check reads the difference around
 *  one call.
 */
PRIVATE int count_logs(void *h, int priority, const char *bf, size_t len)
{
    s_warnings++;
    const char *too_old = "Authentication failed: token too old";
    if(memmem(bf, len, too_old, strlen(too_old))) {
        s_too_old++;
    }
    return 0;
}

PRIVATE int find_tokens(void *h, int priority, const char *bf, size_t len)
{
    for(int i=0; i<MAX_TOKENS; i++) {
        if(s_tokens[i] && memmem(bf, len, s_tokens[i], strlen(s_tokens[i]))) {
            s_token_logged++;
        }
    }
    return 0;
}

/*
 *  A login token of the test issuer for `email`, issued `age` seconds ago
 *  (with no iat when `age` < 0), expiring in a month. Kept in s_tokens
 *  (gbmem) to look for in the log and to be freed at the end.
 */
PRIVATE const char *make_login_token(int slot, const char *email, int age)
{
    time_t now = time(NULL);
    char sid[32];
    snprintf(sid, sizeof(sid), "session-%d", slot);    // the session id C_AUTHZ takes
    json_t *claims = json_pack("{s:s, s:s, s:s, s:b, s:I}",
        "iss", TEST_ISS,
        "sid", sid,
        "email", email,
        "email_verified", 1,
        "exp", (json_int_t)(now + 30*86400)
    );
    if(age >= 0) {
        json_object_set_new(claims, "iat", json_integer((json_int_t)(now - age)));
    }

    jwk_set_t *jwk_set = jwks_create(LOGIN_JWK);
    const jwk_item_t *key = jwk_set? jwks_item_get(jwk_set, 0) : NULL;
    jwt_builder_t *b = key? jwt_builder_new() : NULL;
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
        if(jwk_set) {
            jwks_free(jwk_set);
        }
        JSON_DECREF(claims)
        return "";
    }

    jwt_builder_enable_iat(b, 0);   // the iat is the claims', not now
    const char *name; json_t *jn_value;
    json_object_foreach(claims, name, jn_value) {
        jwt_value_t jval;
        if(json_is_string(jn_value)) {
            jwt_set_SET_STR(&jval, name, json_string_value(jn_value));
        } else if(json_is_boolean(jn_value)) {
            jwt_set_SET_BOOL(&jval, name, json_is_true(jn_value)? 1 : 0);
        } else {
            jwt_set_SET_INT(&jval, name, (jwt_long_t)json_integer_value(jn_value));
        }
        jwt_builder_claim_set(b, &jval);
    }
    s_tokens[slot] = jwt_builder_generate(b);
    jwt_builder_free(b);
    jwks_free(jwk_set);
    JSON_DECREF(claims)
    if(!s_tokens[slot]) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "test token not generated",
            NULL
        );
        return "";
    }
    return s_tokens[slot];
}

/*
 *  A login with the token of `slot`, from a peer of its own: its result,
 *  its comment, the warnings "token too old" it logged, and the user of
 *  the session. A session opened is closed (EV_ON_CLOSE) and the peer
 *  goes.
 */
PRIVATE void check_login(
    hgobj gobj,
    const char *what,
    int slot,
    const char *email,
    int age,
    int expected_result,
    const char *expected_comment,   // only for a refusal
    int expected_too_old
)
{
    const char *token = make_login_token(slot, email, age);
    hgobj authz = gobj_find_service("authz", TRUE);

    char name[NAME_MAX];
    snprintf(name, sizeof(name), "peer-%d", slot);
    hgobj peer = gobj_create(name, C_TEST_TOKEN_AGE, json_pack("{s:b, s:s}",
        "is_peer", 1,
        "peername", "127.0.0.1:40000"
    ), gobj);
    gobj_start(peer);

    int too_old_before = s_too_old;
    json_t *response = gobj_authenticate(authz, json_pack("{s:s, s:s}",
        "jwt", token,
        "dst_service", "checked"
    ), peer);

    int result = (int)kw_get_int(gobj, response, "result", -99, 0);
    char label[NAME_MAX];
    snprintf(label, sizeof(label), "%s: result", what);
    check_int(label, result, expected_result);
    if(expected_result < 0) {
        snprintf(label, sizeof(label), "%s: comment", what);
        check_str(label, kw_get_str(gobj, response, "comment", "", 0), expected_comment);
    } else {
        snprintf(label, sizeof(label), "%s: the user of the session", what);
        check_str(label, gobj_read_str_attr(peer, "__username__"), email);
    }
    snprintf(label, sizeof(label), "%s: warnings 'token too old'", what);
    check_int(label, s_too_old - too_old_before, expected_too_old);
    JSON_DECREF(response)

    if(result == 0) {
        gobj_publish_event(peer, EV_ON_CLOSE, json_object());
    }
    gobj_stop(peer);
    gobj_destroy(peer);
}

/*
 *  The local method verify_jwt with no audience (a login token)
 */
PRIVATE void check_verify(
    hgobj gobj,
    const char *what,
    int slot,
    const char *email,
    int age,
    int expected_result,
    const char *expected_comment    // only for a refusal
)
{
    const char *token = make_login_token(slot, email, age);
    hgobj authz = gobj_find_service("authz", TRUE);

    int warnings_before = s_warnings;
    json_t *response = gobj_local_method(authz, "verify_jwt",
        json_pack("{s:s}", "jwt", token), gobj);

    char label[NAME_MAX];
    snprintf(label, sizeof(label), "%s: result", what);
    check_int(label, (int)kw_get_int(gobj, response, "result", -99, 0), expected_result);
    if(expected_result < 0) {
        snprintf(label, sizeof(label), "%s: comment", what);
        check_str(label, kw_get_str(gobj, response, "comment", "", 0), expected_comment);
    }
    snprintf(label, sizeof(label), "%s: logs nothing", what);
    check_int(label, s_warnings - warnings_before, 0);
    JSON_DECREF(response)
}

PRIVATE void run_checks(hgobj gobj)
{
#ifdef TOKEN_AGE_UNLIMITED
    const int limited = 0;
#else
    const int limited = 1;
#endif

    check_login(gobj, "1. alice, a minute old", 0, ALICE, 60, 0, "", 0);
    check_login(gobj, "2. alice, two hours old", 1, ALICE, 7200,
        limited? -1 : 0, "The token is too old", limited);
    check_login(gobj, "3. alice, no iat", 2, ALICE, -1,
        limited? -1 : 0, "The token has no iat", limited);
    check_login(gobj, "4. old-agent, ten years old (exempt)", 3, OLD_AGENT,
        10*365*86400, 0, "", 0);
    check_login(gobj, "5. bob, 59 minutes old", 4, BOB, 3540, 0, "", 0);
    check_verify(gobj, "6. verify_jwt, alice two hours old", 5, ALICE, 7200,
        limited? -1 : 0, "The token is too old");
    check_verify(gobj, "7. verify_jwt, alice a minute old", 6, ALICE, 60, 0, "");
}

/***************************************************************
 *              Framework Methods
 ***************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!gobj_read_bool_attr(gobj, "is_peer")) {
        priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
    }
}

PRIVATE void mt_destroy(hgobj gobj)
{
    /*  The driver's tokens, made by libjwt with gbmem: freed before the audit  */
    if(!gobj_read_bool_attr(gobj, "is_peer")) {
        for(int i=0; i<MAX_TOKENS; i++) {
            GBMEM_FREE(s_tokens[i])
        }
    }
}

PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->timer) {
        gobj_start(priv->timer);
    }
    return 0;
}

PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->timer) {
        gobj_stop(priv->timer);
    }
    return 0;
}

PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    set_timeout(priv->timer, 10);   // fire once, inside the loop
    return 0;
}

PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    clear_timeout(priv->timer);
    return 0;
}

/***************************************************************
 *              Actions
 ***************************************************************/
/*
 *  First fire: run the checks, then arm the death timer. Second fire:
 *  set_yuno_must_die(), from inside the running loop.
 */
PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->dying) {
        JSON_DECREF(kw)
        set_yuno_must_die();
        return 0;
    }
    run_checks(gobj);
    priv->dying = TRUE;
    set_timeout(priv->timer, 10);
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************
 *              GClass registration
 ***************************************************************/
/*
 *  The driver, and each peer of a login: C_AUTHZ writes the user, the
 *  session and the token's payload in the src of a session, reads its
 *  peername, and subscribes to its EV_ON_CLOSE
 */
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag--------default-----description--*/
SDATA (DTP_BOOLEAN,     "is_peer",          SDF_RD,     "0",        "A peer of a login, not the driver"),
SDATA (DTP_STRING,      "peername",         SDF_RD,     "",         "Peer name"),
SDATA (DTP_STRING,      "__username__",     SDF_VOLATIL,"",         "The user of the session"),
SDATA (DTP_STRING,      "__session_id__",   SDF_VOLATIL,"",         "The session"),
SDATA (DTP_JSON,        "jwt_payload",      SDF_VOLATIL,"{}",       "The payload of the token of the session"),
SDATA_END()
};

PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start  = mt_start,
    .mt_stop   = mt_stop,
    .mt_play   = mt_play,
    .mt_pause  = mt_pause,
};

PRIVATE int register_c_test_token_age(void)
{
    ev_action_t st_idle[] = {
        {EV_TIMEOUT, ac_timer, 0},
        {0, 0, 0}
    };
    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };
    event_type_t event_types[] = {
        {EV_TIMEOUT,    0},
        {EV_ON_CLOSE,   EVF_OUTPUT_EVENT},
        {0, 0}
    };
    hgclass gc = gclass_create(
        C_TEST_TOKEN_AGE,
        event_types,
        states,
        &gmt,
        0,                          // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,                          // authz_table
        0,                          // command_table
        0,                          // trace_level
        0                           // gclass_flag
    );
    return gc ? 0 : -1;
}

PRIVATE int register_yuno_and_more(void)
{
    return register_c_test_token_age();
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    glog_init();
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("count_logs", 0, count_logs, 0);
    gobj_log_add_handler("count_logs", "count_logs", LOG_OPT_UP_WARNING, 0);
    gobj_log_register_handler("find_tokens", 0, find_tokens, 0);
    gobj_log_add_handler("find_tokens", "find_tokens", LOG_OPT_ALL, 0);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);
    set_auto_kill_time(20);    // a hang must not hang the suite

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);

    /*  The store must exist before C_AUTHZ autostarts (it checks is_directory).  */
    rmrdir(STORE);
    mkrdir(STORE, 02770);

    yuneta_setup(
        NULL,                   // persistent_attrs
        NULL,                   // command_parser
        NULL,                   // stats_parser
        NULL,                   // authz_checker: C_AUTHZ's
        NULL,                   // authentication_parser: C_AUTHZ's
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );

    int result = yuneta_entry_point(
        argc, argv,
        APP, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        NULL                    // cleaning
    );

    rmrdir(STORE);

    check_int("no log line carries a token of the test", s_token_logged, 0);
    size_t leaked = get_cur_system_memory();
    check_int("no memory leak", (int)leaked, 0);

    printf("\n%s: %s\n", APP, (s_result == 0 && result == 0) ? "PASS" : "FAIL");
    return s_result + result;
}
