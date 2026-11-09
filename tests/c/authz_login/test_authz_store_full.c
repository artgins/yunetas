/****************************************************************************
 *          test_authz_store_full.c
 *
 *          A login whose history the authzs store cannot write (a full
 *          disk: a file size limit of 0, RLIMIT_FSIZE, SIGXFSZ ignored).
 *
 *          test_authz_store_full (Authz on_critical_error 0, the agent's):
 *            1. alice logs in with a good token: in, its history saved
 *            2. the disk full: alice logs in again: in, and the yuno goes
 *               on; ONE "Login history cannot be saved" (after the
 *               CRITICAL of timeranger2)
 *            3. at once, alice again: in, and nothing is logged (the record
 *               is not tried inside the wait of 1 s)
 *            4. the disk back, 1.2 s later: alice again: in, no warning,
 *               ONE "Login history saved again"
 *          Up to 7.30.18 on_critical_error was LOG_OPT_EXIT_ZERO, fixed:
 *          case 2 exited the yuno with code 0 (red: "the yuno exited at the
 *          login", the atexit() guard).
 *
 *          test_authz_store_full_exit (on_critical_error not set, 2): case 2
 *          exits the yuno with code 0 at the login, as always: a yuno whose
 *          store is its role ends on a full disk. Green when the exit comes
 *          there and its line says "exiting": "0".
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>
#include <jwt.h>
#include <yunetas.h>

#ifdef EXIT_DEFAULT
    #define APP         "test_authz_store_full_exit"
    #define STORE       "/tmp/test_authz_store_full_exit_store"
    #define ON_CRITICAL ""
#else
    #define APP         "test_authz_store_full"
    #define STORE       "/tmp/test_authz_store_full_store"
    #define ON_CRITICAL "'on_critical_error': 0,"
#endif
#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DOC         "a login whose history the store cannot write"
#define APP_DATETIME    ""

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define TEST_ISS        "https://idp.test.example/realms/test"
#define ALICE           "alice@example.com"
#define NOT_SAVED       "Login history cannot be saved"
#define SAVED_AGAIN     "Login history saved again"

/*
 *  The key of the test issuer (its jwks is in the config, the same `k`).
 *  A test key, good for nothing else.
 */
PRIVATE const char *LOGIN_JWK =
    "{\"kty\":\"oct\",\"k\":\"eXVuZXRhLW9wZXJhdG9yLXRlc3Qta2V5LTAxMjM0NTY\","
    "\"alg\":\"HS256\",\"use\":\"sig\"}";

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int s_result = 0;       /* accumulated check result */
PRIVATE int s_warnings = 0;     /* logs of priority WARNING and up */
PRIVATE int s_not_saved = 0;    /* lines that say the history cannot be saved */
PRIVATE int s_saved_again = 0;  /* lines that say it is saved again */
PRIVATE int s_exiting_zero = 0; /* lines of an exit with code 0 */
PRIVATE volatile int s_in_full_login = 0;   /* inside the login with the disk full */
PRIVATE int s_done = 0;         /* the checks ended */
PRIVATE char *s_token = NULL;
PRIVATE struct rlimit s_rl_old;
PRIVATE struct sigaction s_sa_old;

GOBJ_DEFINE_GCLASS(C_TEST_AUTHZ_STORE_FULL);

typedef struct {
    hgobj   timer;
    int     phase;
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
            'gclass': 'C_TEST_AUTHZ_STORE_FULL',                    \n\
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
                " ON_CRITICAL "                                     \n\
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
                        {'id': '" ALICE "', 'roles': ['roles^operator^users']} \n\
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
        printf("FAIL %-66s got %d expected %d\n", name, got, expected);
        s_result += -1;
    } else {
        printf("ok   %-66s (%d)\n", name, got);
    }
}

PRIVATE int count_warnings(void *h, int priority, const char *bf, size_t len)
{
    s_warnings++;
    return 0;
}

PRIVATE int find_lines(void *h, int priority, const char *bf, size_t len)
{
    if(memmem(bf, len, NOT_SAVED, strlen(NOT_SAVED))) {
        s_not_saved++;
    }
    if(memmem(bf, len, SAVED_AGAIN, strlen(SAVED_AGAIN))) {
        s_saved_again++;
    }
    const char *exiting = "\"exiting\": \"0\"";
    if(memmem(bf, len, exiting, strlen(exiting))) {
        s_exiting_zero++;
    }
    return 0;
}

/*
 *  An exit before the end: green only for the exit variant, inside the login
 *  with the disk full, said with "exiting": "0"
 */
PRIVATE void check_exit(void)
{
    if(s_done) {
        return;
    }
#ifdef EXIT_DEFAULT
    if(s_in_full_login && s_exiting_zero > 0) {
        printf("ok   the yuno exited with code 0 at the login with the disk full\n");
        printf("\n%s: PASS\n", APP);
        fflush(stdout);
        _exit(0);
    }
#endif
    printf("FAIL the yuno exited before the end of the test%s\n",
        s_in_full_login? ", at the login with the disk full" : ""
    );
    printf("\n%s: FAIL\n", APP);
    fflush(stdout);
    _exit(1);
}

PRIVATE const char *make_token(void)
{
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
        return "";
    }

    time_t now = time(NULL);
    jwt_value_t jval;
    jwt_builder_enable_iat(b, 0);
    jwt_set_SET_STR(&jval, "iss", TEST_ISS);
    jwt_builder_claim_set(b, &jval);
    jwt_set_SET_STR(&jval, "sid", "session-alice");
    jwt_builder_claim_set(b, &jval);
    jwt_set_SET_STR(&jval, "email", ALICE);
    jwt_builder_claim_set(b, &jval);
    jwt_set_SET_BOOL(&jval, "email_verified", 1);
    jwt_builder_claim_set(b, &jval);
    jwt_set_SET_INT(&jval, "iat", (jwt_long_t)(now - 60));
    jwt_builder_claim_set(b, &jval);
    jwt_set_SET_INT(&jval, "exp", (jwt_long_t)(now + 30*86400));
    jwt_builder_claim_set(b, &jval);
    s_token = jwt_builder_generate(b);
    jwt_builder_free(b);
    jwks_free(jwk_set);
    return s_token? s_token : "";
}

/*
 *  A login of alice from a peer of its own: its result. Its session is
 *  closed (EV_ON_CLOSE: the logout writes the history too).
 */
PRIVATE int login(hgobj gobj)
{
    hgobj authz = gobj_find_service("authz", TRUE);

    static int n = 0;
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "peer-%d", n++);
    hgobj peer = gobj_create(name, C_TEST_AUTHZ_STORE_FULL, json_pack("{s:b, s:s}",
        "is_peer", 1,
        "peername", "127.0.0.1:40000"
    ), gobj);
    gobj_start(peer);

    json_t *response = gobj_authenticate(
        authz,
        json_pack("{s:s, s:s}", "dst_service", "checked", "jwt", s_token? s_token : ""),
        peer
    );
    int result = (int)kw_get_int(gobj, response, "result", -99, 0);
    JSON_DECREF(response)

    if(result == 0 && !empty_string(gobj_read_str_attr(peer, "__session_id__"))) {
        gobj_publish_event(peer, EV_ON_CLOSE, json_object());
    }
    gobj_stop(peer);
    gobj_destroy(peer);
    return result;
}

PRIVATE void disk_full(BOOL full)
{
    if(full) {
        struct sigaction sa_ign;
        memset(&sa_ign, 0, sizeof(sa_ign));
        sa_ign.sa_handler = SIG_IGN;
        sigaction(SIGXFSZ, &sa_ign, &s_sa_old);
        getrlimit(RLIMIT_FSIZE, &s_rl_old);
        struct rlimit rl = {0, s_rl_old.rlim_max};
        setrlimit(RLIMIT_FSIZE, &rl);
    } else {
        setrlimit(RLIMIT_FSIZE, &s_rl_old);
        sigaction(SIGXFSZ, &s_sa_old, NULL);
    }
}

/*
 *  Phase 0: cases 1-3. Phase 1 (1.2 s later): case 4.
 */
PRIVATE void run_checks(hgobj gobj, int phase)
{
    if(phase == 0) {
        make_token();

        int warnings_before = s_warnings;
        check_int("1. alice logs in", login(gobj), 0);
        check_int("1. no warning", s_warnings - warnings_before, 0);

        disk_full(TRUE);

        s_in_full_login = 1;
        int result = login(gobj);
        s_in_full_login = 0;
#ifdef EXIT_DEFAULT
        check_int("2. the yuno exited at the login with the disk full", 0, 1);
#endif
        check_int("2. the disk full: alice logs in", result, 0);
        check_int("2. ONE line: the history cannot be saved", s_not_saved, 1);

        warnings_before = s_warnings;
        check_int("3. at once: alice logs in", login(gobj), 0);
        check_int("3. nothing logged", s_warnings - warnings_before, 0);
        check_int("3. still ONE line: the history cannot be saved", s_not_saved, 1);

        disk_full(FALSE);

    } else {
        int warnings_before = s_warnings;
        check_int("4. the disk back: alice logs in", login(gobj), 0);
        check_int("4. no warning", s_warnings - warnings_before, 0);
        check_int("4. ONE line: the history saved again", s_saved_again, 1);
    }
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
    /*  The driver's token, made by libjwt with gbmem: freed before the audit  */
    if(!gobj_read_bool_attr(gobj, "is_peer")) {
        GBMEM_FREE(s_token)
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
 *  Phase 0, phase 1 (1.2 s later: past the wait of 1 s of the store), then
 *  set_yuno_must_die() from inside the running loop
 */
PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    switch(priv->phase) {
        case 0:
            run_checks(gobj, 0);
            set_timeout(priv->timer, 1200);
            break;
        case 1:
            run_checks(gobj, 1);
            set_timeout(priv->timer, 10);
            break;
        default:
            set_yuno_must_die();
            break;
    }
    priv->phase++;
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

PRIVATE int register_c_test_authz_store_full(void)
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
        C_TEST_AUTHZ_STORE_FULL,
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
    return register_c_test_authz_store_full();
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    glog_init();
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("count_warnings", 0, count_warnings, 0);
    gobj_log_add_handler("count_warnings", "count_warnings", LOG_OPT_UP_WARNING, 0);
    gobj_log_register_handler("find_lines", 0, find_lines, 0);
    gobj_log_add_handler("find_lines", "find_lines", LOG_OPT_ALL, 0);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);
    set_auto_kill_time(20);    // a hang must not hang the suite

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);

    /*  The store must exist before C_AUTHZ autostarts (it checks is_directory).  */
    rmrdir(STORE);
    mkrdir(STORE, 02770);

    atexit(check_exit);

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

    s_done = 1;
    rmrdir(STORE);

    size_t leaked = get_cur_system_memory();
    check_int("no memory leak", (int)leaked, 0);

    printf("\n%s: %s\n", APP, (s_result == 0 && result == 0) ? "PASS" : "FAIL");
    return s_result + result;
}
