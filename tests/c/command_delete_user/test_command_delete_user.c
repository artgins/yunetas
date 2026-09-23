/****************************************************************************
 *          test_command_delete_user.c
 *
 *          Regression test for cmd_delete_user (C_AUTHZ).
 *
 *          Invariant under test: the ONLY "do not delete" boundary for an
 *          authz user is immutability, NOT whether it holds roles.
 *            1. plain local user (no roles)      -> delete-user deletes it
 *            2. role-holding user, no force      -> refused (result -1), kept
 *            3. role-holding user, force=1       -> deleted (roles unlinked)
 *            4. immutable seed user, even force  -> refused (result -1), kept
 *            5. disable-user                     -> disables it, logs no error
 *            7. enable-user on a replica         -> -1, not "User enabled"
 *                                                   (A7 of the 2026-09-21 review)
 *            6. a role that cannot be linked     -> create/update-user refused,
 *                                                   the user keeps its role (M15)
 *            8. EV_REJECT_USER on a replica      -> the live sessions are dropped
 *                                                   although `disabled` cannot be
 *                                                   written, -1 (M5 of 2026-09-23)
 *            9. EV_ADD_USER / EV_IDP_USER_CREATED on a replica -> -1, nothing
 *                                                   created, nothing moved (M4)
 *           10. disable-user / enable-user / set-max-sessions -> the local
 *                                                   password survives them
 *           11. every comment starts with the yuno, walked from the
 *                                                   command table, and the causes are
 *                                                   the command's own, never the global
 *                                                   last message (review of the second
 *                                                   fix round, 2026-09-23)
 *
 *          A real C_AUTHZ service is instantiated over a temp tranger store;
 *          a role and an immutable user are seeded via initial_load, and the
 *          mutable users are created with create-user (which can attach roles
 *          to a local user too -- that is why "has roles" is not a boundary).
 *
 *          The whole test runs under yuneta_entry_point (the canonical yuno
 *          lifecycle): the driver does its work from a timer action, INSIDE
 *          the running event loop, then set_yuno_must_die(). That matters for
 *          leak checking -- the tranger master registers io_uring fs_watchers
 *          per topic whose teardown is asynchronous, so the loop must be
 *          running to reap the cancellations. A synchronous test (no loop)
 *          would leak the fs_watchers and the yev_loop.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP             "test_command_delete_user"
#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DOC         "delete-user regression test"
#define APP_DATETIME    ""
#define STORE           "/tmp/test_command_delete_user_store"

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int s_result = 0;   /* accumulated check result, read after entry_point */
PRIVATE int s_errors = 0;   /* logs of priority ERROR and up, see count_errors() */
PRIVATE int s_drops = 0;    /* EV_DROP received: a session of a rejected user dropped */

GOBJ_DEFINE_GCLASS(C_TEST_DELUSER);

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
        'yuno_role': 'test_command_delete_user',                    \n\
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
            'name': 'deluser-driver',                               \n\
            'gclass': 'C_TEST_DELUSER',                             \n\
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
                'initial_load': {                                   \n\
                    'roles': [                                      \n\
                        {                                           \n\
                            'id': 'testrole',                       \n\
                            'disabled': false,                      \n\
                            'description': 'test role',             \n\
                            'realm_id': '*',                        \n\
                            'parent_role_id': '',                   \n\
                            'service': '*',                         \n\
                            'permission': '*'                       \n\
                        }                                           \n\
                    ],                                              \n\
                    'users': [                                      \n\
                        {                                           \n\
                            'id': 'seed_immutable',                 \n\
                            'roles': ['roles^testrole^users']       \n\
                        }                                           \n\
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
        printf("FAIL %-44s got %d expected %d\n", name, got, expected);
        s_result += -1;
    } else {
        printf("ok   %-44s (%d)\n", name, got);
    }
}

/*
 *  Drive a C_AUTHZ command internally: src == the authz service, and no
 *  __username__ in the kw, so the SDF_AUTHZ_X command gate never fires and we
 *  test cmd_delete_user's own logic in isolation.
 */
PRIVATE int cmd_result(hgobj authz, const char *command, json_t *kw)
{
    json_t *r = gobj_command(authz, command, kw, authz);
    int result = (int)kw_get_int(0, r, "result", -999, 0);
    JSON_DECREF(r)
    return result;
}

/*
 *  The comment of a C_AUTHZ command must start with the yuno that answers,
 *  and must not carry the stale last message planted before it.
 */
PRIVATE void check_comment(hgobj authz, const char *command, json_t *kw, int expected, const char *says)
{
    gobj_log_set_last_message("a stale message of somebody else");
    json_t *r = gobj_command(authz, command, kw, authz);
    int result = (int)kw_get_int(0, r, "result", -999, 0);
    const char *comment = kw_get_str(0, r, "comment", "", 0);
    const char *prefix = gobj_yuno_role_plus_name();
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "%s says its yuno", command);
    int ok = (result == expected &&
        strncmp(comment, prefix, strlen(prefix))==0 &&
        (empty_string(says) || strstr(comment, says)) &&
        !strstr(comment, "stale"))? 1 : 0;
    if(!ok) {
        printf("     %s -> %d '%s'\n", command, result, comment);
    }
    check_int(name, ok, 1);
    JSON_DECREF(r)
}

PRIVATE BOOL user_disabled(const char *username)
{
    hgobj treedb = gobj_find_service("treedb_authzs", FALSE);
    if(!treedb) {
        return FALSE;
    }
    json_t *node = gobj_get_node(
        treedb,
        "users",
        json_pack("{s:s}", "id", username),
        0,
        treedb
    );
    BOOL disabled = kw_get_bool(0, node, "disabled", 0, 0);
    JSON_DECREF(node)
    return disabled;
}

/*
 *  A log handler that only counts: a check reads the difference around one
 *  command, so what the rest of the test logs does not count.
 */
PRIVATE int count_errors(void *h, int priority, const char *bf, size_t len)
{
    s_errors++;
    return 0;
}

PRIVATE BOOL user_exists(const char *username)
{
    hgobj treedb = gobj_find_service("treedb_authzs", FALSE);
    if(!treedb) {
        return FALSE;
    }
    json_t *node = gobj_get_node(
        treedb,
        "users",
        json_pack("{s:s}", "id", username),
        0,
        treedb
    );
    BOOL exists = (node != NULL);
    JSON_DECREF(node)
    return exists;
}

PRIVATE BOOL user_has_role(const char *username, const char *role)
{
    hgobj treedb = gobj_find_service("treedb_authzs", FALSE);
    if(!treedb) {
        return FALSE;
    }
    json_t *node = gobj_get_node(
        treedb,
        "users",
        json_pack("{s:s}", "id", username),
        json_pack("{s:b}", "only_id", 1),
        treedb
    );
    json_t *roles = kw_get_list(0, node, "roles", 0, 0);
    BOOL has = FALSE;
    size_t idx; json_t *jn_role;
    json_array_foreach(roles, idx, jn_role) {
        if(strcmp(json_string_value(jn_role)?json_string_value(jn_role):"", role)==0) {
            has = TRUE;
        }
    }
    JSON_DECREF(node)
    return has;
}

/*
 *  The number of credentials of a user, read WITH the hidden columns: a
 *  plain read masks `credentials` as null.
 */
PRIVATE int user_credentials(const char *username)
{
    hgobj treedb = gobj_find_service("treedb_authzs", FALSE);
    if(!treedb) {
        return -1;
    }
    json_t *node = gobj_get_node(
        treedb,
        "users",
        json_pack("{s:s}", "id", username),
        json_pack("{s:b}", "show_hidden", 1),
        treedb
    );
    int n = (int)json_array_size(kw_get_list(0, node, "credentials", 0, 0));
    JSON_DECREF(node)
    return n;
}

PRIVATE int user_max_sessions(const char *username)
{
    hgobj treedb = gobj_find_service("treedb_authzs", FALSE);
    if(!treedb) {
        return -1;
    }
    json_t *node = gobj_get_node(
        treedb,
        "users",
        json_pack("{s:s}", "id", username),
        0,
        treedb
    );
    int n = (int)kw_get_int(0, node, "max_sessions", -1, 0);
    JSON_DECREF(node)
    return n;
}

/*
 *  check-user-pwd answers result 0 either way; the verdict is its comment.
 */
PRIVATE int password_matches(hgobj authz, const char *username, const char *password)
{
    json_t *r = gobj_command(
        authz,
        "check-user-pwd",
        json_pack("{s:s, s:s}", "username", username, "password", password),
        authz
    );
    const char *comment = kw_get_str(0, r, "comment", "", 0);
    int yes = (strstr(comment, "Yes") != NULL)? 1 : 0;
    JSON_DECREF(r)
    return yes;
}

/***************************************************************************
 *              The actual checks (run inside the loop, from the timer)
 ***************************************************************************/
PRIVATE void run_checks(hgobj gobj)
{
    /*
     *  C_AUTHZ is a config-defined service (already started+seeded by the yuno,
     *  so the yuno's own shutdown tears it down cleanly inside the loop). We
     *  only drive it here.
     */
    hgobj authz = gobj_find_service("authz", FALSE);
    if(!authz) {
        printf("FAIL %-44s\n", "C_AUTHZ service not found");
        s_result += -1;
        return;
    }

    /*  Case 1: plain local user (no roles) deletes without force  */
    check_int("create local_noroles",
        cmd_result(authz, "create-user", json_pack("{s:s}", "username", "local_noroles")),
        0);
    check_int("local_noroles exists after create", user_exists("local_noroles"), 1);
    check_int("delete local_noroles (no force)",
        cmd_result(authz, "delete-user", json_pack("{s:s}", "username", "local_noroles")),
        0);
    check_int("local_noroles gone", user_exists("local_noroles"), 0);

    /*  Case 2 & 3: role-holding user needs force  */
    check_int("create local_withroles",
        cmd_result(authz, "create-user",
            json_pack("{s:s, s:s}", "username", "local_withroles", "role", "roles^testrole^users")),
        0);
    check_int("local_withroles exists after create", user_exists("local_withroles"), 1);

    check_int("delete local_withroles (no force) refused",
        cmd_result(authz, "delete-user", json_pack("{s:s}", "username", "local_withroles")),
        -1);
    check_int("local_withroles kept after refusal", user_exists("local_withroles"), 1);

    check_int("delete local_withroles (force=1) ok",
        cmd_result(authz, "delete-user",
            json_pack("{s:s, s:b}", "username", "local_withroles", "force", 1)),
        0);
    check_int("local_withroles gone after force", user_exists("local_withroles"), 0);

    /*  Case 4: immutable seed user is never deletable, even with force  */
    check_int("delete seed_immutable (no force) refused",
        cmd_result(authz, "delete-user", json_pack("{s:s}", "username", "seed_immutable")),
        -1);
    check_int("delete seed_immutable (force=1) refused",
        cmd_result(authz, "delete-user",
            json_pack("{s:s, s:b}", "username", "seed_immutable", "force", 1)),
        -1);
    check_int("seed_immutable kept", user_exists("seed_immutable"), 1);

    /*
     *  Case 5: disable-user (A7 of the 2026-09-21 review). It handed the
     *  NODE to EV_REJECT_USER, which reads "username" (the node keys on
     *  "id"): the lookup failed with a logged error, the user's live
     *  sessions were never dropped, and the event freed the node that the
     *  response then used. The bug delete-user had, fixed there, not here.
     */
    check_int("create local_to_disable",
        cmd_result(authz, "create-user", json_pack("{s:s}", "username", "local_to_disable")),
        0);
    int errors_before = s_errors;
    check_int("disable local_to_disable",
        cmd_result(authz, "disable-user", json_pack("{s:s}", "username", "local_to_disable")),
        0);
    check_int("disable-user logs no error", s_errors - errors_before, 0);
    check_int("local_to_disable is disabled", user_disabled("local_to_disable"), 1);

    /*
     *  Case 6: a role that cannot be linked (rest of M15 of the 2026-09-21
     *  review). The link was refused inside the treedb and the command
     *  answered "User updated"/"User created" all the same. It is refused
     *  before anything is written now, and the user keeps the role it had.
     */
    check_int("create local_badrole with testrole",
        cmd_result(authz, "create-user",
            json_pack("{s:s, s:s}", "username", "local_badrole", "role", "roles^testrole^users")),
        0);
    check_int("update-user to a role that does not exist is refused",
        cmd_result(authz, "update-user",
            json_pack("{s:s, s:s}", "username", "local_badrole", "role", "roles^nosuchrole^users")),
        -1);
    check_int("local_badrole keeps testrole", user_has_role("local_badrole", "testrole"), 1);
    check_int("update-user with a malformed role ref is refused",
        cmd_result(authz, "update-user",
            json_pack("{s:s, s:s}", "username", "local_badrole", "role", "testrole")),
        -1);
    check_int("create-user with a role that does not exist is refused",
        cmd_result(authz, "create-user",
            json_pack("{s:s, s:s}", "username", "local_norole", "role", "roles^nosuchrole^users")),
        -1);
    check_int("local_norole was not created", user_exists("local_norole"), 0);

    /*
     *  Case 7: enable-user (N7 of the 2026-09-22 review, the sibling of
     *  case 5). It handed the return of gobj_update_node() to the response
     *  as it was: a refused update answered result 0, "User enabled", with
     *  no record. A replica refuses every write, which makes the refusal
     *  reproducible here: the tranger's master flag is turned off around
     *  the command.
     */
    {
        hgobj treedb = gobj_find_service("treedb_authzs", FALSE);
        json_t *tranger = treedb? gobj_read_pointer_attr(treedb, "tranger") : NULL;
        if(!tranger) {
            printf("FAIL %-44s\n", "treedb_authzs tranger not found");
            s_result += -1;
        } else {
            json_object_set_new(tranger, "master", json_false());
            check_int("enable-user on a replica is refused",
                cmd_result(authz, "enable-user", json_pack("{s:s}", "username", "local_to_disable")),
                -1);
            json_object_set_new(tranger, "master", json_true());
            check_int("enable local_to_disable",
                cmd_result(authz, "enable-user", json_pack("{s:s}", "username", "local_to_disable")),
                0);
            check_int("local_to_disable is enabled", user_disabled("local_to_disable"), 0);
        }
    }

    /*
     *  Case 8: EV_REJECT_USER when `disabled` cannot be written (M5 of the
     *  2026-09-23 review). The write's NULL replaced the user node, so its
     *  __sessions were never read and a rejected user stayed connected. The
     *  sessions are dropped from the node read before the write now, and
     *  the event answers -1 for the write it could not do. The session is
     *  this driver: EV_DROP reaches it.
     */
    {
        hgobj treedb = gobj_find_service("treedb_authzs", FALSE);
        json_t *tranger = treedb? gobj_read_pointer_attr(treedb, "tranger") : NULL;
        check_int("create local_session",
            cmd_result(authz, "create-user", json_pack("{s:s}", "username", "local_session")),
            0);
        json_t *with_session = gobj_update_node(
            treedb,
            "users",
            json_pack("{s:s, s:{s:{s:I}}}",
                "id", "local_session",
                "__sessions",
                    "session-1",
                        "channel_gobj", (json_int_t)(uintptr_t)gobj
            ),
            json_pack("{s:b}", "volatil", 1),
            gobj
        );
        check_int("a live session injected", with_session? 1 : 0, 1);
        JSON_DECREF(with_session)

        if(tranger) {
            json_object_set_new(tranger, "master", json_false());
        }
        s_drops = 0;
        int ret = gobj_send_event(
            authz,
            EV_REJECT_USER,
            json_pack("{s:s, s:b}", "username", "local_session", "disabled", 1),
            gobj
        );
        if(tranger) {
            json_object_set_new(tranger, "master", json_true());
        }
        check_int("reject on a replica answers -1", ret, -1);
        check_int("reject on a replica drops the session", s_drops, 1);
        check_int("reject on a replica wrote nothing", user_disabled("local_session"), 0);

        json_t *node = gobj_get_node(
            treedb, "users", json_pack("{s:s}", "id", "local_session"), 0, gobj
        );
        check_int("the dropped session is gone from the node",
            (int)json_object_size(kw_get_dict(0, node, "__sessions", 0, 0)), 0);
        JSON_DECREF(node)
    }

    /*
     *  Case 9: the EVENT doors on a replica. refuse_on_replica() guards the
     *  commands only; EV_ADD_USER (with a role: the autolink path of
     *  C_NODE, M4) and EV_IDP_USER_CREATED went on to the treedb.
     */
    {
        hgobj treedb = gobj_find_service("treedb_authzs", FALSE);
        json_t *tranger = treedb? gobj_read_pointer_attr(treedb, "tranger") : NULL;
        if(tranger) {
            json_object_set_new(tranger, "master", json_false());
        }
        int ret_add = gobj_send_event(
            authz,
            EV_ADD_USER,
            json_pack("{s:s, s:s}", "username", "replica_user", "role", "roles^testrole^users"),
            gobj
        );
        int ret_role = gobj_send_event(
            authz,
            EV_ADD_USER,
            json_pack("{s:s, s:s}", "username", "local_badrole", "role", "roles^testrole^users"),
            gobj
        );
        int ret_idp = gobj_send_event(
            authz,
            EV_IDP_USER_CREATED,
            json_pack("{s:s}", "username", "idp_replica_user"),
            gobj
        );
        json_t *node = gobj_update_node(
            treedb,
            "users",
            json_pack("{s:s, s:[s]}", "id", "local_session", "roles", "roles^testrole^users"),
            json_pack("{s:b}", "autolink", 1),
            gobj
        );
        if(tranger) {
            json_object_set_new(tranger, "master", json_true());
        }
        check_int("EV_ADD_USER on a replica answers -1", ret_add, -1);
        check_int("EV_ADD_USER of an existing user on a replica answers -1", ret_role, -1);
        check_int("EV_IDP_USER_CREATED on a replica answers -1", ret_idp, -1);
        check_int("autolink update on a replica answers NULL", node? 1 : 0, 0);
        JSON_DECREF(node)
        check_int("replica_user was not created", user_exists("replica_user"), 0);
        check_int("idp_replica_user was not created", user_exists("idp_replica_user"), 0);
        check_int("local_session got no role on a replica", user_has_role("local_session", "testrole"), 0);
    }

    /*
     *  Case 10: disable-user, enable-user and set-max-sessions keep the
     *  local password (HIGH of the 2026-09-23 independent review). They
     *  read the user WITHOUT show_hidden, so `credentials` came back as the
     *  view's null mask, and they wrote the whole view back: the password
     *  was erased by a command about something else.
     */
    {
        const char *commands[] = {"disable-user", "enable-user", "set-max-sessions", 0};
        for(int i=0; commands[i]; i++) {
            char username[NAME_MAX];
            snprintf(username, sizeof(username), "local_passw_%d", i);
            check_int("create a user with a password",
                cmd_result(authz, "create-user",
                    json_pack("{s:s, s:s}", "username", username, "password", "S3cret-pass")),
                0);
            check_int("it has one credential", user_credentials(username), 1);

            json_t *kw_cmd = json_pack("{s:s}", "username", username);
            if(strcmp(commands[i], "set-max-sessions")==0) {
                json_object_set_new(kw_cmd, "max_sessions", json_integer(3));
            }
            printf("     %s %s\n", commands[i], username);
            check_int("the command succeeds", cmd_result(authz, commands[i], kw_cmd), 0);
            check_int("the credential survives it", user_credentials(username), 1);
            check_int("the password still matches",
                password_matches(authz, username, "S3cret-pass"), 1);
        }
        check_int("disable-user wrote disabled", user_disabled("local_passw_0"), 1);
        check_int("set-max-sessions wrote max_sessions", user_max_sessions("local_passw_2"), 3);
    }

    /*
     *  Case 11: every comment starts with the yuno (review of the second fix
     *  round, 2026-09-23: "User enabled: x", "Set max_sessions ...", "User
     *  not found" and most of the others did not). Named, so the check of
     *  the prefix is not a check of "".
     */
    {
        gobj_write_str_attr(gobj_yuno(), "yuno_role_plus_name", APP "^messages");

        /*  The table, each command with nothing to act on. Left out: what
         *  answers no comment (help, authzs), and set-max-sessions, which
         *  with no username sets the service's own value.  */
        const char *skip[] = {"help", "authzs", "set-max-sessions", NULL};
        const sdata_desc_t *cmds = gclass_command_desc(gclass_find_by_name(C_AUTHZ), NULL, TRUE);
        for(const sdata_desc_t *it = cmds; it && it->name; it++) {
            BOOL skipped = FALSE;
            for(int i = 0; skip[i]; i++) {
                if(strcmp(it->name, skip[i])==0) {
                    skipped = TRUE;
                }
            }
            if(skipped) {
                continue;
            }
            gobj_log_set_last_message("a stale message of somebody else");
            json_t *r = gobj_command(authz, it->name, json_object(), authz);
            const char *comment = kw_get_str(0, r, "comment", "", 0);
            const char *prefix = gobj_yuno_role_plus_name();
            int ok = ((empty_string(comment) || strncmp(comment, prefix, strlen(prefix))==0) &&
                !strstr(comment, "stale"))? 1 : 0;
            if(!ok) {
                printf("     %s -> '%s'\n", it->name, comment);
            }
            char name[NAME_MAX];
            snprintf(name, sizeof(name), "%s (empty kw) says its yuno", it->name);
            check_int(name, ok, 1);
            JSON_DECREF(r)
        }

        check_comment(authz, "create-user",
            json_pack("{s:s}", "username", "local_comment"), 0, "User created");
        check_comment(authz, "create-user",
            json_pack("{s:s}", "username", "local_comment"), -1, "already exists");
        check_comment(authz, "disable-user",
            json_pack("{s:s}", "username", "local_comment"), 0, "User disabled");
        check_comment(authz, "enable-user",
            json_pack("{s:s}", "username", "local_comment"), 0, "User enabled");
        check_comment(authz, "enable-user",
            json_pack("{s:s}", "username", "no_such_user"), -1, "User not found");
        check_comment(authz, "set-max-sessions",
            json_pack("{s:s, s:i}", "username", "local_comment", "max_sessions", 2), 0, "max_sessions");
        check_comment(authz, "set-max-sessions",
            json_pack("{s:s, s:i}", "username", "no_such_user", "max_sessions", 2), -1, "User not found");
        check_comment(authz, "set-user-pwd",
            json_pack("{s:s, s:s}", "username", "local_comment", "password", "An0ther-pass"), 0,
            "password");
        check_comment(authz, "check-user-pwd",
            json_pack("{s:s, s:s}", "username", "local_comment", "password", "An0ther-pass"), 0,
            "Password match");
        check_comment(authz, "delete-user",
            json_pack("{s:s}", "username", "local_comment"), 0, "User deleted");
    }
}

/***************************************************************
 *              Framework Methods
 ***************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
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
 *  EV_DROP: C_AUTHZ drops a session of a rejected user by sending it to the
 *  session's channel, and case 8 names this driver as that channel.
 */
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    s_drops++;
    JSON_DECREF(kw)
    return 0;
}

/*
 *  First fire (dying==FALSE): run the checks, then arm the death timer.
 *  Second fire (dying==TRUE): set_yuno_must_die() -- from inside the running
 *  loop, so the yuno teardown (and the tranger's async fs_watcher
 *  cancellations) are reaped before the loop exits.
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
PRIVATE sdata_desc_t attrs_table[] = {
    SDATA_END()
};

PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start  = mt_start,
    .mt_stop   = mt_stop,
    .mt_play   = mt_play,
    .mt_pause  = mt_pause,
};

PRIVATE int register_c_test_deluser(void)
{
    ev_action_t st_idle[] = {
        {EV_TIMEOUT, ac_timer, 0},
        {EV_DROP, ac_drop, 0},
        {0, 0, 0}
    };
    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };
    event_type_t event_types[] = {
        {EV_TIMEOUT, 0},
        {EV_DROP, 0},
        {0, 0}
    };
    hgclass gc = gclass_create(
        C_TEST_DELUSER,
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
    /*  yuneta_entry_point already calls yunetas_register_c_core(); only our
     *  own driver gclass needs registering here.  */
    return register_c_test_deluser();
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    glog_init();
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("count_errors", 0, count_errors, 0);
    gobj_log_add_handler("count_errors", "count_errors", LOG_OPT_UP_ERROR, 0);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);

    /*  The store must exist before C_AUTHZ autostarts (it checks is_directory).  */
    rmrdir(STORE);
    mkrdir(STORE, 02770);

    yuneta_setup(
        NULL,                   // persistent_attrs
        command_parser,         // command_parser (needed for gobj_command)
        NULL,                   // stats_parser
        NULL,                   // authz_checker
        NULL,                   // authentication_parser
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

    size_t leaked = get_cur_system_memory();
    check_int("no memory leak", (int)leaked, 0);

    printf("\n%s: %s\n", APP, (s_result == 0 && result == 0) ? "PASS" : "FAIL");
    return s_result + result;
}
