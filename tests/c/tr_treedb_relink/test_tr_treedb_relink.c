/****************************************************************************
 *          test_tr_treedb_relink.c
 *
 *  Regression coverage for a link into a single-valued (string) fkey.
 *
 *  A new link REPLACES the old one, and _link_nodes() used to write the new
 *  reference while the old parent kept the child in its hook: a phantom.
 *  A delete of that parent without `force` was refused, and with `force` it
 *  "unlinked" the phantom -- _unlink_nodes() emptied the child's string
 *  without checking which parent it named, and saved it -- so after a
 *  reload the child hung from nobody.
 *
 *      1. a second link moves the child out of the old parent's hook
 *      2. an unlink from a parent the child does not hang from is refused
 *      3. the old parent is deleted without force
 *      4. a forced delete of an old parent leaves the child alone
 *      5. after a reload the child hangs from its last parent
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <limits.h>

#include <gobj.h>
#include <kwid.h>
#include <helpers.h>
#include <timeranger2.h>
#include <tr_treedb.h>
#include <testing.h>

#define APP         "test_tr_treedb_relink"
#define DATABASE    "tr_treedb_relink"
#define TREEDB_NAME "treedb_relink"
#define TOPIC_NAME  "departments"
#define HOOK_NAME   "departments"
#define FKEY_NAME   "department_id"

/***************************************************************
 *              Data
 ***************************************************************/
static char schema_relink[]= "\
{                                                                   \n\
    'id': 'treedb_relink',                                          \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'departments',                                    \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'writable']              \n\
                },                                                  \n\
                'department_id': {                                  \n\
                    'header': 'Top Department',                     \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Departments',                        \n\
                    'fillspace': 20,                                \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'departments': 'department_id'              \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int open_treedb(json_t *tranger)
{
    helper_quote2doublequote(schema_relink);
    json_t *jn_schema = legalstring2json(schema_relink, TRUE);
    if(!jn_schema) {
        printf("%sERROR%s --> cannot decode the schema\n", On_Red BWhite, Color_Off);
        return -1;
    }
    if(!treedb_open_db(tranger, TREEDB_NAME, jn_schema, 0)) {
        printf("%sERROR%s --> cannot open the treedb\n", On_Red BWhite, Color_Off);
        return -1;
    }
    return 0;
}

PRIVATE json_t *get_dept(json_t *tranger, const char *id)
{
    return treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, id);
}

PRIVATE int create_dept(json_t *tranger, const char *id)
{
    json_t *node = treedb_create_node( // Return is NOT YOURS
        tranger,
        TREEDB_NAME,
        TOPIC_NAME,
        json_pack("{s:s, s:s}", "id", id, "name", id)
    );
    if(!node) {
        printf("%sERROR%s --> cannot create department '%s'\n", On_Red BWhite, Color_Off, id);
        return -1;
    }
    return 0;
}

PRIVATE int link_dept(json_t *tranger, const char *parent_id, const char *child_id)
{
    json_t *parent = get_dept(tranger, parent_id);
    json_t *child = get_dept(tranger, child_id);
    if(!parent || !child || treedb_link_nodes(tranger, HOOK_NAME, parent, child) < 0) {
        printf("%sERROR%s --> cannot link '%s' under '%s'\n",
            On_Red BWhite, Color_Off, child_id, parent_id);
        return -1;
    }
    return 0;
}

/*
 *  Does the hook of `parent_id` hold `child_id`? (the hook is an object)
 */
PRIVATE BOOL hook_holds(json_t *tranger, const char *parent_id, const char *child_id)
{
    json_t *parent = get_dept(tranger, parent_id);
    json_t *hook = json_object_get(parent, HOOK_NAME);
    return json_object_get(hook, child_id)? TRUE : FALSE;
}

PRIVATE int expect_hangs_from(json_t *tranger, const char *child_id, const char *parent_id)
{
    int result = 0;
    char ref[NAME_MAX];
    snprintf(ref, sizeof(ref), "%s^%s^%s", TOPIC_NAME, parent_id, HOOK_NAME);

    json_t *child = get_dept(tranger, child_id);
    const char *cur = kw_get_str(0, child, FKEY_NAME, "", 0);
    if(strcmp(cur, ref) != 0) {
        printf("%sERROR%s --> '%s' refers to '%s', expected '%s'\n",
            On_Red BWhite, Color_Off, child_id, cur, ref);
        result += -1;
    }
    if(!hook_holds(tranger, parent_id, child_id)) {
        printf("%sERROR%s --> the hook of '%s' does not hold '%s'\n",
            On_Red BWhite, Color_Off, parent_id, child_id);
        result += -1;
    }
    return result;
}

PRIVATE int expect_not_held(json_t *tranger, const char *parent_id, const char *child_id)
{
    if(hook_holds(tranger, parent_id, child_id)) {
        printf("%sERROR%s --> the hook of '%s' still holds '%s' (phantom child)\n",
            On_Red BWhite, Color_Off, parent_id, child_id);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  The scenario
 ***************************************************************************/
PRIVATE int test_relink(json_t *tranger)
{
    int result = 0;

    set_expected_results("a second link moves the child", NULL, NULL, NULL, 1);
    result += create_dept(tranger, "p1");
    result += create_dept(tranger, "p2");
    result += create_dept(tranger, "p3");
    result += create_dept(tranger, "ch");
    result += link_dept(tranger, "p1", "ch");
    result += expect_hangs_from(tranger, "ch", "p1");
    result += link_dept(tranger, "p2", "ch");
    result += expect_hangs_from(tranger, "ch", "p2");
    result += expect_not_held(tranger, "p1", "ch");
    result += test_json(NULL);

    set_expected_results(
        "an unlink from a parent the child does not hang from",
        json_pack("[{s:s}]", "msg", "Parent ref not found in string child data"),
        NULL, NULL, 1
    );
    treedb_unlink_nodes(tranger, HOOK_NAME, get_dept(tranger, "p1"), get_dept(tranger, "ch"));
    result += expect_hangs_from(tranger, "ch", "p2");
    result += test_json(NULL);

    set_expected_results("the old parent is deleted without force", NULL, NULL, NULL, 1);
    if(treedb_delete_node(tranger, get_dept(tranger, "p1"), 0) < 0) {
        printf("%sERROR%s --> the old parent 'p1' cannot be deleted without force\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_hangs_from(tranger, "ch", "p2");
    result += test_json(NULL);

    /*
     *  Move ch to p3, then force-delete p2: before the fix p2 still held ch,
     *  and the forced unlink cleared ch's reference to p3.
     */
    set_expected_results("a forced delete of an old parent leaves the child alone", NULL, NULL, NULL, 1);
    result += link_dept(tranger, "p3", "ch");
    result += expect_not_held(tranger, "p2", "ch");
    if(treedb_delete_node(tranger, get_dept(tranger, "p2"), json_pack("{s:b}", "force", 1)) < 0) {
        printf("%sERROR%s --> cannot force-delete 'p2'\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_hangs_from(tranger, "ch", "p3");
    result += test_json(NULL);

    set_expected_results("after a reload the child hangs from its last parent", NULL, NULL, NULL, 1);
    treedb_close_db(tranger, TREEDB_NAME);
    result += open_treedb(tranger);
    result += expect_hangs_from(tranger, "ch", "p3");
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    set_expected_results(
        "open tranger",
        json_pack("[{s:s}]", "msg", "Creating __timeranger2__.json"),
        NULL, NULL, 1
    );
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    json_t *tranger = tranger2_startup(0, jn_tranger, 0);
    result += test_json(NULL);
    if(!tranger) {
        return -1;
    }

    /*
     *  departments + __snaps__ + __graphs__ + __assets__
     */
    set_expected_results(
        "open treedb",
        json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    if(open_treedb(tranger) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    result += test_json(NULL);

    result += test_relink(tranger);

    set_expected_results("close and shutdown", NULL, NULL, NULL, 1);
    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

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
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    int result = do_test();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    } else {
        printf("<-- %sTEST OK%s: %s\n", On_Green BWhite, Color_Off, APP);
    }
    return result < 0? -1 : 0;
}
