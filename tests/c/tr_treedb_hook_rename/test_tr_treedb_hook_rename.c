/****************************************************************************
 *          test_tr_treedb_hook_rename.c
 *
 *  Renaming a hook (M2 of the 2026-09-21 review).
 *
 *  parse_hooks() marks the CHILD's fkey column with the one hook that fills
 *  it (`"fkey": {parent_topic: hook}`), and the loader keeps only the links
 *  that mark names. The mark is DERIVED from the parent's schema, and it was
 *  written to disk: parse_schema() marks the literal in place, and that same
 *  literal is what the child's topic_cols.json and the treedb schema file are
 *  written from. Renaming the hook raises the PARENT's topic_version only, so
 *  the child reloaded the old mark: "Only can be one fkey" at every open, and
 *  the links made through the new hook were dropped at every restart.
 *
 *  The mark lives in memory now: recomputed from the hooks at every open, the
 *  stale one of an existing store included, and never written.
 *
 *      1. what this code writes carries no mark; then a store written by
 *         an older release is imitated: a mark planted by hand
 *      2. the hook is renamed (the parent's topic_version rises): no
 *         "Only can be one fkey", and a link through the new hook survives
 *         a reload
 *      3. the files the rename writes carry no mark
 *      4. M3: a ref that names the OLD hook is removed, with a warning,
 *         when the node is cleaned or force-deleted -- it failed both
 *      5. M10 of the 2026-09-23 review: the hook KEEPS its name but fills
 *         ANOTHER column of the child. The ref left in the old column
 *         names a hook that exists and hooks this topic, so it did not look
 *         stale, and the unlink checked the new column and refused: the
 *         node could never be force-deleted. The ref is stale when the
 *         column holding it is not the one the hook fills.
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

#define APP         "test_tr_treedb_hook_rename"
#define DATABASE    "tr_treedb_hook_rename"
#define TREEDB_NAME "treedb_hook_rename"

/*
 *  `departments` hooks `users` through their `departments` fkey. The second
 *  schema renames the hook to `members` and raises the parent's version.
 */
static char schema_v1[]= "\
{                                                                   \n\
    'id': 'treedb_hook_rename',                                     \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'departments',                                    \n\
            'topic_version': '1',                                   \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'users': {                                          \n\
                    'header': 'Users',                              \n\
                    'type': 'dict',                                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {'users': 'departments'}                \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'id': 'users',                                          \n\
            'topic_version': '1',                                   \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Departments',                        \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

static char schema_v2[]= "\
{                                                                   \n\
    'id': 'treedb_hook_rename',                                     \n\
    'schema_version': '2',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'departments',                                    \n\
            'topic_version': '2',                                   \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'members': {                                        \n\
                    'header': 'Members',                            \n\
                    'type': 'dict',                                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {'users': 'departments'}                \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'id': 'users',                                          \n\
            'topic_version': '1',                                   \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Departments',                        \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/*
 *  The third schema keeps the hook `members` and makes it fill ANOTHER
 *  column of the child: `sections` instead of `departments`.
 */
static char schema_v3[]= "\
{                                                                   \n\
    'id': 'treedb_hook_rename',                                     \n\
    'schema_version': '3',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'departments',                                    \n\
            'topic_version': '3',                                   \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'members': {                                        \n\
                    'header': 'Members',                            \n\
                    'type': 'dict',                                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {'users': 'sections'}                   \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'id': 'users',                                          \n\
            'topic_version': '2',                                   \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Departments',                        \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'sections': {                                       \n\
                    'header': 'Sections',                           \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE char path_database[PATH_MAX];

PRIVATE json_t *start_tranger(void)
{
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    return tranger2_startup(0, jn_tranger, 0);
}

PRIVATE int open_with(json_t *tranger, char *literal)
{
    json_t *jn_schema = legalstring2json(literal, TRUE);
    /*  As C_TREEDB does before it opens: parse_schema() marks the literal
     *  in place, and that literal is what the files are written from.  */
    if(!jn_schema || parse_schema(jn_schema) < 0 ||
            !treedb_open_db(tranger, TREEDB_NAME, jn_schema, "persistent")) {
        printf("%sERROR%s --> cannot open the treedb\n", On_Red BWhite, Color_Off);
        return -1;
    }
    return 0;
}

/*
 *  Does a file of the store say `"fkey": {`? The mark is derived: no file
 *  should carry it.
 */
PRIVATE BOOL has_fkey_mark(json_t *jn)
{
    if(json_is_object(jn)) {
        const char *key; json_t *value;
        json_object_foreach(jn, key, value) {
            if(strcmp(key, "fkey")==0 && json_is_object(value)) {
                return TRUE;
            }
            if(has_fkey_mark(value)) {
                return TRUE;
            }
        }
    } else if(json_is_array(jn)) {
        size_t idx; json_t *value;
        json_array_foreach(jn, idx, value) {
            if(has_fkey_mark(value)) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

PRIVATE BOOL file_has_fkey_mark(const char *directory, const char *filename)
{
    json_t *jn = load_json_from_file(0, directory, filename, 0);
    BOOL found = has_fkey_mark(jn);
    JSON_DECREF(jn)
    return found;
}

/*
 *  The mark as the old code left it: in the child's topic_cols.json.
 */
PRIVATE int plant_stale_mark(void)
{
    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), path_database, "users", NULL);
    json_t *cols = load_json_from_file(0, dir, "topic_cols.json", 0);
    json_t *col = json_object_get(cols, "departments");
    if(!col) {
        JSON_DECREF(cols)
        printf("%sERROR%s --> no users/topic_cols.json to plant the mark in\n",
            On_Red BWhite, Color_Off);
        return -1;
    }
    json_object_set_new(col, "fkey", json_pack("{s:s}", "departments", "users"));
    return save_json_to_file(0, dir, "topic_cols.json", 02770, 0660, 0, TRUE, FALSE, cols);
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;

    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    helper_quote2doublequote(schema_v1);
    helper_quote2doublequote(schema_v2);
    helper_quote2doublequote(schema_v3);

    /*
     *  1. The first schema, and the mark of an old store
     */
    set_expected_results(
        "open with the first schema",
        json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating TreeDB schema file",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = start_tranger();
    if(!tranger || open_with(tranger, schema_v1) < 0) {
        return -1;
    }
    json_t *d1_v1 = treedb_create_node(tranger, TREEDB_NAME, "departments", json_pack("{s:s}", "id", "d1"));
    treedb_create_node(tranger, TREEDB_NAME, "users", json_pack("{s:s}", "id", "u1"));
    /*  u2 and u3 hang from d1 through the hook that is about to be renamed  */
    const char *linked[] = {"u2", "u3", NULL};
    for(int i = 0; linked[i]; i++) {
        json_t *u = treedb_create_node(tranger, TREEDB_NAME, "users", json_pack("{s:s}", "id", linked[i]));
        if(treedb_link_nodes(tranger, "users", d1_v1, u) < 0) {
            printf("%sERROR%s --> cannot link %s through the first hook\n",
                On_Red BWhite, Color_Off, linked[i]);
            result += -1;
        }
    }
    treedb_close_db(tranger, TREEDB_NAME);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    /*
     *  What this code writes carries no mark: the child's topic_cols.json
     *  and the treedb schema file
     */
    char users_dir[PATH_MAX];
    build_path(users_dir, sizeof(users_dir), path_database, "users", NULL);
    if(file_has_fkey_mark(users_dir, "topic_cols.json")) {
        printf("%sERROR%s --> users/topic_cols.json carries the derived fkey mark\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(file_has_fkey_mark(path_database, TREEDB_NAME ".treedb_schema.json")) {
        printf("%sERROR%s --> the treedb schema file carries the derived fkey mark\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*
     *  An older release did write it: plant one, as a store in the field has
     */
    if(plant_stale_mark() < 0) {
        return -1;
    }

    /*
     *  2. The hook renamed: no "Only can be one fkey", and a link through
     *     the new hook survives a reload
     */
    set_expected_results(
        "the hook renamed",
        json_pack("[{s:s}, {s:s}, {s:s}]",
            "msg", "Re-Creating TreeDB schema file",
            "msg", "Re-Creating topic_var.json",
            "msg", "Re-Creating topic_cols.json"
        ),
        NULL, NULL, 1
    );
    tranger = start_tranger();
    if(!tranger || open_with(tranger, schema_v2) < 0) {
        return -1;
    }
    json_t *d1 = treedb_get_node(tranger, TREEDB_NAME, "departments", "d1");
    json_t *u1 = treedb_get_node(tranger, TREEDB_NAME, "users", "u1");
    if(!d1 || !u1 || treedb_link_nodes(tranger, "members", d1, u1) < 0) {
        printf("%sERROR%s --> cannot link through the renamed hook\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    treedb_close_db(tranger, TREEDB_NAME);
    result += test_json(NULL);

    set_expected_results("reload", NULL, NULL, NULL, 1);
    if(open_with(tranger, schema_v2) < 0) {
        return -1;
    }
    d1 = treedb_get_node(tranger, TREEDB_NAME, "departments", "d1");
    if(!json_object_get(kw_get_dict(0, d1, "members", 0, 0), "u1")) {
        printf("%sERROR%s --> the link through the renamed hook did not survive a reload\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  3. The files written by the rename carry no mark either. The stale
     *     one stays in users/topic_cols.json -- that file is rewritten only
     *     when the CHILD's topic_version rises -- and is harmless: every
     *     open recomputes the marks and drops what a file says.
     */
    char departments_dir[PATH_MAX];
    build_path(departments_dir, sizeof(departments_dir), path_database, "departments", NULL);
    if(file_has_fkey_mark(departments_dir, "topic_cols.json") ||
            file_has_fkey_mark(path_database, TREEDB_NAME ".treedb_schema.json")) {
        printf("%sERROR%s --> a file written on the rename carries the derived fkey mark\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*
     *  4. M3: u2 and u3 still name the OLD hook (departments^d1^users), which
     *     hangs from nothing now. Cleaning one and force-deleting the other
     *     remove that ref with a warning instead of failing on it: the node
     *     could be neither relinked, cleaned nor deleted.
     */
    set_expected_results(
        "a ref to a hook that no longer exists",
        json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "Parent ref names a hook that no longer exists",
            "msg", "Removing wrong fkey ref",
            "msg", "Parent ref names a hook that no longer exists",
            "msg", "Removing wrong fkey ref"
        ),
        NULL, NULL, 1
    );
    json_t *u2 = treedb_get_node(tranger, TREEDB_NAME, "users", "u2");
    if(!u2 || treedb_clean_node(tranger, u2, TRUE) < 0 ||
            json_array_size(kw_get_list(0, u2, "departments", 0, 0)) != 0) {
        printf("%sERROR%s --> a ref to a vanished hook could not be cleaned\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    json_t *u3 = treedb_get_node(tranger, TREEDB_NAME, "users", "u3");
    if(!u3 || treedb_delete_node(tranger, u3, json_pack("{s:b}", "force", 1)) < 0) {
        printf("%sERROR%s --> a node with a ref to a vanished hook could not be deleted\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  5. M10: u1 hangs from d1 through `members`, its ref in `departments`.
     *     The hook now fills `sections`: that ref is stale, and a forced
     *     delete removes it with a warning instead of failing for ever.
     */
    treedb_close_db(tranger, TREEDB_NAME);
    set_expected_results(
        "a ref left in the column a hook no longer fills",
        /*  `departments` is an fkey column no hook fills any more: the
         *  loader says so ONCE for the column, with the nodes that hold a
         *  ref in it (u1; u2's is empty). It was an ERROR per node of
         *  `users`, at every open.  */
        json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s, s:s, s:s, s:i}]",
            "msg", "Re-Creating TreeDB schema file",
            "msg", "Re-Creating topic_var.json",
            "msg", "Re-Creating topic_cols.json",
            "msg", "Re-Creating topic_var.json",
            "msg", "Re-Creating topic_cols.json",
            "msg", "An fkey column is filled by no hook: its refs link nothing",
            "topic_name", "users",
            "col_name", "departments",
            "nodes_with_refs", 1
        ),
        NULL, NULL, 1
    );
    if(open_with(tranger, schema_v3) < 0) {
        return -1;
    }
    result += test_json(NULL);

    set_expected_results(
        "a ref left in the column a hook no longer fills",
        json_pack("[{s:s}, {s:s}]",
            "msg", "Parent ref names a hook that fills another column",
            "msg", "Removing wrong fkey ref"
        ),
        NULL, NULL, 1
    );
    u1 = treedb_get_node(tranger, TREEDB_NAME, "users", "u1");
    if(!u1 || treedb_delete_node(tranger, u1, json_pack("{s:b}", "force", 1)) < 0 ||
            treedb_get_node(tranger, TREEDB_NAME, "users", "u1")) {
        printf("%sERROR%s --> a node with a ref in the column its hook left could not be deleted\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

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
