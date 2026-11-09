/****************************************************************************
 *          test_authz_store_unopenable.c
 *
 *          An authzs store that is there and does not open at start up.
 *          Three stores, each built from one .c (#ifdef):
 *            - its __timeranger2__.json corrupt, in a master (the agent);
 *            - its __timeranger2__.json EMPTY, in a replica (master false,
 *              agent22): a first write that did not end, which only a
 *              master makes again;
 *            - its __timeranger2__.json MISSING, in a replica: the only
 *              store whose default exit is C_AUTHZ's own line (the other
 *              two exit before it, in timeranger2).
 *          And test_authz_store_no_directory: no store directory at all,
 *          the local access only mode of always ("No authz db, authz only
 *          to local access", never an exit): the same checks.
 *
 *          test_authz_store_corrupt, test_authz_store_empty_replica,
 *          test_authz_store_missing_replica
 *          (Authz on_critical_error 0, the agent's):
 *            1. the yuno stays up, with ONE "The authz db cannot be
 *               opened, authz only to local access"
 *            2. yuneta, no token, from 127.0.0.1: in (local access)
 *            3. alice, a good token of a key of the config: refused, as
 *               without the directory (no JWT is validated there)
 *            4. the yuno opens a treedb of its own through a C_TREEDB, as
 *               the agent opens its treedb at start up (`open-treedb`, an
 *               internal call from a src whose user is yuneta): it opens;
 *               and a question of the yuno from a src with no user: allowed
 *            5. yuneta, asked from outside (`__username__`): every
 *               permission
 *            6. alice, asked from outside: none, ONE "Authz only to local
 *               access: only yuneta has permissions" and no ERROR; and none
 *               to a peer's event (`__md_iev__`) nor to a src of another
 *               user, ONE line each
 *            7. an operator assertion signed by a key of `assertion_jwks`
 *               (config keys, not the store's): verified, its `sub` is the
 *               operator
 *            8. alice's good token at verify_jwt with no audience (a token
 *               of a login): refused, "No OAuth2 Issuer found"
 *          Up to 7.30.19 C_AUTHZ gave its C_NODE a fixed exit_on_error
 *          LOG_OPT_EXIT_ZERO: the yuno exited with code 0 at start up
 *          ("tranger NULL"; red: "the yuno exited before the end of the
 *          test"). In 7.30.20 it stayed up, but every permission was asked
 *          to the treedb it had not (4 and 5 refused, with an ERROR with a
 *          stack each: the agent exited at its open-treedb), and the
 *          assertion keys were not loaded (7 refused).
 *
 *          test_authz_store_corrupt_exit, test_authz_store_empty_replica_exit,
 *          test_authz_store_missing_replica_exit
 *          (on_critical_error not set, 2): the yuno exits with code 0 at
 *          start up, as always. Green when the exit comes before the play
 *          and its line says "exiting": "0"; for the missing one, that line
 *          is C_AUTHZ's "The authz db cannot be opened".
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <jwt.h>
#include <yunetas.h>

#if defined(NO_DIRECTORY)
    #define KIND        "no_directory"
    #define MASTER      "true"
    #define CONTENT     NULL
#elif defined(MISSING_REPLICA)
    #define KIND        "missing_replica"
    #define MASTER      "false"
    #define CONTENT     NULL
#elif defined(EMPTY_REPLICA)
    #define KIND        "empty_replica"
    #define MASTER      "false"
    #define CONTENT     ""
#else
    #define KIND        "corrupt"
    #define MASTER      "true"
    #define CONTENT     "{\"filename_mask\": \"%Y\", \"rperm"
#endif
#ifdef EXIT_DEFAULT
    #define APP         "test_authz_store_" KIND "_exit"
    #define ON_CRITICAL ""
#else
    #define APP         "test_authz_store_" KIND
    #define ON_CRITICAL "'on_critical_error': 0,"
#endif
#if defined(NO_DIRECTORY)
    #define STORE       "/tmp/test_authz_store_no_directory_store"
#elif defined(MISSING_REPLICA) && defined(EXIT_DEFAULT)
    #define STORE       "/tmp/test_authz_store_missing_replica_exit_store"
#elif defined(MISSING_REPLICA)
    #define STORE       "/tmp/test_authz_store_missing_replica_store"
#elif defined(EMPTY_REPLICA) && defined(EXIT_DEFAULT)
    #define STORE       "/tmp/test_authz_store_empty_replica_exit_store"
#elif defined(EMPTY_REPLICA)
    #define STORE       "/tmp/test_authz_store_empty_replica_store"
#elif defined(EXIT_DEFAULT)
    #define STORE       "/tmp/test_authz_store_corrupt_exit_store"
#else
    #define STORE       "/tmp/test_authz_store_corrupt_store"
#endif
#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DOC         "an authzs store that does not open"
#define APP_DATETIME    ""

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define TEST_ISS        "https://idp.test.example/realms/test"
#define ALICE           "alice@example.com"
#ifdef NO_DIRECTORY
    #define NOT_OPENED  "No authz db, authz only to local access"
#else
    #define NOT_OPENED  "The authz db cannot be opened, authz only to local access"
#endif
#define LOCAL_DENIED    "Authz only to local access: only yuneta has permissions"
#define NULL_GOBJ       "hgobj NULL or DESTROYED"
#define ASSERTION_ISS   "test-controlcenter"
#define NODE            "test-node"
#define TREEDB_NAME     "treedb_local_access"
#define TREEDBS_STORE   STORE "_treedbs"

/*
 *  The key of the test issuer (its jwks is in the config, the same `k`).
 *  A test key, good for nothing else.
 */
PRIVATE const char *LOGIN_JWK =
    "{\"kty\":\"oct\",\"k\":\"eXVuZXRhLW9wZXJhdG9yLXRlc3Qta2V5LTAxMjM0NTY\","
    "\"alg\":\"HS256\",\"use\":\"sig\"}";

/*
 *  The key of the operator assertions (EC P-256: its public part is the
 *  assertion_jwks of the config). A test key, good for nothing else.
 */
PRIVATE const char *ASSERTION_JWK =
    "{\"kid\":\"test-controlcenter\",\"kty\":\"EC\",\"crv\":\"P-256\",\"alg\":\"ES256\",\"use\":\"sig\","
    "\"x\":\"nXhUxejW5sVCIMMt5VPATGJS_nsEIvIHbw_xYLdpa1Q\",\"y\":\"EmyPj6BtdnzvQTYMrLgqtRxTZig5KqIS1hh2YEB2kW8\","
    "\"d\":\"As0_L5bAc2Ga27EKaeu0KhDW5IjXIHxKfgXw2nFDKPo\"}";

/*
 *  The schema of the treedb the yuno opens itself, as the agent its own
 */
PRIVATE char treedb_schema_local_access[] = "\
{                                                                   \n\
    'id': '" TREEDB_NAME "',                                        \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'items',                                          \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int s_result = 0;       /* accumulated check result */
PRIVATE int s_not_opened = 0;   /* lines that say the store cannot be opened */
PRIVATE int s_exiting_zero = 0; /* lines of an exit with code 0 */
PRIVATE int s_not_opened_exiting = 0;   /* C_AUTHZ's line, exiting with code 0 */
PRIVATE int s_local_denied = 0; /* refusals of the local access only mode */
PRIVATE int s_null_gobj = 0;    /* questions to a treedb that is not there */
PRIVATE int s_played = 0;       /* the driver reached its play */
PRIVATE int s_done = 0;         /* the checks ended */
PRIVATE char *s_token = NULL;

GOBJ_DEFINE_GCLASS(C_TEST_AUTHZ_UNOPENABLE);

typedef struct {
    hgobj   timer;
    hgobj   gobj_treedbs;
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
            'gclass': 'C_TEST_AUTHZ_UNOPENABLE',                    \n\
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
                'master': " MASTER ",                               \n\
                " ON_CRITICAL "                                     \n\
                'assertion_jwks': [                                 \n\
                    {                                               \n\
                        'kid': '" ASSERTION_ISS "',                 \n\
                        'kty': 'EC',                                \n\
                        'crv': 'P-256',                             \n\
                        'alg': 'ES256',                             \n\
                        'use': 'sig',                               \n\
                        'x': 'nXhUxejW5sVCIMMt5VPATGJS_nsEIvIHbw_xYLdpa1Q', \n\
                        'y': 'EmyPj6BtdnzvQTYMrLgqtRxTZig5KqIS1hh2YEB2kW8'  \n\
                    }                                               \n\
                ],                                                  \n\
                'jwks': [                                           \n\
                    {                                               \n\
                        'kid': '" TEST_ISS "',                      \n\
                        'kty': 'oct',                               \n\
                        'alg': 'HS256',                             \n\
                        'k': 'eXVuZXRhLW9wZXJhdG9yLXRlc3Qta2V5LTAxMjM0NTY' \n\
                    }                                               \n\
                ]                                                   \n\
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

PRIVATE int find_lines(void *h, int priority, const char *bf, size_t len)
{
    if(memmem(bf, len, NOT_OPENED, strlen(NOT_OPENED))) {
        s_not_opened++;
    }
    const char *exiting = "\"exiting\": \"0\"";
    if(memmem(bf, len, exiting, strlen(exiting))) {
        s_exiting_zero++;
        if(memmem(bf, len, NOT_OPENED, strlen(NOT_OPENED))) {
            s_not_opened_exiting++;
        }
    }
    if(memmem(bf, len, LOCAL_DENIED, strlen(LOCAL_DENIED))) {
        s_local_denied++;
    }
    if(memmem(bf, len, NULL_GOBJ, strlen(NULL_GOBJ))) {
        s_null_gobj++;
    }
    return 0;
}

/*
 *  An exit before the end: green only for the exit variant, at start up
 *  (before the play of the driver), said with "exiting": "0"
 */
PRIVATE void check_exit(void)
{
    if(s_done) {
        return;
    }
    rmrdir(STORE);
    rmrdir(TREEDBS_STORE);
#ifdef EXIT_DEFAULT
#ifdef MISSING_REPLICA
    int exited = s_not_opened_exiting;
#else
    int exited = s_exiting_zero;
#endif
    if(!s_played && exited > 0) {
        printf("ok   the yuno exited with code 0 at start up%s\n",
            s_not_opened_exiting? ", by C_AUTHZ's line" : ""
        );
        printf("\n%s: PASS\n", APP);
        fflush(stdout);
        _exit(0);
    }
#endif
    printf("FAIL the yuno exited before the end of the test%s\n",
        s_played? "" : ", at start up"
    );
    printf("\n%s: FAIL\n", APP);
    fflush(stdout);
    _exit(1);
}

/*
 *  A token of `claims` (owned) signed by the key of `jwk_json`: yours
 *  (GBMEM_FREE), or NULL
 */
PRIVATE char *sign_token(const char *jwk_json, jwt_alg_t alg, json_t *claims)
{
    jwk_set_t *jwk_set = jwks_create(jwk_json);
    const jwk_item_t *key = jwk_set? jwks_item_get(jwk_set, 0) : NULL;
    jwt_builder_t *b = key? jwt_builder_new() : NULL;
    if(!b || jwt_builder_setkey(b, alg, key) != 0) {
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
        return NULL;
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

    char *token = jwt_builder_generate(b);
    jwt_builder_free(b);
    jwks_free(jwk_set);
    JSON_DECREF(claims)
    return token;
}

PRIVATE const char *make_token(void)
{
    time_t now = time(NULL);
    s_token = sign_token(LOGIN_JWK, JWT_ALG_HS256, json_pack("{s:s, s:s, s:s, s:b, s:I, s:I}",
        "iss", TEST_ISS,
        "sid", "session-alice",
        "email", ALICE,
        "email_verified", 1,
        "iat", (json_int_t)(now - 60),
        "exp", (json_int_t)(now + 30*86400)
    ));
    return s_token? s_token : "";
}

/*
 *  A login from a peer of its own at 127.0.0.1: its result
 */
PRIVATE int login(hgobj gobj, const char *username, const char *jwt)
{
    hgobj authz = gobj_find_service("authz", TRUE);

    static int n = 0;
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "peer-%d", n++);
    hgobj peer = gobj_create(name, C_TEST_AUTHZ_UNOPENABLE, json_pack("{s:b, s:s}",
        "is_peer", 1,
        "peername", "127.0.0.1:40000"
    ), gobj);
    gobj_start(peer);

    json_t *response = gobj_authenticate(
        authz,
        json_pack("{s:s, s:s, s:s}",
            "dst_service", "checked",
            "username", username,
            "jwt", jwt
        ),
        peer
    );
    int result = (int)kw_get_int(gobj, response, "result", -99, 0);
    JSON_DECREF(response)

    gobj_stop(peer);
    gobj_destroy(peer);
    return result;
}

/*
 *  The yuno opens a treedb of its own, as the agent opens its treedb at
 *  start up (agent_treedb.c): an internal open-treedb, from a gobj whose
 *  __username__ is the yuno's. The answer's result
 */
PRIVATE int open_own_treedb(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_str_attr(gobj, "__username__", gobj_read_str_attr(gobj_yuno(), "__username__"));
    gobj_start_tree(priv->gobj_treedbs);

    json_t *jn_resp = gobj_command(priv->gobj_treedbs,
        "open-treedb",
        json_pack("{s:s, s:i, s:s, s:o, s:b}",
            "filename_mask", "%Y",
            "exit_on_error", 0,
            "treedb_name", TREEDB_NAME,
            "treedb_schema", legalstring2json(treedb_schema_local_access, TRUE),
            "impose_c_schema", 1
        ),
        gobj
    );
    int result = (int)kw_get_int(gobj, jn_resp, "result", -99, 0);
    if(result < 0) {
        printf("     open-treedb: %s\n", kw_get_str(gobj, jn_resp, "comment", "", 0));
    }
    JSON_DECREF(jn_resp)
    return result;
}

/*
 *  A permission of the C_TREEDB asked for `username` as a peer's command
 *  asks it (its `__username__` in the kw)
 */
PRIVATE int asked_from_outside(hgobj gobj, const char *username)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    return gobj_user_has_authz(
        priv->gobj_treedbs,
        "open-close",
        json_pack("{s:s}", "__username__", username),
        gobj
    )? 1 : 0;
}

/*
 *  A permission of the C_TREEDB asked with `kw` (owned) from `src`
 */
PRIVATE int asked_from(hgobj gobj, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    return gobj_user_has_authz(priv->gobj_treedbs, "open-close", kw, src)? 1 : 0;
}

/*
 *  verify_jwt of the C_AUTHZ: its result; `username` is set with the
 *  username it answers
 */
PRIVATE int verify_jwt(hgobj gobj, const char *jwt, const char *audience, char *username, size_t size)
{
    hgobj authz = gobj_find_service("authz", TRUE);
    json_t *jn_resp = gobj_local_method(
        authz,
        "verify_jwt",
        json_pack("{s:s, s:s}", "jwt", jwt? jwt : "", "audience", audience),
        gobj
    );
    int result = (int)kw_get_int(gobj, jn_resp, "result", -99, 0);
    snprintf(username, size, "%s", kw_get_str(gobj, jn_resp, "username", "", 0));
    if(result < 0) {
        printf("     verify_jwt: %s\n", kw_get_str(gobj, jn_resp, "comment", "", 0));
    }
    JSON_DECREF(jn_resp)
    return result;
}

PRIVATE void run_checks(hgobj gobj)
{
    make_token();

#ifdef EXIT_DEFAULT
    check_int("1. the yuno exited at start up", 0, 1);
#endif
    check_int("1. the yuno is up: ONE line, authz only to local access", s_not_opened, 1);
    check_int("2. yuneta, no token, from 127.0.0.1: in", login(gobj, "yuneta", ""), 0);
    check_int("3. alice, a good token: refused, no JWT is validated", login(gobj, "", s_token? s_token : ""), -1);

    check_int("4. the yuno opens its own treedb (open-treedb, internal)", open_own_treedb(gobj), 0);
    check_int("4. its treedb is there", gobj_find_service(TREEDB_NAME, FALSE)? 1 : 0, 1);

    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    check_int("4. the yuno asking itself, a src with no user: allowed",
        asked_from(gobj, json_object(), priv->gobj_treedbs), 1);

    check_int("5. yuneta, asked from outside: allowed", asked_from_outside(gobj, "yuneta"), 1);
    int denied_before = s_local_denied;
    check_int("6. alice, asked from outside: refused", asked_from_outside(gobj, ALICE), 0);
    check_int("6. ONE line says why", s_local_denied - denied_before, 1);
    check_int("6. a peer's event (__md_iev__), a src with no user: refused",
        asked_from(gobj, json_pack("{s:{}}", "__md_iev__"), priv->gobj_treedbs), 0);
    gobj_write_str_attr(gobj, "__username__", ALICE);
    check_int("6. no __username__ in kw, a src of another user: refused",
        asked_from(gobj, json_object(), gobj), 0);
    gobj_write_str_attr(gobj, "__username__", "yuneta");
    check_int("6. ONE line for each refusal", s_local_denied - denied_before, 3);
    check_int("6. no question went to a treedb that is not there", s_null_gobj, 0);

    char username[NAME_MAX];
    time_t now = time(NULL);
    char *assertion = sign_token(ASSERTION_JWK, JWT_ALG_ES256, json_pack("{s:s, s:s, s:s, s:I, s:I}",
        "iss", ASSERTION_ISS,
        "sub", ALICE,
        "aud", NODE,
        "iat", (json_int_t)now,
        "exp", (json_int_t)(now + 60)
    ));
    check_int("7. an operator assertion (assertion_jwks): verified",
        verify_jwt(gobj, assertion, NODE, username, sizeof(username)), 0);
    check_int("7. its operator is its sub", strcmp(username, ALICE)==0? 1 : 0, 1);
    GBMEM_FREE(assertion)

    check_int("8. alice's good token at verify_jwt, a login's: refused",
        verify_jwt(gobj, s_token, "", username, sizeof(username)), -1);
}

/***************************************************************
 *              Framework Methods
 ***************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!gobj_read_bool_attr(gobj, "is_peer")) {
        priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

        /*
         *  A C_TREEDB service as the agent's (c_agent.c), started and
         *  asked for its treedb after the play
         */
        priv->gobj_treedbs = gobj_create_service(
            "treedbs",
            C_TREEDB,
            json_pack("{s:s, s:s, s:b, s:i, s:i, s:i}",
                "path", TREEDBS_STORE,
                "filename_mask", "%Y",
                "master", 1,
                "xpermission", 02770,
                "rpermission", 0660,
                "exit_on_error", 0
            ),
            gobj
        );
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
    if(priv->gobj_treedbs && gobj_is_running(priv->gobj_treedbs)) {
        gobj_stop_tree(priv->gobj_treedbs);
    }
    return 0;
}

PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->timer) {
        s_played = 1;
        set_timeout(priv->timer, 10);   // fire once, inside the loop
    }
    return 0;
}

PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->timer) {
        clear_timeout(priv->timer);
    }
    return 0;
}

/***************************************************************
 *              Actions
 ***************************************************************/
PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    run_checks(gobj);
    set_yuno_must_die();
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************
 *              GClass registration
 ***************************************************************/
/*
 *  The driver, and each peer of a login: C_AUTHZ writes the user and the
 *  session in the src of a session, and reads its peername
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

PRIVATE int register_c_test_authz_unopenable(void)
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
        C_TEST_AUTHZ_UNOPENABLE,
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
    return register_c_test_authz_unopenable();
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    glog_init();
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("find_lines", 0, find_lines, 0);
    gobj_log_add_handler("find_lines", "find_lines", LOG_OPT_ALL, 0);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);
    set_auto_kill_time(20);    // a hang must not hang the suite

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    helper_quote2doublequote(treedb_schema_local_access);

    /*
     *  The store is there (C_AUTHZ checks is_directory), its metadata
     *  corrupt, empty or missing
     */
    rmrdir(STORE);
    rmrdir(TREEDBS_STORE);
#ifndef NO_DIRECTORY
    mkrdir(STORE, 02770);
#endif
    const char *content = CONTENT;
    if(content) {
        char path[PATH_MAX];
        if(!build_path(path, sizeof(path), STORE, "__timeranger2__.json", NULL)) {
            return -1;  // Error already logged
        }
        FILE *f = fopen(path, "w");
        if(!f) {
            printf("FAIL the store's metadata cannot be written: %s\n", path);
            return -1;
        }
        fputs(content, f);
        fclose(f);
    }

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
    rmrdir(TREEDBS_STORE);

    size_t leaked = get_cur_system_memory();
    check_int("no memory leak", (int)leaked, 0);

    printf("\n%s: %s\n", APP, (s_result == 0 && result == 0) ? "PASS" : "FAIL");
    return s_result + result;
}
