/****************************************************************************
 *          test_tr_treedb_failed_save.c
 *
 *          A write of treedb whose SAVE fails is taken back in memory.
 *
 *          treedb_update_node(), treedb_link_nodes(),
 *          treedb_unlink_nodes(), treedb_replace_links() and
 *          treedb_autolink() change the node in memory first (its
 *          fields, its fkeys and the hooks of its parents), then save it.
 *          When the save fails (here the files of the key are read-only),
 *          the node in memory must go back to what the disk has, and no
 *          event of the write is told: a node that kept the update
 *          answered a value nobody stored until the next load, and a retry
 *          of the same write found nothing to write.
 *
 *          The treedb is reloaded from disk first, so no file of the key
 *          is open for writing when its mode changes. When the files are
 *          writable again, the same writes work, and a reload from disk
 *          says what memory said.
 *
 *          Also taken back whole:
 *            - treedb_update_node_and_links(), the update-node with
 *              autolink of C_NODE: fields, links and save are ONE write;
 *            - treedb_clean_node(), and a ref it removes as stale (its
 *              hook no longer exists, or fills another column) is put
 *              back in the field alone, never linked;
 *            - a forced treedb_delete_node() that is refused (a child that
 *              cannot be saved unlinked, a key that cannot be deleted):
 *              the node keeps its links, and the children unlinked before
 *              the failure are put back, on disk too.
 *          A write that cannot be taken back whole says so. With two snaps
 *          active on disk, the open deactivates all but the last; a
 *          deactivation that cannot be saved leaves the snap active in
 *          memory as on disk, and a replica does not try.
 *
 *          A refused forced delete whose put-back of a child fails too
 *          leaves that child unlinked, in memory as on disk, and says so.
 *          A read-only file cannot make that second save fail (the first
 *          save opened the files of the key, and they stay open), so the
 *          test fails the writes of that key in its own __wrap_write()
 *          (see CMakeLists.txt).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gobj.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <kwid.h>
#include <testing.h>
#include <tr_treedb.h>

#define APP "test_tr_treedb_failed_save"

/***************************************************************************
 *      Constants
 ***************************************************************************/
#define DATABASE    "tr_treedb_failed_save"
#define TREEDB_NAME "treedb_failed_save"

/*
 *  users.departments is a list fkey to departments.users;
 *  departments.department_id is a single fkey to departments.departments
 */
static char schema_failed_save[]= "\
{                                                                   \n\
    'id': 'treedb_failed_save',                                     \n\
    'schema_version': 1,                                            \n\
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
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Departments',                        \n\
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
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'department_id': {                                  \n\
                    'header': 'Top Department',                     \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Departments',                        \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'departments': 'department_id'              \n\
                    }                                               \n\
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

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char path_root[PATH_MAX];
PRIVATE char path_database[PATH_MAX];

PRIVATE int events_told = 0;

/*
 *  __wrap_write(): with `fail_writes_key` set (a key directory, ending in
 *  '/'), the writes into the files of that key go through until
 *  `fail_writes_after_rows` rows of its md2 were written, and fail with
 *  EIO after. Every other write goes through.
 */
PRIVATE char fail_writes_key[PATH_MAX];
PRIVATE int fail_writes_after_rows = 0;

/*
 *  What a write that cannot open its files logs, in order
 */
#define M_CREATE_JSON   "Cannot create json file"
#define M_OPEN_WRITE    "Cannot open file to write"

#define REF_BOARD       "departments^board^departments"
#define REF_FINANCE     "departments^finance^departments"
#define REF_FINANCE_U   "departments^finance^users"
#define REF_SALES_U     "departments^sales^users"
#define REF_DIRECTION_U "departments^direction^users"
#define REF_STALE       "departments^direction^nohook"

/***************************************************************************
 *  The writes of the files of one key fail on demand (see above)
 ***************************************************************************/
ssize_t __real_write(int fd, const void *buf, size_t count);
ssize_t __wrap_write(int fd, const void *buf, size_t count);

ssize_t __wrap_write(int fd, const void *buf, size_t count)
{
    if(fail_writes_key[0]) {
        char link[64];
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
 *  Every event of the treedb is counted: a write that is taken back
 *  must tell none
 ***************************************************************************/
PRIVATE int treedb_callback(
    void *user_data,
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *operation,
    json_t *node    // owned
)
{
    events_told++;
    json_decref(node);
    return 0;
}

/***************************************************************************
 *  Start the tranger and open the treedb: a load from DISK
 ***************************************************************************/
PRIVATE json_t *open_all_as(const char *test, json_t *expected, BOOL master) // expected owned
{
    set_expected_results(test, expected, NULL, NULL, 1);

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", master,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    json_t *tranger = tranger2_startup(0, jn_tranger, 0);

    helper_quote2doublequote(schema_failed_save);
    json_t *jn_schema = legalstring2json(schema_failed_save, TRUE);
    if(!jn_schema) {
        printf("Can't decode schema json\n");
        exit(-1);
    }
    if(!treedb_open_db(tranger, TREEDB_NAME, jn_schema, "persistent")) {
        printf("%s  --> ERROR %s: treedb_open_db failed%s\n", On_Red BWhite, test, Color_Off);
    }
    treedb_set_callback(tranger, TREEDB_NAME, treedb_callback, NULL, TREEDB_CALLBACK_LINK_EVENTS);
    return tranger;
}

PRIVATE json_t *open_all(const char *test, json_t *expected) // expected owned
{
    return open_all_as(test, expected, TRUE);
}

PRIVATE void close_all(json_t *tranger)
{
    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);
}

/***************************************************************************
 *  chmod every file of the key `key` of `topic_name`
 ***************************************************************************/
PRIVATE int chmod_key(const char *topic_name, const char *key, mode_t mode)
{
    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), path_database, topic_name, "keys", key, NULL);
    DIR *d = opendir(dir);
    if(!d) {
        printf("%s  --> ERROR cannot open %s%s\n", On_Red BWhite, dir, Color_Off);
        return -1;
    }
    int changed = 0;
    int result = 0;
    struct dirent *de;
    while((de = readdir(d)) != NULL) {
        if(de->d_name[0] == '.') {
            continue;
        }
        char path[PATH_MAX];
        build_path(path, sizeof(path), dir, de->d_name, NULL);
        if(chmod(path, mode) < 0) {
            printf("%s  --> ERROR chmod %s%s\n", On_Red BWhite, path, Color_Off);
            result = -1;
        } else {
            changed++;
        }
    }
    closedir(d);
    if(changed == 0) {
        printf("%s  --> ERROR no files in %s%s\n", On_Red BWhite, dir, Color_Off);
        result = -1;
    }
    return result;
}

/***************************************************************************
 *  A TEST FAIL, with what was seen
 ***************************************************************************/
PRIVATE int fail(const char *test, const char *what, json_t *seen) // seen not owned
{
    char *s = seen? json_dumps(seen, JSON_COMPACT|JSON_SORT_KEYS) : NULL;
    printf("%s  --> ERROR %s: %s: %s%s\n", On_Red BWhite, test, what, s? s : "", Color_Off);
    if(s) {
        gbmem_free(s);
    }
    return -1;
}

/***************************************************************************
 *  Does the hook `hook_name` of `parent` hold `child`?
 ***************************************************************************/
PRIVATE BOOL hook_holds(json_t *parent, const char *hook_name, json_t *child)
{
    json_t *hook = json_object_get(parent, hook_name);
    if(json_is_array(hook)) {
        int idx; json_t *v;
        json_array_foreach(hook, idx, v) {
            if(v == child) {
                return TRUE;
            }
        }
        return FALSE;
    }
    if(json_is_object(hook)) {
        const char *k; json_t *v;
        json_object_foreach(hook, k, v) {
            if(v == child) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/***************************************************************************
 *  Is the field `col` of `node` exactly the json `expected` (owned)?
 ***************************************************************************/
PRIVATE BOOL field_is(json_t *node, const char *col, json_t *expected)
{
    BOOL equal = json_equal(json_object_get(node, col), expected)? TRUE : FALSE;
    JSON_DECREF(expected)
    return equal;
}

/***************************************************************************
 *  chmod the directory of the key `key` of `topic_name` itself
 ***************************************************************************/
PRIVATE int chmod_key_dir(const char *topic_name, const char *key, mode_t mode, mode_t *prev)
{
    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), path_database, topic_name, "keys", key, NULL);
    struct stat st;
    if(prev && stat(dir, &st) == 0) {
        *prev = st.st_mode & 07777;
    }
    if(chmod(dir, mode) < 0) {
        printf("%s  --> ERROR chmod %s%s\n", On_Red BWhite, dir, Color_Off);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  The nodes of the refused-delete case, as the disk has them: finance
 *  under board, with audit, bob and carol under it; bob in finance and
 *  sales, in that order; temp under board.
 ***************************************************************************/
PRIVATE int check_delete_family(json_t *tranger, const char *test)
{
    int result = 0;
    json_t *board = treedb_get_node(tranger, TREEDB_NAME, "departments", "board");
    json_t *finance = treedb_get_node(tranger, TREEDB_NAME, "departments", "finance");
    json_t *audit = treedb_get_node(tranger, TREEDB_NAME, "departments", "audit");
    json_t *temp = treedb_get_node(tranger, TREEDB_NAME, "departments", "temp");
    json_t *bob = treedb_get_node(tranger, TREEDB_NAME, "users", "bob");
    json_t *carol = treedb_get_node(tranger, TREEDB_NAME, "users", "carol");
    if(!board || !finance || !audit || !temp || !bob || !carol) {
        return fail(test, "a node of the family is gone", NULL);
    }
    if(!field_is(finance, "department_id", json_string(REF_BOARD)) ||
            !hook_holds(board, "departments", finance)) {
        result += fail(test, "finance lost its parent", finance);
    }
    if(!field_is(temp, "department_id", json_string(REF_BOARD)) ||
            !hook_holds(board, "departments", temp)) {
        result += fail(test, "temp lost its parent", temp);
    }
    if(!field_is(audit, "department_id", json_string(REF_FINANCE)) ||
            !hook_holds(finance, "departments", audit)) {
        result += fail(test, "audit lost its parent", audit);
    }
    if(!field_is(bob, "departments", json_pack("[s,s]", REF_FINANCE_U, REF_SALES_U)) ||
            !hook_holds(finance, "users", bob)) {
        result += fail(test, "bob is not in finance then sales", bob);
    }
    if(!field_is(carol, "departments", json_pack("[s]", REF_FINANCE_U)) ||
            !hook_holds(finance, "users", carol)) {
        result += fail(test, "carol lost her parent", carol);
    }
    return result;
}

/***************************************************************************
 *  The update-node with autolink of C_NODE (treedb_update_node_and_links):
 *  fields, links and save are ONE write, taken back whole
 ***************************************************************************/
PRIVATE int test_failed_update_and_links(json_t *tranger)
{
    int result = 0;
    json_t *alice = treedb_get_node(tranger, TREEDB_NAME, "users", "alice");
    json_t *direction = treedb_get_node(tranger, TREEDB_NAME, "departments", "direction");

    /*
     *  With the fields (an update), and without them (a node just created)
     */
    for(int with_fields = 1; with_fields >= 0; with_fields--) {
        const char *test = with_fields?
            "failed update with its links is taken back whole" :
            "failed links of a created node are taken back";
        set_expected_results(test, json_pack("[{s:s}, {s:s}]",
            "msg", M_CREATE_JSON,
            "msg", M_OPEN_WRITE
        ), NULL, NULL, 1);
        events_told = 0;
        BOOL links_refused = TRUE;
        json_t *written = treedb_update_node_and_links(
            tranger,
            alice,
            json_pack("{s:s, s:s, s:[s]}",
                "id", "alice",
                "username", "ALICE-NEW",
                "departments", REF_DIRECTION_U
            ),
            with_fields? TRUE : FALSE,
            &links_refused
        );
        if(written) {
            result += fail(test, "the write answered the node", NULL);
        }
        if(links_refused) {
            result += fail(test, "a link was said refused", NULL);
        }
        if(strcmp(kw_get_str(0, alice, "username", "", 0), "alice")!=0) {
            result += fail(test, "memory kept the field", alice);
        }
        if(json_array_size(json_object_get(alice, "departments")) != 0) {
            result += fail(test, "memory kept the fkey", alice);
        }
        if(hook_holds(direction, "users", alice)) {
            result += fail(test, "memory kept the hook", NULL);
        }
        if(events_told != 0) {
            result += fail(test, "an event was told", NULL);
        }
        result += test_json(NULL);
    }
    return result;
}

/***************************************************************************
 *  treedb_clean_node() whose save fails: every link comes back. A ref it
 *  removed as STALE (its hook no longer exists) comes back in the field
 *  alone: it was never a link, and linking it now fails ("hook field not
 *  found") and leaves the write not taken back whole.
 ***************************************************************************/
PRIVATE int test_failed_clean(json_t *tranger)
{
    int result = 0;
    const char *test;
    json_t *direction = treedb_get_node(tranger, TREEDB_NAME, "departments", "direction");
    json_t *sales = treedb_get_node(tranger, TREEDB_NAME, "departments", "sales");
    json_t *dave = treedb_get_node(tranger, TREEDB_NAME, "users", "dave");
    json_t *erin = treedb_get_node(tranger, TREEDB_NAME, "users", "erin");
    if(!dave || !erin) {
        return fail("clean", "nodes not loaded", NULL);
    }

    test = "failed clean is taken back";
    set_expected_results(test, json_pack("[{s:s}, {s:s}]",
        "msg", M_CREATE_JSON,
        "msg", M_OPEN_WRITE
    ), NULL, NULL, 1);
    events_told = 0;
    if(treedb_clean_node(tranger, dave, TRUE) >= 0) {
        result += fail(test, "the clean answered success", NULL);
    }
    if(!field_is(dave, "departments", json_pack("[s]", REF_DIRECTION_U))) {
        result += fail(test, "memory kept the unlink", dave);
    }
    if(!hook_holds(direction, "users", dave)) {
        result += fail(test, "the parent lost the child", NULL);
    }
    if(events_told != 0) {
        result += fail(test, "an event was told", NULL);
    }
    result += test_json(NULL);

    test = "failed clean puts a stale ref back in the field alone";
    set_expected_results(test, json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", "Parent ref names a hook that no longer exists",
        "msg", "Removing wrong fkey ref",
        "msg", M_CREATE_JSON,
        "msg", M_OPEN_WRITE
    ), NULL, NULL, 1);
    events_told = 0;
    if(treedb_clean_node(tranger, erin, TRUE) >= 0) {
        result += fail(test, "the clean answered success", NULL);
    }
    if(!field_is(erin, "departments", json_pack("[s,s]", REF_SALES_U, REF_STALE))) {
        result += fail(test, "the field is not what the disk has", erin);
    }
    if(!hook_holds(sales, "users", erin)) {
        result += fail(test, "the parent lost the child", NULL);
    }
    if(hook_holds(direction, "users", erin)) {
        result += fail(test, "the stale ref made a link", NULL);
    }
    if(events_told != 0) {
        result += fail(test, "an event was told", NULL);
    }
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  A forced delete that is refused changes nothing
 ***************************************************************************/
PRIVATE int test_failed_deletes(json_t *tranger)
{
    int result = 0;
    const char *test;

    /*
     *  A child cannot be saved unlinked (carol): audit and bob, unlinked
     *  and saved before her, are put back, and finance keeps its parent
     */
    test = "refused delete, a child cannot be saved: nothing changes";
    set_expected_results(test, json_pack("[{s:s}, {s:s}, {s:s}]",
        "msg", M_CREATE_JSON,
        "msg", M_OPEN_WRITE,
        "msg", "Cannot delete node: still has down links"
    ), NULL, NULL, 1);
    events_told = 0;
    json_t *finance = treedb_get_node(tranger, TREEDB_NAME, "departments", "finance");
    if(treedb_delete_node(tranger, finance, json_pack("{s:b}", "force", 1)) >= 0) {
        result += fail(test, "the delete answered success", NULL);
    }
    result += check_delete_family(tranger, test);
    if(events_told != 0) {
        result += fail(test, "an event was told", NULL);
    }
    result += test_json(NULL);

    /*
     *  The key cannot be deleted (its directory is read-only): temp keeps
     *  its parent
     */
    test = "refused delete, the key cannot be deleted: nothing changes";
    set_expected_results(test, json_pack("[{s:s}, {s:s}, {s:s}]",
        "msg", "remove() FAILED",
        "msg", "Cannot delete subdir key. rmrdir() FAILED",
        "msg", "Cannot delete node"
    ), NULL, NULL, 1);
    events_told = 0;
    json_t *temp = treedb_get_node(tranger, TREEDB_NAME, "departments", "temp");
    mode_t mode = 0;
    result += chmod_key_dir("departments", "temp", 0550, &mode);
    if(treedb_delete_node(tranger, temp, json_pack("{s:b}", "force", 1)) >= 0) {
        result += fail(test, "the delete answered success", NULL);
    }
    result += chmod_key_dir("departments", "temp", mode, NULL);
    result += check_delete_family(tranger, test);
    if(events_told != 0) {
        result += fail(test, "an event was told", NULL);
    }
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  The writes whose save fails: each is refused, the nodes in memory are
 *  what the disk has, and no event is told
 ***************************************************************************/
PRIVATE int test_failed_saves(json_t *tranger)
{
    int result = 0;
    const char *test;

    json_t *alice = treedb_get_node(tranger, TREEDB_NAME, "users", "alice");
    json_t *admin = treedb_get_node(tranger, TREEDB_NAME, "departments", "admin");
    json_t *direction = treedb_get_node(tranger, TREEDB_NAME, "departments", "direction");
    json_t *sales = treedb_get_node(tranger, TREEDB_NAME, "departments", "sales");
    if(!alice || !admin || !direction || !sales) {
        return fail("setup", "nodes not loaded", NULL);
    }

    /*
     *  An update: the field goes back
     */
    test = "failed update is taken back";
    set_expected_results(test, json_pack("[{s:s}, {s:s}]",
        "msg", M_CREATE_JSON,
        "msg", M_OPEN_WRITE
    ), NULL, NULL, 1);
    events_told = 0;
    json_t *updated = treedb_update_node(
        tranger, alice, json_pack("{s:s}", "username", "Alice v2"), TRUE
    );
    if(updated) {
        result += fail(test, "the update answered the node", NULL);
    }
    if(strcmp(kw_get_str(0, alice, "username", "", 0), "alice")!=0) {
        result += fail(test, "memory kept the update", alice);
    }
    if(events_told != 0) {
        result += fail(test, "an event was told", NULL);
    }
    result += test_json(NULL);

    /*
     *  A link to a list fkey: the ref and the hook go back
     */
    test = "failed link is taken back";
    set_expected_results(test, json_pack("[{s:s}, {s:s}]",
        "msg", M_CREATE_JSON,
        "msg", M_OPEN_WRITE
    ), NULL, NULL, 1);
    events_told = 0;
    if(treedb_link_nodes(tranger, "users", direction, alice) >= 0) {
        result += fail(test, "the link answered success", NULL);
    }
    if(json_array_size(json_object_get(alice, "departments")) != 0) {
        result += fail(test, "memory kept the fkey", alice);
    }
    if(hook_holds(direction, "users", alice)) {
        result += fail(test, "memory kept the hook", NULL);
    }
    if(events_told != 0) {
        result += fail(test, "an event was told", NULL);
    }
    result += test_json(NULL);

    /*
     *  A link that REPLACES a single fkey: the old parent comes back
     */
    test = "failed replace is taken back";
    set_expected_results(test, json_pack("[{s:s}, {s:s}]",
        "msg", M_CREATE_JSON,
        "msg", M_OPEN_WRITE
    ), NULL, NULL, 1);
    events_told = 0;
    if(treedb_link_nodes(tranger, "departments", sales, admin) >= 0) {
        result += fail(test, "the link answered success", NULL);
    }
    if(strcmp(kw_get_str(0, admin, "department_id", "", 0),
            "departments^direction^departments")!=0) {
        result += fail(test, "memory kept the new parent", admin);
    }
    if(!hook_holds(direction, "departments", admin)) {
        result += fail(test, "the old parent lost the child", NULL);
    }
    if(hook_holds(sales, "departments", admin)) {
        result += fail(test, "the new parent kept the child", NULL);
    }
    if(events_told != 0) {
        result += fail(test, "an event was told", NULL);
    }
    result += test_json(NULL);

    /*
     *  The links a record names (treedb_replace_links(), what C_NODE's
     *  update-node with autolink uses) and treedb_autolink(): taken back
     */
    const char *link_writers[] = {"replace_links", "autolink", NULL};
    for(int i = 0; link_writers[i]; i++) {
        char test_[80];
        snprintf(test_, sizeof(test_), "failed %s is taken back", link_writers[i]);
        test = test_;
        set_expected_results(test, json_pack("[{s:s}, {s:s}]",
            "msg", M_CREATE_JSON,
            "msg", M_OPEN_WRITE
        ), NULL, NULL, 1);
        events_told = 0;
        json_t *kw = json_pack("{s:[s]}", "departments", "departments^direction^users");
        int r = (i == 0)?
            treedb_replace_links(tranger, alice, kw, TRUE) :
            treedb_autolink(tranger, alice, kw, TRUE);
        if(r >= 0) {
            result += fail(test, "the write answered success", NULL);
        }
        if(json_array_size(json_object_get(alice, "departments")) != 0) {
            result += fail(test, "memory kept the fkey", alice);
        }
        if(hook_holds(direction, "users", alice)) {
            result += fail(test, "memory kept the hook", NULL);
        }
        if(events_told != 0) {
            result += fail(test, "an event was told", NULL);
        }
        result += test_json(NULL);
    }

    /*
     *  An unlink: the link comes back
     */
    test = "failed unlink is taken back";
    set_expected_results(test, json_pack("[{s:s}, {s:s}]",
        "msg", M_CREATE_JSON,
        "msg", M_OPEN_WRITE
    ), NULL, NULL, 1);
    events_told = 0;
    if(treedb_unlink_nodes(tranger, "departments", direction, admin) >= 0) {
        result += fail(test, "the unlink answered success", NULL);
    }
    if(strcmp(kw_get_str(0, admin, "department_id", "", 0),
            "departments^direction^departments")!=0) {
        result += fail(test, "memory kept the unlink", admin);
    }
    if(!hook_holds(direction, "departments", admin)) {
        result += fail(test, "the parent lost the child", NULL);
    }
    if(events_told != 0) {
        result += fail(test, "an event was told", NULL);
    }
    result += test_json(NULL);

    result += test_failed_update_and_links(tranger);
    result += test_failed_clean(tranger);
    result += test_failed_deletes(tranger);

    return result;
}

/***************************************************************************
 *  The same writes with the files writable: they work, and are told
 ***************************************************************************/
PRIVATE int test_retries(json_t *tranger)
{
    int result = 0;
    const char *test = "the same writes, retried";
    set_expected_results(test, NULL, NULL, NULL, 1);

    json_t *alice = treedb_get_node(tranger, TREEDB_NAME, "users", "alice");
    json_t *admin = treedb_get_node(tranger, TREEDB_NAME, "departments", "admin");
    json_t *direction = treedb_get_node(tranger, TREEDB_NAME, "departments", "direction");
    json_t *sales = treedb_get_node(tranger, TREEDB_NAME, "departments", "sales");

    events_told = 0;
    if(!treedb_update_node(tranger, alice, json_pack("{s:s}", "username", "Alice v2"), TRUE)) {
        result += fail(test, "the update failed", NULL);
    }
    if(events_told != 1) {
        result += fail(test, "the update was not told once", NULL);
    }

    events_told = 0;
    if(treedb_link_nodes(tranger, "users", direction, alice) < 0) {
        result += fail(test, "the link failed", NULL);
    }
    if(treedb_link_nodes(tranger, "departments", sales, admin) < 0) {
        result += fail(test, "the replace failed", NULL);
    }
    if(!hook_holds(direction, "users", alice) || !hook_holds(sales, "departments", admin) ||
            hook_holds(direction, "departments", admin)) {
        result += fail(test, "the links are not in memory", NULL);
    }
    if(events_told == 0) {
        result += fail(test, "the links were not told", NULL);
    }

    /*
     *  An update with its links that moves no link: the record is saved,
     *  and only its update is told
     */
    json_t *bob = treedb_get_node(tranger, TREEDB_NAME, "users", "bob");
    events_told = 0;
    BOOL links_refused = TRUE;
    if(!treedb_update_node_and_links(
        tranger,
        bob,
        json_pack("{s:s, s:s, s:[s,s]}",
            "id", "bob",
            "username", "Bob v2",
            "departments", REF_FINANCE_U, REF_SALES_U
        ),
        TRUE,
        &links_refused
    )) {
        result += fail(test, "the update with its links failed", NULL);
    }
    if(links_refused) {
        result += fail(test, "a link was said refused", NULL);
    }
    if(events_told != 1) {
        result += fail(test, "the update with its links was not told once", NULL);
    }
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  What a reload from disk says
 ***************************************************************************/
PRIVATE int check_disk(json_t *tranger)
{
    int result = 0;
    const char *test = "the disk says what memory said";
    set_expected_results(test, NULL, NULL, NULL, 1);

    json_t *alice = treedb_get_node(tranger, TREEDB_NAME, "users", "alice");
    json_t *admin = treedb_get_node(tranger, TREEDB_NAME, "departments", "admin");
    json_t *sales = treedb_get_node(tranger, TREEDB_NAME, "departments", "sales");
    json_t *direction = treedb_get_node(tranger, TREEDB_NAME, "departments", "direction");

    if(strcmp(kw_get_str(0, alice, "username", "", 0), "Alice v2")!=0) {
        result += fail(test, "the update is not on disk", alice);
    }
    json_t *expected = json_pack("[s]", "departments^direction^users");
    if(!json_equal(json_object_get(alice, "departments"), expected)) {
        result += fail(test, "the link of alice is not on disk", alice);
    }
    JSON_DECREF(expected)
    if(strcmp(kw_get_str(0, admin, "department_id", "", 0), "departments^sales^departments")!=0) {
        result += fail(test, "the replace is not on disk", admin);
    }
    if(!hook_holds(sales, "departments", admin) || hook_holds(direction, "departments", admin)) {
        result += fail(test, "the hooks loaded are not the links", NULL);
    }

    /*
     *  The refused deletes changed nothing on disk
     */
    result += check_delete_family(tranger, test);
    json_t *bob = treedb_get_node(tranger, TREEDB_NAME, "users", "bob");
    if(strcmp(kw_get_str(0, bob, "username", "", 0), "Bob v2")!=0) {
        result += fail(test, "the update with its links is not on disk", bob);
    }
    json_t *dave = treedb_get_node(tranger, TREEDB_NAME, "users", "dave");
    if(!field_is(dave, "departments", json_pack("[s]", REF_DIRECTION_U))) {
        result += fail(test, "the failed clean changed the disk", dave);
    }
    json_t *erin = treedb_get_node(tranger, TREEDB_NAME, "users", "erin");
    if(!field_is(erin, "departments", json_pack("[s,s]", REF_SALES_U, REF_STALE))) {
        result += fail(test, "the failed clean changed the disk", erin);
    }
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  A write that cannot be taken back whole SAYS so. Memory is made
 *  inconsistent on purpose: sales hangs from admin in its fkey (a cycle a
 *  disk written before the cycle guard can hold), so the unlink of admin
 *  from sales goes, and linking it back is refused as a cycle.
 ***************************************************************************/
PRIVATE int test_not_taken_back_whole(json_t *tranger)
{
    int result = 0;
    const char *test = "a write not taken back whole says so";
    set_expected_results(test, json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", M_CREATE_JSON,
        "msg", M_OPEN_WRITE,
        "msg", "Cannot link, the link would close a cycle in the hook",
        "msg", "A write that did not reach the disk could not be taken back whole in memory: the links in memory differ from the disk until the treedb is opened again"
    ), NULL, NULL, 1);

    json_t *admin = treedb_get_node(tranger, TREEDB_NAME, "departments", "admin");
    json_t *sales = treedb_get_node(tranger, TREEDB_NAME, "departments", "sales");
    json_object_set_new(sales, "department_id", json_string("departments^admin^departments"));

    events_told = 0;
    if(treedb_unlink_nodes(tranger, "departments", sales, admin) >= 0) {
        result += fail(test, "the unlink answered success", NULL);
    }
    if(!field_is(admin, "department_id", json_string("departments^sales^departments"))) {
        result += fail(test, "the field is not what the disk has", admin);
    }
    if(events_told != 0) {
        result += fail(test, "an event was told", NULL);
    }
    json_object_set_new(sales, "department_id", json_string(""));
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  A ref whose hook fills ANOTHER column since the schema re-pointed it is
 *  stale: a failed clean puts it back in its field, and makes no link
 *  through the column the hook fills now. The schema is re-pointed in
 *  memory: users.departments2 is a new fkey, and departments.users fills
 *  it.
 ***************************************************************************/
PRIVATE int test_repointed_hook(json_t *tranger)
{
    int result = 0;
    const char *test = "failed clean puts a ref of a re-pointed hook back in its field alone";
    set_expected_results(test, json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", "Parent ref names a hook that fills another column",
        "msg", "Removing wrong fkey ref",
        "msg", M_CREATE_JSON,
        "msg", M_OPEN_WRITE
    ), NULL, NULL, 1);

    json_t *alice = treedb_get_node(tranger, TREEDB_NAME, "users", "alice");
    json_t *users_topic = tranger2_topic(tranger, "users");
    json_t *new_cols = json_object();
    json_object_set_new(new_cols, "departments2", json_pack("{s:s, s:s, s:[s]}",
        "id", "departments2",
        "type", "array",
        "flag", "fkey"
    ));
    json_object_update(new_cols, json_object_get(users_topic, "cols"));
    json_object_set_new(users_topic, "cols", new_cols);
    json_t *hook = kwid_get(0, tranger, 0, "topics`departments`cols`users`hook");
    json_object_set_new(hook, "users", json_string("departments2"));
    json_object_set_new(alice, "departments2", json_array());

    events_told = 0;
    if(treedb_clean_node(tranger, alice, TRUE) >= 0) {
        result += fail(test, "the clean answered success", NULL);
    }
    if(!field_is(alice, "departments", json_pack("[s]", REF_DIRECTION_U))) {
        result += fail(test, "the field is not what the disk has", alice);
    }
    if(!field_is(alice, "departments2", json_array())) {
        result += fail(test, "the stale ref made a link through the new column", alice);
    }
    if(events_told != 0) {
        result += fail(test, "an event was told", NULL);
    }
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  The nodes of the put-back case, in memory or as a reload gives them:
 *  bob unlinked from finance (in sales alone), everything else as before
 ***************************************************************************/
PRIVATE int check_put_back_family(json_t *tranger, const char *test)
{
    int result = 0;
    json_t *board = treedb_get_node(tranger, TREEDB_NAME, "departments", "board");
    json_t *finance = treedb_get_node(tranger, TREEDB_NAME, "departments", "finance");
    json_t *audit = treedb_get_node(tranger, TREEDB_NAME, "departments", "audit");
    json_t *sales = treedb_get_node(tranger, TREEDB_NAME, "departments", "sales");
    json_t *bob = treedb_get_node(tranger, TREEDB_NAME, "users", "bob");
    json_t *carol = treedb_get_node(tranger, TREEDB_NAME, "users", "carol");
    if(!board || !finance || !audit || !sales || !bob || !carol) {
        return fail(test, "a node of the family is gone", NULL);
    }
    if(!field_is(finance, "department_id", json_string(REF_BOARD)) ||
            !hook_holds(board, "departments", finance)) {
        result += fail(test, "finance lost its parent", finance);
    }
    if(!field_is(audit, "department_id", json_string(REF_FINANCE)) ||
            !hook_holds(finance, "departments", audit)) {
        result += fail(test, "audit was not put back", audit);
    }
    if(!field_is(carol, "departments", json_pack("[s]", REF_FINANCE_U)) ||
            !hook_holds(finance, "users", carol)) {
        result += fail(test, "carol lost her parent", carol);
    }
    if(!field_is(bob, "departments", json_pack("[s]", REF_SALES_U))) {
        result += fail(test, "bob's fkey is not the one on disk", bob);
    }
    if(hook_holds(finance, "users", bob)) {
        result += fail(test, "finance hooks bob, the disk does not", NULL);
    }
    if(!hook_holds(sales, "users", bob)) {
        result += fail(test, "sales lost bob", NULL);
    }
    return result;
}

/***************************************************************************
 *  A refused forced delete whose put-back fails too. finance has audit,
 *  bob and carol: audit and bob are unlinked and saved, carol cannot be
 *  saved (read-only), so the delete is refused, and bob cannot be saved
 *  linked again (his writes fail after his unlink). bob stays unlinked, in
 *  memory as on disk, and the ERROR names him; audit is put back.
 ***************************************************************************/
PRIVATE int test_put_back_fails(json_t *tranger)
{
    int result = 0;
    const char *test = "refused delete, a child cannot be put back: it stays unlinked, as on disk";
    set_expected_results(test, json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", M_CREATE_JSON,
        "msg", M_OPEN_WRITE,
        "msg", "Cannot delete node: still has down links",
        "msg", "Cannot append record, write FAILED",
        "msg", "A refused delete cannot put back a child it had unlinked: the child stays unlinked, in memory as on disk"
    ), NULL, NULL, 1);

    json_t *finance = treedb_get_node(tranger, TREEDB_NAME, "departments", "finance");
    json_t *bob = treedb_get_node(tranger, TREEDB_NAME, "users", "bob");
    if(!finance || !bob || !field_is(bob, "departments", json_pack("[s,s]", REF_FINANCE_U, REF_SALES_U))) {
        return fail(test, "the family is not as the setup left it", bob);
    }

    events_told = 0;
    fail_writes_of_key("users", "bob", 1);
    int ret = treedb_delete_node(tranger, finance, json_pack("{s:b}", "force", 1));
    fail_writes_of_key(NULL, NULL, 0);
    if(ret >= 0) {
        result += fail(test, "the delete answered success", NULL);
    }
    result += check_put_back_family(tranger, test);
    if(events_told != 0) {
        result += fail(test, "an event was told", NULL);
    }
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  The snap `name` of __snaps__, NOT yours
 ***************************************************************************/
PRIVATE json_t *get_snap(json_t *tranger, const char *name)
{
    json_t *snaps = treedb_list_nodes(
        tranger, TREEDB_NAME, "__snaps__", json_pack("{s:s}", "name", name), 0
    );
    json_t *snap = json_array_get(snaps, 0);
    JSON_DECREF(snaps)
    return snap;
}

/***************************************************************************
 *  Two snaps active on disk: the open deactivates all but the last one.
 *  A deactivation that cannot be saved leaves the snap active in memory,
 *  as on disk; a replica does not try.
 ***************************************************************************/
PRIVATE int test_too_many_active_snaps(void)
{
    int result = 0;
    const char *test;

    json_t *tranger = open_all("snaps: two active on disk", NULL);
    treedb_shoot_snap(tranger, TREEDB_NAME, "s1", "first");
    treedb_shoot_snap(tranger, TREEDB_NAME, "s2", "second");
    json_t *s1 = get_snap(tranger, "s1");
    json_t *s2 = get_snap(tranger, "s2");
    if(!s1 || !s2) {
        result += fail("snaps", "the snaps were not shot", NULL);
        close_all(tranger);
        return result;
    }
    char s1_id[NAME_MAX];
    snprintf(s1_id, sizeof(s1_id), "%s", kw_get_str(0, s1, "id", "", 0));
    json_object_set_new(s1, "active", json_true());
    json_object_set_new(s2, "active", json_true());
    if(treedb_save_node(tranger, s1) < 0 || treedb_save_node(tranger, s2) < 0) {
        result += fail("snaps", "the snaps were not saved active", NULL);
    }
    result += test_json(NULL);
    close_all(tranger);

    result += chmod_key("__snaps__", s1_id, 0440);

    test = "snaps: a deactivation that cannot be saved leaves the snap active";
    tranger = open_all(test, json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", "Too much actives tags",
        "msg", M_CREATE_JSON,
        "msg", M_OPEN_WRITE,
        "msg", "Cannot deactivate a snap of too many active ones, it stays active on disk",
        "msg", "loading snap_tag 2"
    ));
    s1 = get_snap(tranger, "s1");
    if(!kw_get_bool(0, s1, "active", 0, 0)) {
        result += fail(test, "memory says inactive, the disk says active", s1);
    }
    result += test_json(NULL);
    close_all(tranger);

    test = "snaps: a replica does not repair";
    tranger = open_all_as(test, json_pack("[{s:s}, {s:s}]",
        "msg", "Too much actives tags",
        "msg", "loading snap_tag 2"
    ), FALSE);
    s1 = get_snap(tranger, "s1");
    if(!kw_get_bool(0, s1, "active", 0, 0)) {
        result += fail(test, "memory says inactive, the disk says active", s1);
    }
    result += test_json(NULL);
    close_all(tranger);

    result += chmod_key("__snaps__", s1_id, 0660);
    return result;
}

/***************************************************************************
 *              Test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;

    const char *home = getenv("HOME");
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    /*
     *  The nodes: alice with no department, admin under direction
     */
    json_t *tranger = open_all("create", json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", "Creating __timeranger2__.json",
        "msg", "Creating TreeDB schema file",
        "msg", "Creating topic",
        "msg", "Creating topic",
        "msg", "Creating topic",
        "msg", "Creating topic",
        "msg", "Creating topic"
    ));
    json_t *alice = treedb_create_node(tranger, TREEDB_NAME, "users",
        json_pack("{s:s, s:s}", "id", "alice", "username", "alice"));
    json_t *direction = treedb_create_node(tranger, TREEDB_NAME, "departments",
        json_pack("{s:s, s:s}", "id", "direction", "name", "Direction"));
    json_t *admin = treedb_create_node(tranger, TREEDB_NAME, "departments",
        json_pack("{s:s, s:s}", "id", "admin", "name", "Administration"));
    json_t *sales = treedb_create_node(tranger, TREEDB_NAME, "departments",
        json_pack("{s:s, s:s}", "id", "sales", "name", "Sales"));
    if(!alice || !direction || !admin || !sales ||
            treedb_link_nodes(tranger, "departments", direction, admin) < 0) {
        result += fail("create", "setup failed", NULL);
    }

    /*
     *  The family of the refused deletes: finance under board, with audit,
     *  bob and carol under it, bob in finance THEN sales; temp under board.
     *  dave in direction, erin in sales with a stale ref (a hook that does
     *  not exist) after it.
     */
    const char *departments_[] = {"board", "finance", "audit", "temp", NULL};
    const char *users_[] = {"bob", "carol", "dave", "erin", NULL};
    for(int i = 0; departments_[i]; i++) {
        treedb_create_node(tranger, TREEDB_NAME, "departments",
            json_pack("{s:s, s:s}", "id", departments_[i], "name", departments_[i]));
    }
    for(int i = 0; users_[i]; i++) {
        treedb_create_node(tranger, TREEDB_NAME, "users",
            json_pack("{s:s, s:s}", "id", users_[i], "username", users_[i]));
    }
    #define NODE_(topic, id) treedb_get_node(tranger, TREEDB_NAME, topic, id)
    if(treedb_link_nodes(tranger, "departments", NODE_("departments", "board"), NODE_("departments", "finance")) < 0 ||
            treedb_link_nodes(tranger, "departments", NODE_("departments", "finance"), NODE_("departments", "audit")) < 0 ||
            treedb_link_nodes(tranger, "departments", NODE_("departments", "board"), NODE_("departments", "temp")) < 0 ||
            treedb_link_nodes(tranger, "users", NODE_("departments", "finance"), NODE_("users", "bob")) < 0 ||
            treedb_link_nodes(tranger, "users", sales, NODE_("users", "bob")) < 0 ||
            treedb_link_nodes(tranger, "users", NODE_("departments", "finance"), NODE_("users", "carol")) < 0 ||
            treedb_link_nodes(tranger, "users", direction, NODE_("users", "dave")) < 0 ||
            treedb_link_nodes(tranger, "users", sales, NODE_("users", "erin")) < 0) {
        result += fail("create", "setup of the family failed", NULL);
    }
    json_t *erin = NODE_("users", "erin");
    json_array_append_new(json_object_get(erin, "departments"), json_string(REF_STALE));
    if(treedb_save_node(tranger, erin) < 0) {
        result += fail("create", "setup of the stale ref failed", NULL);
    }
    #undef NODE_
    result += test_json(NULL);
    close_all(tranger);

    /*
     *  Loaded from disk, nothing open for writing: the files of alice,
     *  admin, carol, dave and erin read-only
     */
    const char *read_only[][2] = {
        {"users", "alice"}, {"departments", "admin"}, {"users", "carol"},
        {"users", "dave"}, {"users", "erin"}, {NULL, NULL}
    };
    tranger = open_all("reload", NULL);
    result += test_json(NULL);
    for(int i = 0; read_only[i][0]; i++) {
        result += chmod_key(read_only[i][0], read_only[i][1], 0440);
    }
    result += test_failed_saves(tranger);
    json_check_refcounts(tranger, 1000, &result);
    close_all(tranger);

    /*
     *  Writable again: the same writes work
     */
    for(int i = 0; read_only[i][0]; i++) {
        result += chmod_key(read_only[i][0], read_only[i][1], 0660);
    }
    tranger = open_all("reopen", NULL);
    result += test_json(NULL);
    result += test_retries(tranger);
    close_all(tranger);

    tranger = open_all("reload after the retries", NULL);
    result += test_json(NULL);
    result += check_disk(tranger);
    json_check_refcounts(tranger, 1000, &result);
    close_all(tranger);

    /*
     *  Memory made inconsistent on purpose, each on its own load
     */
    tranger = open_all("reload, a cycle in memory", NULL);
    result += test_json(NULL);
    result += chmod_key("departments", "admin", 0440);
    result += test_not_taken_back_whole(tranger);
    close_all(tranger);
    result += chmod_key("departments", "admin", 0660);

    tranger = open_all("reload, a hook re-pointed in memory", NULL);
    result += test_json(NULL);
    result += chmod_key("users", "alice", 0440);
    result += test_repointed_hook(tranger);
    close_all(tranger);
    result += chmod_key("users", "alice", 0660);

    /*
     *  A put-back that fails too: memory and disk agree on bob after it
     */
    tranger = open_all("reload, a put-back that fails", NULL);
    result += test_json(NULL);
    result += chmod_key("users", "carol", 0440);
    result += test_put_back_fails(tranger);
    json_check_refcounts(tranger, 1000, &result);
    close_all(tranger);
    result += chmod_key("users", "carol", 0660);

    tranger = open_all("reload after the put-back that failed", NULL);
    result += test_json(NULL);
    set_expected_results("the disk says what memory said after the put-back", NULL, NULL, NULL, 1);
    result += check_put_back_family(tranger, "the disk says what memory said after the put-back");
    result += test_json(NULL);
    close_all(tranger);

    result += test_too_many_active_snaps();

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(
        argc,
        argv,
        NULL, NULL, NULL, NULL, NULL, NULL
    );

    yuno_catch_signals();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    yev_loop_create(
        0,
        2024,
        10,
        NULL,
        &yev_loop
    );

    int result = do_test();

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR%s <-- %s\n", On_Red BWhite, Color_Off, "system memory not free");
        print_track_mem();
        result += -1;
    }

    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    }
    return result<0?-1:0;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop_reset_running(yev_loop);
    if(xtimes_once > 1) {
        exit(-1);
    }
}

PUBLIC void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);
}
