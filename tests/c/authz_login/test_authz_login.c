/****************************************************************************
 *          test_authz_login.c
 *
 *          Who C_AUTHZ lets in at a login, and what its refusals write.
 *
 *          A peer with no token and no password logs in by its address
 *          alone: from 127.0.0.* as the local user `yuneta`, with its real
 *          roles, or from an address of the yuno's `allowed_ips`. A proxy on
 *          the same host (nginx in front of a C_IEVENT_SRV) makes every
 *          browser such a peer. `trust_peer_ip` false turns that off; a
 *          token or a password is then the only way in.
 *
 *          test_authz_login (trust_peer_ip true, the default):
 *            1. yuneta, no token, from 127.0.0.1: in, a superuser
 *            2. yuneta, no token, from 10.1.2.3: refused, "Without JWT/passw
 *               only localhost is allowed", ONE warning
 *            3. yuneta, no token, from 10.1.2.4 (in allowed_ips): in
 *          test_authz_login_no_peer_ip (trust_peer_ip false):
 *            1-3. refused, "Without JWT/passw the peer address is not
 *               trusted", ONE warning each
 *          In both:
 *            4. alice, a good token, from 127.0.0.1: in
 *            5. alice, a token signed by another key: refused, "Authentication
 *               failed: Invalid token", ONE warning; no log line carries the
 *               token (up to 7.30.14 the warning carried it whole)
 *            6. a good token with no `email`: refused, "Username must be an
 *               email address", a warning, no error (up to 7.30.14 an ERROR
 *               of the reader first)
 *            7. a good token with no `email_verified`: refused, "Email not
 *               verified", a warning, no error (the same)
 *            8. alice, a good token expired an hour ago, a claim of its own:
 *               refused; no log line carries its claims (up to 7.30.14 libjwt
 *               traced the claims of every token whose claims failed)
 *            9. a token with no `iss`, signed by the key of the test
 *               issuer: refused, a warning, no error (up to 7.30.14 an ERROR
 *               of the reader of `iss` first)
 *           10. alice, a token expired AND not valid yet: refused, its status
 *               names both claims, "Failed claims: exp, nbf" (up to 7.30.14
 *               "What JWT_CLAIM_???")
 *           11. the config's key of kid `*`, before the key of the test
 *               issuer: refused at start up with ONE error, and the key after
 *               it still logs alice in (case 4). Since 7.2.0 such a key
 *               validated nothing, and nobody was told
 *           12. add-jwk kid=*: refused, its comment says why, no log
 *           13. remove-jwk kid=*: refused, it was never loaded, no log (up
 *               to 7.30.18 it answered 0 "deleted" and logged three ERRORs,
 *               one with a stack)
 *           14. the two keys of a rotation (one kid): a token of the second
 *               logs in; remove-jwk of the kid removes both, logs nothing,
 *               and the token is refused (up to 7.30.18 only the first key
 *               went, and the second went on validating until a restart)
 *           15. bob, a password, from 10.1.2.3: in; EV_AUTHZ_USER_LOGIN
 *               carries username, dst_service, user, session and
 *               services_roles, and no jwt_payload (a login with a token,
 *               case 4, carries its payload); EV_AUTHZ_USER_LOGOUT of the
 *               close carries the session. Up to 7.30.26 a login without a
 *               token published an EMPTY kw
 *           16. bob with max_sessions 1 logs in from a second peer: the
 *               first session is dropped (EV_DROP, ONE warning); the close
 *               of the dropped peer publishes EV_AUTHZ_USER_LOGOUT with
 *               username and user and no session (up to 7.30.26 an EMPTY
 *               kw), the close of the second one with its session
 *          and no memory is left.
 *
 *          A real C_AUTHZ service over a temp tranger store, its roles and
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

#ifdef NO_PEER_IP
    #define APP         "test_authz_login_no_peer_ip"
    #define TRUST       "false"
    #define STORE       "/tmp/test_authz_login_no_peer_ip_store"
#else
    #define APP         "test_authz_login"
    #define TRUST       "true"
    #define STORE       "/tmp/test_authz_login_store"
#endif
#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DOC         "who C_AUTHZ lets in at a login"
#define APP_DATETIME    ""

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define TEST_ISS        "https://idp.test.example/realms/test"
#define ROTATE_ISS      "https://idp.test.example/realms/rotate"
#define ALICE           "alice@example.com"
#define CLAIMS_MARKER   "claims-marker-of-the-test-0451"
#define MAX_TOKENS      10

#define BOB             "bob@example.com"
#define BOB_PASSWORD    "bob-password-of-the-test"
#define NOT_TRUSTED     "Without JWT/passw the peer address is not trusted"
#define KID_STAR_REFUSED "A jwks key with kid '*' is not allowed"

/*
 *  The key of the test issuer (its jwks is in the config, the same `k`),
 *  and another one nobody trusts: base64url of 32 ASCII bytes each. Test
 *  keys, good for nothing else.
 */
PRIVATE const char *LOGIN_JWK =
    "{\"kty\":\"oct\",\"k\":\"eXVuZXRhLW9wZXJhdG9yLXRlc3Qta2V5LTAxMjM0NTY\","
    "\"alg\":\"HS256\",\"use\":\"sig\"}";
PRIVATE const char *OTHER_JWK =
    "{\"kty\":\"oct\",\"k\":\"YW5vdGhlci1rZXktbm9ib2R5LXRydXN0cy0wMTIzNDU\","
    "\"alg\":\"HS256\",\"use\":\"sig\"}";

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int s_result = 0;       /* accumulated check result, read after entry_point */
PRIVATE int s_warnings = 0;     /* logs of priority WARNING and up */
PRIVATE int s_errors = 0;       /* logs of priority ERROR and up */
PRIVATE int s_token_logged = 0; /* log lines that carry a token of the test */
PRIVATE int s_claims_logged = 0;/* log lines that carry the claims of a token */
PRIVATE int s_claims_named = 0; /* log lines that name the claims that failed */
PRIVATE int s_claims_unnamed = 0;/* log lines with the old "What JWT_CLAIM_???" */
PRIVATE int s_kid_star_refused = 0;/* log lines that refuse the key of kid '*' */
PRIVATE char *s_tokens[MAX_TOKENS] = {0};
PRIVATE json_t *s_login_kw = NULL;  /* the kw of the last EV_AUTHZ_USER_LOGIN */
PRIVATE json_t *s_logout_kw = NULL; /* the kw of the last EV_AUTHZ_USER_LOGOUT */
PRIVATE int s_drops = 0;            /* EV_DROP received by a peer */

GOBJ_DEFINE_GCLASS(C_TEST_AUTHZ_LOGIN);

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
            'gclass': 'C_TEST_AUTHZ_LOGIN',                         \n\
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
                'trust_peer_ip': " TRUST ",                         \n\
                'jwks': [                                           \n\
                    {                                               \n\
                        'kid': '*',                                 \n\
                        'kty': 'oct',                               \n\
                        'alg': 'HS256',                             \n\
                        'k': 'eXVuZXRhLW9wZXJhdG9yLXRlc3Qta2V5LTAxMjM0NTY' \n\
                    },                                              \n\
                    {                                               \n\
                        'kid': '" ROTATE_ISS "',                    \n\
                        'kty': 'oct',                               \n\
                        'alg': 'HS256',                             \n\
                        'k': 'eXVuZXRhLW9wZXJhdG9yLXRlc3Qta2V5LTAxMjM0NTY' \n\
                    },                                              \n\
                    {                                               \n\
                        'kid': '" ROTATE_ISS "',                    \n\
                        'kty': 'oct',                               \n\
                        'alg': 'HS256',                             \n\
                        'k': 'YW5vdGhlci1rZXktbm9ib2R5LXRydXN0cy0wMTIzNDU' \n\
                    },                                              \n\
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
                            'id': 'root',                           \n\
                            'description': 'everything',            \n\
                            'realm_id': '*',                        \n\
                            'service': '*',                         \n\
                            'permission': '*'                       \n\
                        },                                          \n\
                        {                                           \n\
                            'id': 'operator',                       \n\
                            'description': 'may log in checked',    \n\
                            'realm_id': '*',                        \n\
                            'service': 'checked'                    \n\
                        }                                           \n\
                    ],                                              \n\
                    'users': [                                      \n\
                        {'id': 'yuneta', 'roles': ['roles^root^users']}, \n\
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

PRIVATE void check_str(const char *name, const char *got, const char *expected)
{
    if(strcmp(got, expected)!=0) {
        printf("FAIL %-66s got '%s' expected '%s'\n", name, got, expected);
        s_result += -1;
    } else {
        printf("ok   %-66s ('%s')\n", name, got);
    }
}

/*
 *  Counts the warnings (and up) and the errors (and up): a check reads the
 *  difference around one call
 */
PRIVATE int count_logs(void *h, int priority, const char *bf, size_t len)
{
    s_warnings++;
    if(priority <= LOG_ERR) {
        s_errors++;
    }
    return 0;
}

/*
 *  Every line, of any priority: does it carry a token of the test, or the
 *  claims of one?
 */
PRIVATE int find_tokens(void *h, int priority, const char *bf, size_t len)
{
    for(int i=0; i<MAX_TOKENS; i++) {
        if(s_tokens[i] && memmem(bf, len, s_tokens[i], strlen(s_tokens[i]))) {
            s_token_logged++;
        }
    }
    if(memmem(bf, len, CLAIMS_MARKER, strlen(CLAIMS_MARKER))) {
        s_claims_logged++;
    }
    const char *named = "Failed claims: exp, nbf";
    if(memmem(bf, len, named, strlen(named))) {
        s_claims_named++;
    }
    const char *unnamed = "What JWT_CLAIM_";
    if(memmem(bf, len, unnamed, strlen(unnamed))) {
        s_claims_unnamed++;
    }
    if(memmem(bf, len, KID_STAR_REFUSED, strlen(KID_STAR_REFUSED))) {
        s_kid_star_refused++;
    }
    return 0;
}

/*
 *  A token of the test issuer: its claims, signed with `jwk`. Kept in
 *  s_tokens (gbmem) to look for in the log and to be freed at the end.
 */
PRIVATE const char *make_token(int slot, const char *jwk, json_t *claims /* owned */)
{
    jwk_set_t *jwk_set = jwks_create(jwk);
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

    jwt_builder_enable_iat(b, 0);   // the iat is the claims'
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
 *  The claims of a good login token of alice, issued a minute ago, expiring
 *  in a month
 */
PRIVATE json_t *alice_claims(void)
{
    time_t now = time(NULL);
    return json_pack("{s:s, s:s, s:s, s:b, s:I, s:I}",
        "iss", TEST_ISS,
        "sid", "session-alice",
        "email", ALICE,
        "email_verified", 1,
        "iat", (json_int_t)(now - 60),
        "exp", (json_int_t)(now + 30*86400)
    );
}

/*
 *  A login from a peer of its own, `peername`, with `jwt` (or none):
 *  its result, its comment, the warnings and errors it logged, and the user
 *  of the session. A session opened is closed (EV_ON_CLOSE) and the peer
 *  goes.
 */
PRIVATE void check_login(
    hgobj gobj,
    const char *what,
    const char *peername,
    const char *username,   // no token: the user it says it is
    const char *jwt,        // or NULL
    int expected_result,
    const char *expected_comment,   // only for a refusal
    int expected_warnings,
    int expected_superuser  // -1: not checked
)
{
    hgobj authz = gobj_find_service("authz", TRUE);

    static int n = 0;
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "peer-%d", n++);
    hgobj peer = gobj_create(name, C_TEST_AUTHZ_LOGIN, json_pack("{s:b, s:s}",
        "is_peer", 1,
        "peername", peername
    ), gobj);
    gobj_start(peer);

    json_t *kw_login = json_pack("{s:s}", "dst_service", "checked");
    if(jwt) {
        json_object_set_new(kw_login, "jwt", json_string(jwt));
    } else {
        json_object_set_new(kw_login, "username", json_string(username));
    }

    int warnings_before = s_warnings;
    int errors_before = s_errors;
    json_t *response = gobj_authenticate(authz, kw_login, peer);

    int result = (int)kw_get_int(gobj, response, "result", -99, 0);
    char label[NAME_MAX];
    snprintf(label, sizeof(label), "%s: result", what);
    check_int(label, result, expected_result);
    if(expected_result < 0) {
        snprintf(label, sizeof(label), "%s: comment", what);
        check_str(label, kw_get_str(gobj, response, "comment", "", 0), expected_comment);
    } else {
        snprintf(label, sizeof(label), "%s: the user of the session", what);
        check_str(label, gobj_read_str_attr(peer, "__username__"), jwt? ALICE : username);
    }
    if(expected_superuser >= 0) {
        snprintf(label, sizeof(label), "%s: superuser", what);
        check_int(label, kw_get_bool(gobj, response, "superuser", 0, 0)? 1 : 0, expected_superuser);
    }
    snprintf(label, sizeof(label), "%s: warnings", what);
    check_int(label, s_warnings - warnings_before, expected_warnings);
    snprintf(label, sizeof(label), "%s: errors", what);
    check_int(label, s_errors - errors_before, 0);
    JSON_DECREF(response)

    if(result == 0 && !empty_string(gobj_read_str_attr(peer, "__session_id__"))) {
        gobj_publish_event(peer, EV_ON_CLOSE, json_object());
    }
    gobj_stop(peer);
    gobj_destroy(peer);
}

/*
 *  A peer of a login: a child of the driver, as C_IEVENT_SRV is for a
 *  real session
 */
PRIVATE hgobj open_peer(hgobj gobj, const char *name, const char *peername)
{
    hgobj peer = gobj_create(name, C_TEST_AUTHZ_LOGIN, json_pack("{s:b, s:s}",
        "is_peer", 1,
        "peername", peername
    ), gobj);
    gobj_start(peer);
    return peer;
}

PRIVATE int login_by_password(hgobj peer, const char *username, const char *password)
{
    hgobj authz = gobj_find_service("authz", TRUE);
    json_t *response = gobj_authenticate(
        authz,
        json_pack("{s:s, s:s, s:s}",
            "dst_service", "checked",
            "username", username,
            "password", password
        ),
        peer
    );
    int result = (int)kw_get_int(0, response, "result", -99, 0);
    JSON_DECREF(response)
    return result;
}

/*
 *  The session of the peer ends (EV_ON_CLOSE) and the peer goes
 */
PRIVATE void close_peer(hgobj peer)
{
    if(!empty_string(gobj_read_str_attr(peer, "__session_id__"))) {
        gobj_publish_event(peer, EV_ON_CLOSE, json_object());
    }
    gobj_stop(peer);
    gobj_destroy(peer);
}

/*
 *  15, 16. What EV_AUTHZ_USER_LOGIN and EV_AUTHZ_USER_LOGOUT carry for a
 *  login without a token
 */
PRIVATE void check_password_login_events(hgobj gobj)
{
    hgobj authz = gobj_find_service("authz", TRUE);

    json_t *r = gobj_command(authz, "create-user",
        json_pack("{s:s, s:s, s:s}",
            "username", BOB,
            "role", "roles^operator^users",
            "password", BOB_PASSWORD
        ),
        gobj
    );
    check_int("15. create-user bob with a password: result",
        (int)kw_get_int(gobj, r, "result", -99, 0), 0);
    JSON_DECREF(r)

    /*
     *  15. bob, by password
     */
    int warnings_before = s_warnings;
    int errors_before = s_errors;
    JSON_DECREF(s_login_kw)
    hgobj peer = open_peer(gobj, "peer-bob", "10.1.2.3:40000");
    check_int("15. bob, a password, from 10.1.2.3: result",
        login_by_password(peer, BOB, BOB_PASSWORD), 0);
    const char *session_id = gobj_read_str_attr(peer, "__session_id__");
    check_int("15. bob: the session has an id", empty_string(session_id)? 0 : 1, 1);

    check_int("15. EV_AUTHZ_USER_LOGIN published", s_login_kw? 1 : 0, 1);
    check_str("15. EV_AUTHZ_USER_LOGIN: username",
        kw_get_str(gobj, s_login_kw, "username", "", 0), BOB);
    check_str("15. EV_AUTHZ_USER_LOGIN: dst_service",
        kw_get_str(gobj, s_login_kw, "dst_service", "", 0), "checked");
    check_str("15. EV_AUTHZ_USER_LOGIN: user`id",
        kw_get_str(gobj, s_login_kw, "user`id", "", 0), BOB);
    check_str("15. EV_AUTHZ_USER_LOGIN: session`id",
        kw_get_str(gobj, s_login_kw, "session`id", "", 0), session_id);
    check_int("15. EV_AUTHZ_USER_LOGIN: services_roles has checked",
        kw_has_key(kw_get_dict(gobj, s_login_kw, "services_roles", 0, 0), "checked")? 1 : 0, 1);
    check_int("15. EV_AUTHZ_USER_LOGIN: no jwt_payload",
        kw_has_key(s_login_kw, "jwt_payload")? 1 : 0, 0);

    JSON_DECREF(s_logout_kw)
    close_peer(peer);
    check_str("15. EV_AUTHZ_USER_LOGOUT: username",
        kw_get_str(gobj, s_logout_kw, "username", "", 0), BOB);
    check_int("15. EV_AUTHZ_USER_LOGOUT: its session", json_is_object(kw_get_dict(gobj, s_logout_kw, "session", 0, 0))? 1 : 0, 1);
    check_int("15. warnings", s_warnings - warnings_before, 0);
    check_int("15. errors", s_errors - errors_before, 0);

    /*
     *  16. bob with one session at most, from two peers
     */
    r = gobj_command(authz, "set-max-sessions",
        json_pack("{s:s, s:i}", "username", BOB, "max_sessions", 1),
        gobj
    );
    check_int("16. set-max-sessions bob 1: result",
        (int)kw_get_int(gobj, r, "result", -99, 0), 0);
    JSON_DECREF(r)

    warnings_before = s_warnings;
    errors_before = s_errors;
    int drops_before = s_drops;
    hgobj peer_a = open_peer(gobj, "peer-bob-a", "10.1.2.3:40001");
    hgobj peer_b = open_peer(gobj, "peer-bob-b", "10.1.2.3:40002");
    check_int("16. bob from peer a: result", login_by_password(peer_a, BOB, BOB_PASSWORD), 0);
    check_int("16. bob from peer b: result", login_by_password(peer_b, BOB, BOB_PASSWORD), 0);
    check_int("16. the session of peer a dropped", s_drops - drops_before, 1);
    check_int("16. warnings (the drop)", s_warnings - warnings_before, 1);

    JSON_DECREF(s_logout_kw)
    close_peer(peer_a);
    check_int("16. the close of the dropped peer: EV_AUTHZ_USER_LOGOUT published",
        s_logout_kw? 1 : 0, 1);
    check_str("16. the close of the dropped peer: username",
        kw_get_str(gobj, s_logout_kw, "username", "", 0), BOB);
    check_str("16. the close of the dropped peer: user`id",
        kw_get_str(gobj, s_logout_kw, "user`id", "", 0), BOB);
    check_int("16. the close of the dropped peer: no session",
        kw_has_key(s_logout_kw, "session")? 1 : 0, 0);

    JSON_DECREF(s_logout_kw)
    close_peer(peer_b);
    check_str("16. the close of peer b: username",
        kw_get_str(gobj, s_logout_kw, "username", "", 0), BOB);
    check_int("16. the close of peer b: its session",
        json_is_object(kw_get_dict(gobj, s_logout_kw, "session", 0, 0))? 1 : 0, 1);
    check_int("16. errors", s_errors - errors_before, 0);
}

PRIVATE void run_checks(hgobj gobj)
{
#ifdef NO_PEER_IP
    const int trusted = 0;
#else
    const int trusted = 1;
#endif
    time_t now = time(NULL);

    hgobj authz_ = gobj_find_service("authz", TRUE);
    gobj_subscribe_event(authz_, EV_AUTHZ_USER_LOGIN, 0, gobj);
    gobj_subscribe_event(authz_, EV_AUTHZ_USER_LOGOUT, 0, gobj);

    /*
     *  11. Start up is over: the key of kid '*' was refused, once, and that
     *  was the only error (the key after it is case 4)
     */
    check_int("11. the key of kid '*' refused at start up", s_kid_star_refused, 1);
    check_int("11. errors at start up", s_errors, 1);

    /*
     *  An address the yuno allows
     */
    json_object_set_new(
        gobj_read_json_attr(gobj_yuno(), "allowed_ips"), "10.1.2.4", json_true()
    );

    check_login(gobj, "1. yuneta, no token, from 127.0.0.1", "127.0.0.1:40000",
        "yuneta", NULL,
        trusted? 0 : -1, NOT_TRUSTED, trusted? 0 : 1, trusted? 1 : -1);
    check_login(gobj, "2. yuneta, no token, from 10.1.2.3", "10.1.2.3:40000",
        "yuneta", NULL,
        -1, trusted? "Without JWT/passw only localhost is allowed" : NOT_TRUSTED, 1, -1);
    check_login(gobj, "3. yuneta, no token, from 10.1.2.4 (allowed_ips)", "10.1.2.4:40000",
        "yuneta", NULL,
        trusted? 0 : -1, NOT_TRUSTED, trusted? 0 : 1, -1);

    json_object_del(gobj_read_json_attr(gobj_yuno(), "allowed_ips"), "10.1.2.4");

    JSON_DECREF(s_login_kw)
    check_login(gobj, "4. alice, a good token, from 127.0.0.1", "127.0.0.1:40000",
        "", make_token(0, LOGIN_JWK, alice_claims()),
        0, "", 0, 0);
    check_str("4. EV_AUTHZ_USER_LOGIN: username",
        kw_get_str(gobj, s_login_kw, "username", "", 0), ALICE);
    check_str("4. EV_AUTHZ_USER_LOGIN: jwt_payload`email",
        kw_get_str(gobj, s_login_kw, "jwt_payload`email", "", 0), ALICE);

    check_login(gobj, "5. alice, a token of another key", "127.0.0.1:40000",
        "", make_token(1, OTHER_JWK, alice_claims()),
        -1, "Authentication failed: Invalid token", 1, -1);

    json_t *claims = alice_claims();
    json_object_del(claims, "email");
    check_login(gobj, "6. a good token with no email", "127.0.0.1:40000",
        "", make_token(2, LOGIN_JWK, claims),
        -1, "Username must be an email address", 1, -1);

    claims = alice_claims();
    json_object_del(claims, "email_verified");
    check_login(gobj, "7. a good token with no email_verified", "127.0.0.1:40000",
        "", make_token(3, LOGIN_JWK, claims),
        -1, "Email not verified", 1, -1);

    claims = alice_claims();
    json_object_set_new(claims, "iat", json_integer((json_int_t)(now - 7200)));
    json_object_set_new(claims, "exp", json_integer((json_int_t)(now - 3600)));
    json_object_set_new(claims, "name", json_string(CLAIMS_MARKER));
    check_login(gobj, "8. alice, a good token expired an hour ago", "127.0.0.1:40000",
        "", make_token(4, LOGIN_JWK, claims),
        -1, "Authentication failed: Invalid token", 1, -1);

    claims = alice_claims();
    json_object_del(claims, "iss");
    check_login(gobj, "9. a token with no iss", "127.0.0.1:40000",
        "", make_token(5, LOGIN_JWK, claims),
        -1, "Authentication failed: Invalid token", 1, -1);

    claims = alice_claims();
    json_object_set_new(claims, "exp", json_integer((json_int_t)(now - 3600)));
    json_object_set_new(claims, "nbf", json_integer((json_int_t)(now + 3600)));
    check_login(gobj, "10. alice, expired and not valid yet", "127.0.0.1:40000",
        "", make_token(6, LOGIN_JWK, claims),
        -1, "Authentication failed: Invalid token", 1, -1);

    /*
     *  12. add-jwk refuses kid '*' as the config does, before the key is
     *  in the running set
     */
    hgobj authz = gobj_find_service("authz", TRUE);
    int warnings_before = s_warnings;
    json_t *r = gobj_command(authz, "add-jwk",
        json_pack("{s:s, s:s, s:s, s:s}",
            "kid", "*", "kty", "oct", "alg", "HS256", "n", "AQAB"
        ),
        gobj
    );
    check_int("12. add-jwk kid=*: result", (int)kw_get_int(gobj, r, "result", -99, 0), -1);
    check_int("12. add-jwk kid=*: the comment says why",
        strstr(kw_get_str(gobj, r, "comment", "", 0), KID_STAR_REFUSED)? 1 : 0, 1);
    check_int("12. add-jwk kid=*: logs", s_warnings - warnings_before, 0);
    JSON_DECREF(r)

    /*
     *  13. remove-jwk kid=*: its key was never loaded (case 11)
     */
    warnings_before = s_warnings;
    r = gobj_command(authz, "remove-jwk", json_pack("{s:s}", "kid", "*"), gobj);
    check_int("13. remove-jwk kid=*: result", (int)kw_get_int(gobj, r, "result", -99, 0), -1);
    check_int("13. remove-jwk kid=*: the comment says it was never loaded",
        strstr(kw_get_str(gobj, r, "comment", "", 0), "never loaded")? 1 : 0, 1);
    check_int("13. remove-jwk kid=*: logs", s_warnings - warnings_before, 0);
    JSON_DECREF(r)

    /*
     *  14. The two keys of a rotation: the second validates; remove-jwk of
     *  their kid takes both
     */
    claims = alice_claims();
    json_object_set_new(claims, "iss", json_string(ROTATE_ISS));
    const char *token_b = make_token(7, OTHER_JWK, claims);
    claims = alice_claims();
    json_object_set_new(claims, "iss", json_string(ROTATE_ISS));
    const char *token_a = make_token(8, LOGIN_JWK, claims);
    check_login(gobj, "14. alice, a token of the second key of a rotation", "127.0.0.1:40000",
        "", token_b,
        0, "", 0, 0);

    warnings_before = s_warnings;
    r = gobj_command(authz, "remove-jwk", json_pack("{s:s}", "kid", ROTATE_ISS), gobj);
    check_int("14. remove-jwk of the rotation: result", (int)kw_get_int(gobj, r, "result", -99, 0), 0);
    check_int("14. remove-jwk of the rotation: the comment says 2 keys",
        strstr(kw_get_str(gobj, r, "comment", "", 0), "(2 keys)")? 1 : 0, 1);
    check_int("14. remove-jwk of the rotation: its data has the 2 records",
        (int)json_array_size(kw_get_list(gobj, r, "data", 0, 0)), 2);
    check_int("14. remove-jwk of the rotation: logs", s_warnings - warnings_before, 0);
    JSON_DECREF(r)

    check_login(gobj, "14. the token of the second key, after remove-jwk", "127.0.0.1:40000",
        "", token_b,
        -1, "Authentication failed: Invalid token", 1, -1);
    check_login(gobj, "14. a token of the first key, after remove-jwk", "127.0.0.1:40000",
        "", token_a,
        -1, "Authentication failed: Invalid token", 1, -1);

    check_password_login_events(gobj);

    gobj_unsubscribe_event(authz_, EV_AUTHZ_USER_LOGIN, 0, gobj);
    gobj_unsubscribe_event(authz_, EV_AUTHZ_USER_LOGOUT, 0, gobj);
    JSON_DECREF(s_login_kw)
    JSON_DECREF(s_logout_kw)
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

/*
 *  The kw of the last login and logout of C_AUTHZ, kept for the checks
 */
PRIVATE int ac_authz_user_login(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(s_login_kw)
    s_login_kw = kw;
    return 0;
}

PRIVATE int ac_authz_user_logout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(s_logout_kw)
    s_logout_kw = kw;
    return 0;
}

/*
 *  A peer whose session C_AUTHZ drops (max_sessions): a real channel would
 *  close, the check closes it
 */
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    s_drops++;
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

PRIVATE int register_c_test_authz_login(void)
{
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,            ac_timer,               0},
        {EV_AUTHZ_USER_LOGIN,   ac_authz_user_login,    0},
        {EV_AUTHZ_USER_LOGOUT,  ac_authz_user_logout,   0},
        {EV_DROP,               ac_drop,                0},
        {0, 0, 0}
    };
    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };
    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_AUTHZ_USER_LOGIN,   0},
        {EV_AUTHZ_USER_LOGOUT,  0},
        {EV_DROP,               0},
        {EV_ON_CLOSE,           EVF_OUTPUT_EVENT},
        {0, 0}
    };
    hgclass gc = gclass_create(
        C_TEST_AUTHZ_LOGIN,
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
    return register_c_test_authz_login();
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
    check_int("no log line carries the claims of a token", s_claims_logged, 0);
    check_int("10. a line names the claims that failed", s_claims_named > 0? 1 : 0, 1);
    check_int("10. no line says 'What JWT_CLAIM_'", s_claims_unnamed, 0);
    size_t leaked = get_cur_system_memory();
    check_int("no memory leak", (int)leaked, 0);

    printf("\n%s: %s\n", APP, (s_result == 0 && result == 0) ? "PASS" : "FAIL");
    return s_result + result;
}
