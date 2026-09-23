/****************************************************************************
 *          test_tr_treedb_failed_save.c
 *
 *          A write of treedb whose SAVE fails is taken back in memory.
 *
 *          treedb_update_node(), treedb_link_nodes() and
 *          treedb_unlink_nodes() change the node in memory first (its
 *          fields, its fkey and the hooks of its parents), then save it.
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
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h>
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
 *  What a write that cannot open its files logs, in order
 */
#define M_CREATE_JSON   "Cannot create json file"
#define M_OPEN_WRITE    "Cannot open file to write"

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
PRIVATE json_t *open_all(const char *test, json_t *expected) // expected owned
{
    set_expected_results(test, expected, NULL, NULL, 1);

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
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
    result += test_json(NULL);
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
    result += test_json(NULL);
    close_all(tranger);

    /*
     *  Loaded from disk, nothing open for writing: the files of alice and
     *  admin read-only
     */
    tranger = open_all("reload", NULL);
    result += test_json(NULL);
    result += chmod_key("users", "alice", 0440);
    result += chmod_key("departments", "admin", 0440);
    result += test_failed_saves(tranger);
    json_check_refcounts(tranger, 1000, &result);
    close_all(tranger);

    /*
     *  Writable again: the same writes work
     */
    result += chmod_key("users", "alice", 0660);
    result += chmod_key("departments", "admin", 0660);
    tranger = open_all("reopen", NULL);
    result += test_json(NULL);
    result += test_retries(tranger);
    close_all(tranger);

    tranger = open_all("reload after the retries", NULL);
    result += test_json(NULL);
    result += check_disk(tranger);
    json_check_refcounts(tranger, 1000, &result);
    close_all(tranger);

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
