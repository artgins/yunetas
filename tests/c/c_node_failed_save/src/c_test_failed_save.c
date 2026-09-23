/***********************************************************************
 *          C_TEST_FAILED_SAVE.C
 *
 *          GClass to test an update-node with autolink whose SAVE fails,
 *          at the c_node level
 *
 *          C_NODE writes an update-node with `autolink` as ONE write
 *          (treedb_update_node_and_links()): the fields, the links and
 *          the save. When the save fails, the answer is NULL (the method)
 *          or -1 (the command), memory is what the disk has, and no
 *          EV_TREEDB_NODE_LINKED / EV_TREEDB_NODE_UPDATED is published:
 *
 *            - update + autolink: nothing moved;
 *            - create + autolink: the create is on disk without its links,
 *              memory has the node without them too, and the log says so.
 *
 *          A reload from disk (a new tranger) says what memory said, and
 *          the same writes, retried with the disk writable, work.
 *
 *          A read-only file cannot make the save fail here: C_NODE keeps
 *          the files of a key open once written, and the create+autolink
 *          case needs the create to go through and the save after it to
 *          fail. So the test fails the writes of one key in its own
 *          __wrap_write() (see CMakeLists.txt).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "c_test_failed_save.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define DATABASE        "c_node_failed_save"
#define TREEDB_NAME     "treedb_failed_save"
#define REF_DIRECTION_U "departments^direction^users"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level of help"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-------json_fn---------description--*/
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,    cmd_help,       "Command's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name--------------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *      Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_node;
    hgobj timer;
    json_t *tranger;

    int linked_count;
    int unlinked_count;
    int created_count;
    int updated_count;
} PRIVATE_DATA;

/*
 *  __wrap_write(): with `fail_writes_key` set (a key directory, ending in
 *  '/'), the writes into the files of that key go through until
 *  `fail_writes_after_rows` rows of its md2 were written, and fail with
 *  EIO after. Every other write goes through.
 */
PRIVATE char path_database[PATH_MAX];
PRIVATE char fail_writes_key[PATH_MAX];
PRIVATE int fail_writes_after_rows = 0;

/***************************************************************************
 *  Schema for the test treedb: departments + users
 ***************************************************************************/
PRIVATE char schema_failed_save[] = "\
{                                                                   \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'users',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'username': {                                       \n\
                    'header': 'User Name',                          \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Department',                         \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'topic_name': 'departments',                            \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'users': {                                          \n\
                    'header': 'Users',                              \n\
                    'type': 'array',                                \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'users': 'departments'                      \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *  The writes of the files of one key fail on demand (see above)
 ***************************************************************************/
ssize_t __real_write(int fd, const void *buf, size_t count);
ssize_t __wrap_write(int fd, const void *buf, size_t count);

ssize_t __wrap_write(int fd, const void *buf, size_t count)
{
    if(fail_writes_key[0]) {
        char link[PATH_MAX];
        char target[PATH_MAX];
        snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
        ssize_t ln = readlink(link, target, sizeof(target) - 1);
        if(ln > 0) {
            target[ln] = 0;
            if(strncmp(target, fail_writes_key, strlen(fail_writes_key)) == 0) {
                if(fail_writes_after_rows <= 0) {
                    errno = EIO;
                    return -1;
                }
                if(ln > 4 && strcmp(target + ln - 4, ".md2") == 0) {
                    fail_writes_after_rows--;
                }
            }
        }
    }
    return __real_write(fd, buf, count);
}

/***************************************************************************
 *  Arm the failing writes of the key `key` of `topic_name`: they fail
 *  after `after_rows` rows of its md2 (NULL disarms)
 ***************************************************************************/
PRIVATE void fail_writes_of_key(const char *topic_name, const char *key, int after_rows)
{
    if(!topic_name) {
        fail_writes_key[0] = 0;
        fail_writes_after_rows = 0;
        return;
    }
    build_path(fail_writes_key, sizeof(fail_writes_key) - 1,
        path_database, topic_name, "keys", key, NULL);
    strcat(fail_writes_key, "/");
    fail_writes_after_rows = after_rows;
}

/***************************************************************************
 *  Start a tranger over the test database: a load from DISK
 ***************************************************************************/
PRIVATE json_t *start_tranger(const char *path_root)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    return tranger2_startup(0, jn_tranger, 0);
}

/***************************************************************************
 *              Framework Methods
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Prepare paths
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    priv->tranger = start_tranger(path_root);

    /*
     *  C_NODE child with with_link_events=true
     */
    helper_quote2doublequote(schema_failed_save);
    json_t *jn_schema = legalstring2json(schema_failed_save, TRUE);

    json_t *kw_resource = json_pack("{s:I, s:s, s:o, s:i, s:b}",
        "tranger", (json_int_t)(uintptr_t)priv->tranger,
        "treedb_name", TREEDB_NAME,
        "treedb_schema", jn_schema,
        "exit_on_error", LOG_OPT_TRACE_STACK,
        "with_link_events", 1
    );

    priv->gobj_node = gobj_create_pure_child(
        "test_node",
        C_NODE,
        kw_resource,
        gobj
    );

    /*
     *  A timer to run the tests from within the event loop
     */
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_node);
    gobj_start(priv->timer);

    return 0;
}

PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->gobj_node);

    return 0;
}

PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 100);

    return 0;
}

PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->tranger) {
        // NOTE: treedb is already closed by C_NODE's mt_stop
        tranger2_shutdown(priv->tranger);
        priv->tranger = NULL;
    }
}

/***************************************************************************
 *  Reset the event counters
 ***************************************************************************/
PRIVATE void reset_counters(PRIVATE_DATA *priv)
{
    priv->linked_count = 0;
    priv->unlinked_count = 0;
    priv->created_count = 0;
    priv->updated_count = 0;
}

/***************************************************************************
 *  A TEST FAIL, logged: it is not in the expected list, so test_json fails
 ***************************************************************************/
PRIVATE int fail(hgobj gobj, const char *test, const char *what, json_t *seen) // seen not owned
{
    gobj_log_error(gobj, 0,
        "function", "%s", __FUNCTION__,
        "msgset", "%s", MSGSET_INTERNAL,
        "msg", "%s", "TEST FAIL",
        "test", "%s", test,
        "what", "%s", what,
        "seen", "%j", seen? seen : json_null(),
        NULL
    );
    return -1;
}

/***************************************************************************
 *  Does the hook `users` of `parent` hold `child`?
 ***************************************************************************/
PRIVATE BOOL users_hook_holds(json_t *parent, json_t *child)
{
    json_t *hook = json_object_get(parent, "users");
    int idx; json_t *v;
    json_array_foreach(hook, idx, v) {
        if(v == child) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  The user `id` as the disk has it after a failed save: username
 *  `username`, no department, and direction does not hook it
 ***************************************************************************/
PRIVATE int check_user_as_on_disk(
    hgobj gobj,
    const char *test,
    const char *id,
    const char *username
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    json_t *user = treedb_get_node(priv->tranger, TREEDB_NAME, "users", id);
    json_t *direction = treedb_get_node(priv->tranger, TREEDB_NAME, "departments", "direction");
    if(!user || !direction) {
        return fail(gobj, test, "a node is gone", user);
    }
    if(strcmp(kw_get_str(gobj, user, "username", "", 0), username) != 0) {
        result += fail(gobj, test, "memory kept the field", user);
    }
    if(json_array_size(json_object_get(user, "departments")) != 0) {
        result += fail(gobj, test, "memory kept the fkey", user);
    }
    if(users_hook_holds(direction, user)) {
        result += fail(gobj, test, "memory kept the hook", direction);
    }
    return result;
}

/***************************************************************************
 *  No LINKED and no UPDATED was published
 ***************************************************************************/
PRIVATE int check_nothing_told(hgobj gobj, const char *test)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    if(priv->linked_count != 0) {
        result += fail(gobj, test, "EV_TREEDB_NODE_LINKED was published", NULL);
    }
    if(priv->updated_count != 0) {
        result += fail(gobj, test, "EV_TREEDB_NODE_UPDATED was published", NULL);
    }
    return result;
}

/***************************************************************************
 *  Run all tests -- called from the timer inside the event loop
 ***************************************************************************/
PRIVATE int run_tests(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    const char *test;
    json_t *node;
    json_t *jn_resp;

    /*
     *  direction, and alice in no department
     */
    if(!treedb_create_node(priv->tranger, TREEDB_NAME, "departments",
            json_pack("{s:s, s:s}", "id", "direction", "name", "Direction")) ||
        !treedb_create_node(priv->tranger, TREEDB_NAME, "users",
            json_pack("{s:s, s:s}", "id", "alice", "username", "alice"))) {
        return fail(gobj, "setup", "the nodes were not created", NULL);
    }

    /*-----------------------------------------------*
     *  update + autolink, the method: NULL, and
     *  nothing moved
     *-----------------------------------------------*/
    test = "update + autolink, its save fails (method)";
    reset_counters(priv);
    fail_writes_of_key("users", "alice", 0);
    node = gobj_update_node(
        priv->gobj_node,
        "users",
        json_pack("{s:s, s:s, s:[s]}",
            "id", "alice",
            "username", "ALICE-NEW",
            "departments", REF_DIRECTION_U
        ),
        json_pack("{s:b}", "autolink", 1),
        gobj
    );
    fail_writes_of_key(NULL, NULL, 0);
    if(node) {
        result += fail(gobj, test, "the update answered the node", node);
        JSON_DECREF(node)
    }
    result += check_user_as_on_disk(gobj, test, "alice", "alice");
    result += check_nothing_told(gobj, test);

    /*-----------------------------------------------*
     *  update + autolink, the command: -1
     *-----------------------------------------------*/
    test = "update + autolink, its save fails (command)";
    reset_counters(priv);
    fail_writes_of_key("users", "alice", 0);
    jn_resp = gobj_command(priv->gobj_node, "update-node",
        json_pack("{s:s, s:{s:s, s:s, s:[s]}, s:{s:b}}",
            "topic_name", "users",
            "record",
                "id", "alice",
                "username", "ALICE-NEW",
                "departments", REF_DIRECTION_U,
            "options",
                "autolink", 1
        ),
        gobj
    );
    fail_writes_of_key(NULL, NULL, 0);
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) != -1) {
        result += fail(gobj, test, "the command did not answer -1", jn_resp);
    }
    JSON_DECREF(jn_resp)
    result += check_user_as_on_disk(gobj, test, "alice", "alice");
    result += check_nothing_told(gobj, test);

    /*-----------------------------------------------*
     *  create + autolink: the create goes through
     *  (one row), the save of its links fails
     *-----------------------------------------------*/
    test = "create + autolink, the save of its links fails";
    reset_counters(priv);
    fail_writes_of_key("users", "zoe", 1);
    node = gobj_update_node(
        priv->gobj_node,
        "users",
        json_pack("{s:s, s:s, s:[s]}",
            "id", "zoe",
            "username", "zoe",
            "departments", REF_DIRECTION_U
        ),
        json_pack("{s:b, s:b}", "create", 1, "autolink", 1),
        gobj
    );
    fail_writes_of_key(NULL, NULL, 0);
    if(node) {
        result += fail(gobj, test, "the create answered the node", node);
        JSON_DECREF(node)
    }
    result += check_user_as_on_disk(gobj, test, "zoe", "zoe");
    result += check_nothing_told(gobj, test);
    if(priv->created_count != 1) {
        result += fail(gobj, test, "the create was not published once", NULL);
    }

    /*-----------------------------------------------*
     *  A reload from disk, with a new tranger: the
     *  disk says what memory said
     *-----------------------------------------------*/
    test = "the disk says what memory said";
    gobj_stop(priv->gobj_node);
    tranger2_shutdown(priv->tranger);
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    priv->tranger = start_tranger(path_root);
    gobj_write_pointer_attr(priv->gobj_node, "tranger", priv->tranger);
    gobj_start(priv->gobj_node);
    result += check_user_as_on_disk(gobj, test, "alice", "alice");
    result += check_user_as_on_disk(gobj, test, "zoe", "zoe");

    /*-----------------------------------------------*
     *  The same writes with the disk writable: they
     *  work, and are published
     *-----------------------------------------------*/
    test = "the same update + autolink, retried";
    reset_counters(priv);
    node = gobj_update_node(
        priv->gobj_node,
        "users",
        json_pack("{s:s, s:s, s:[s]}",
            "id", "alice",
            "username", "ALICE-NEW",
            "departments", REF_DIRECTION_U
        ),
        json_pack("{s:b}", "autolink", 1),
        gobj
    );
    if(!node) {
        result += fail(gobj, test, "the retry failed", NULL);
    }
    JSON_DECREF(node)
    json_t *alice = treedb_get_node(priv->tranger, TREEDB_NAME, "users", "alice");
    json_t *direction = treedb_get_node(priv->tranger, TREEDB_NAME, "departments", "direction");
    if(strcmp(kw_get_str(gobj, alice, "username", "", 0), "ALICE-NEW") != 0 ||
            !users_hook_holds(direction, alice)) {
        result += fail(gobj, test, "the retry is not in memory", alice);
    }
    if(priv->linked_count != 1) {
        result += fail(gobj, test, "the link was not published once", NULL);
    }
    if(priv->updated_count == 0) {
        result += fail(gobj, test, "the update was not published", NULL);
    }

    if(result == 0) {
        gobj_log_info(gobj, 0,
            "msgset", "%s", MSGSET_INFO,
            "msg", "%s", "All c_node failed save tests PASSED",
            NULL
        );
    }

    return result;
}

/***************************************************************************
 *              Action callbacks
 ***************************************************************************/

/***************************************************************************
 *  EV_TREEDB_NODE_LINKED
 ***************************************************************************/
PRIVATE int ac_node_linked(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->linked_count++;
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TREEDB_NODE_UNLINKED
 ***************************************************************************/
PRIVATE int ac_node_unlinked(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->unlinked_count++;
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TREEDB_NODE_CREATED
 ***************************************************************************/
PRIVATE int ac_node_created(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->created_count++;
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TREEDB_NODE_UPDATED
 ***************************************************************************/
PRIVATE int ac_node_updated(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->updated_count++;
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TIMEOUT -- runs the tests inside the event loop, then exits
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    run_tests(gobj);

    gobj_log_info(gobj, 0,
        "msgset", "%s", MSGSET_INFO,
        "msg", "%s", "Exit to die",
        NULL
    );
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_STOPPED
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *              Command: help
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw)
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *              GClass
 ***************************************************************************/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_destroy = mt_destroy,
};

GOBJ_DEFINE_GCLASS(C_TEST_FAILED_SAVE);

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

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,         0},
        {EV_TREEDB_NODE_LINKED,     ac_node_linked,     0},
        {EV_TREEDB_NODE_UNLINKED,   ac_node_unlinked,   0},
        {EV_TREEDB_NODE_CREATED,    ac_node_created,    0},
        {EV_TREEDB_NODE_UPDATED,    ac_node_updated,    0},
        {EV_STOPPED,                ac_stopped,         0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_TREEDB_NODE_LINKED,     EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_UNLINKED,   EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_CREATED,    EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_UPDATED,    EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
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
        0,  //lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    return __gclass__ ? 0 : -1;
}

/***************************************************************************
 *              Registration
 ***************************************************************************/
PUBLIC int register_c_test_failed_save(void)
{
    return create_gclass(C_TEST_FAILED_SAVE);
}
