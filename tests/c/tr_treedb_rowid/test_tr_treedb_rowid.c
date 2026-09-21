/****************************************************************************
 *          test_tr_treedb_rowid.c
 *
 *  Regression coverage for the id a `rowid` topic hands out.
 *
 *  It used to be tranger2_topic_size() + 1, which sums the RECORDS of every
 *  key: deleting a node lowered it (the next id landed on an existing one),
 *  and every update raised it (the ids skipped). In `__snaps__` the collision
 *  made treedb_shoot_snap() log a critical; in a topic with pkey2s it made a
 *  new node an INSTANCE of an unrelated one, because an existing id with a
 *  different secondary key is not refused.
 *
 *  The id is now one past every id ever handed out, kept in topic_var.json
 *  as `last_rowid_id`, and never reused.
 *
 *      1. ids in sequence; an update does not move it
 *      2. after a delete, a fresh id and no merge into another node
 *      3. the deleted last id is not handed out again
 *      4. the counter survives close + open, even with its highest node gone
 *      5. the same in __snaps__
 *      6. a store with no counter yet and a snap active: the seed is not
 *         fooled by the snap-filtered index (M4)
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

#define APP         "test_tr_treedb_rowid"
#define DATABASE    "tr_treedb_rowid"
#define TREEDB_NAME "treedb_rowid"
#define TOPIC_NAME  "yunos"

/***************************************************************
 *              Data
 ***************************************************************/
/*
 *  Shaped like the agent's `yunos`: a rowid id plus a secondary key.
 */
static char schema_rowid[]= "\
{                                                                   \n\
    'id': 'treedb_rowid',                                           \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'yunos',                                          \n\
            'pkey': 'id',                                           \n\
            'pkey2s': 'release',                                    \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required', 'rowid']     \n\
                },                                                  \n\
                'release': {                                        \n\
                    'header': 'Release',                            \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'required']              \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent', 'writable']              \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/*
 *  The topic_version the schema is opened with. It only goes up: a version
 *  going down would be read as a change being reverted.
 */
static int topic_version = 1;

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int open_treedb(json_t *tranger)
{
    helper_quote2doublequote(schema_rowid);
    json_t *jn_schema = legalstring2json(schema_rowid, TRUE);
    if(!jn_schema) {
        printf("%sERROR%s --> cannot decode the schema\n", On_Red BWhite, Color_Off);
        return -1;
    }
    json_t *topic = json_array_get(json_object_get(jn_schema, "topics"), 0);
    char version[32];
    snprintf(version, sizeof(version), "%d", topic_version);
    json_object_set_new(topic, "topic_version", json_string(version));
    if(!treedb_open_db(tranger, TREEDB_NAME, jn_schema, 0)) {
        printf("%sERROR%s --> cannot open the treedb\n", On_Red BWhite, Color_Off);
        return -1;
    }
    return 0;
}

/*
 *  Create a yuno with no id and copy the id it was given into `id`.
 */
PRIVATE int create_yuno(
    json_t *tranger,
    const char *release,
    const char *name,
    char *id,
    size_t id_size
)
{
    json_t *node = treedb_create_node( // Return is NOT YOURS
        tranger,
        TREEDB_NAME,
        TOPIC_NAME,
        json_pack("{s:s, s:s}", "release", release, "name", name)
    );
    if(!node) {
        printf("%sERROR%s --> cannot create yuno '%s'\n", On_Red BWhite, Color_Off, name);
        id[0] = 0;
        return -1;
    }
    snprintf(id, id_size, "%s", kw_get_str(0, node, "id", "", 0));
    return 0;
}

PRIVATE int delete_by_id(json_t *tranger, const char *topic_name, const char *id)
{
    json_t *node = treedb_get_node(tranger, TREEDB_NAME, topic_name, id);
    if(!node) {
        printf("%sERROR%s --> %s '%s' not found to delete\n",
            On_Red BWhite, Color_Off, topic_name, id);
        return -1;
    }
    if(treedb_delete_node(tranger, node, 0) < 0) {
        printf("%sERROR%s --> cannot delete %s '%s'\n",
            On_Red BWhite, Color_Off, topic_name, id);
        return -1;
    }
    return 0;
}

PRIVATE int expect_id(const char *what, const char *got, const char *expected)
{
    if(strcmp(got, expected) != 0) {
        printf("%sERROR%s --> %s: id '%s', expected '%s'\n",
            On_Red BWhite, Color_Off, what, got, expected);
        return -1;
    }
    return 0;
}

/*
 *  Shoot a snap and copy the id its row was given into `id`.
 */
PRIVATE int shoot_snap(json_t *tranger, const char *snap_name, char *id, size_t id_size)
{
    id[0] = 0;
    if(treedb_shoot_snap(tranger, TREEDB_NAME, snap_name, "") < 0) {
        printf("%sERROR%s --> cannot shoot snap '%s'\n", On_Red BWhite, Color_Off, snap_name);
        return -1;
    }
    json_t *snaps = treedb_list_nodes( // Return MUST be decref
        tranger,
        TREEDB_NAME,
        "__snaps__",
        json_pack("{s:s}", "name", snap_name),
        0
    );
    json_t *snap = json_array_get(snaps, 0);
    if(!snap) {
        printf("%sERROR%s --> snap '%s' has no row\n", On_Red BWhite, Color_Off, snap_name);
        JSON_DECREF(snaps)
        return -1;
    }
    snprintf(id, id_size, "%s", kw_get_str(0, snap, "id", "", 0));
    JSON_DECREF(snaps)
    return 0;
}

PRIVATE int reload(json_t *tranger)
{
    treedb_close_db(tranger, TREEDB_NAME);
    return open_treedb(tranger);
}

/*
 *  A tranger on the test database, as a yuno starts it.
 */
PRIVATE json_t *start_tranger(void)
{
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    json_t *tranger = tranger2_startup(0, jn_tranger, 0);
    if(!tranger) {
        printf("%sERROR%s --> cannot start the tranger\n", On_Red BWhite, Color_Off);
    }
    return tranger;
}

/***************************************************************************
 *  1-4: a user topic with pkey2s
 ***************************************************************************/
PRIVATE int test_user_topic(json_t *tranger)
{
    int result = 0;
    char a[64], b[64], c[64], d[64], e[64], f[64];

    set_expected_results("ids in sequence, an update does not move it", NULL, NULL, NULL, 1);
    result += create_yuno(tranger, "r1", "a", a, sizeof(a));
    json_t *node_a = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, a);
    if(!node_a || !treedb_update_node(
            tranger,
            node_a,
            json_pack("{s:s, s:s, s:s}", "id", a, "release", "r1", "name", "a2"),
            TRUE)) {
        printf("%sERROR%s --> cannot update yuno '%s'\n", On_Red BWhite, Color_Off, a);
        result += -1;
    }
    result += create_yuno(tranger, "r1", "b", b, sizeof(b));
    result += create_yuno(tranger, "r1", "c", c, sizeof(c));
    result += expect_id("first create", a, "1");
    result += expect_id("create after an update", b, "2");
    result += expect_id("third create", c, "3");
    result += test_json(NULL);

    /*
     *  With the old count, deleting `a` (two records) handed out the id of
     *  `b`, and `d`, of another release, became an instance of `b`.
     */
    set_expected_results("a delete does not merge the next node into another", NULL, NULL, NULL, 1);
    result += delete_by_id(tranger, TOPIC_NAME, a);
    result += create_yuno(tranger, "r2", "d", d, sizeof(d));
    result += expect_id("create after a delete", d, "4");
    json_t *node_b = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, b);
    if(!node_b || strcmp(kw_get_str(0, node_b, "name", "", 0), "b") != 0) {
        printf("%sERROR%s --> yuno '%s' is no longer 'b'\n", On_Red BWhite, Color_Off, b);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("the deleted last id is not handed out again", NULL, NULL, NULL, 1);
    result += delete_by_id(tranger, TOPIC_NAME, d);
    result += create_yuno(tranger, "r1", "e", e, sizeof(e));
    result += expect_id("create after deleting the last", e, "5");
    result += test_json(NULL);

    set_expected_results("the counter survives close + open", NULL, NULL, NULL, 1);
    result += delete_by_id(tranger, TOPIC_NAME, e);
    result += reload(tranger);
    result += create_yuno(tranger, "r1", "f", f, sizeof(f));
    result += expect_id("create after a reload", f, "6");
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  5: a topic_version change, across a restart of the tranger
 *
 *  The change re-creates topic_var.json from the schema, and the counter
 *  lived there. In the same process the topic stays open in the tranger
 *  and keeps the counter in memory, so the loss shows only with a NEW
 *  tranger: the restart of a yuno whose schema bumped a version. Re-seeded
 *  from the ids alive (2, 3, 6 minus the deleted 6), the next create got
 *  4, the id `d` had.
 ***************************************************************************/
PRIVATE int test_version_change(json_t **ptranger)
{
    int result = 0;
    char f[64], g[64];

    set_expected_results("delete the highest id, close everything", NULL, NULL, NULL, 1);
    json_t *node_f = treedb_get_node(*ptranger, TREEDB_NAME, TOPIC_NAME, "6");
    if(!node_f) {
        printf("%sERROR%s --> yuno '6' not found\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    snprintf(f, sizeof(f), "6");
    result += delete_by_id(*ptranger, TOPIC_NAME, f);
    treedb_close_db(*ptranger, TREEDB_NAME);
    tranger2_shutdown(*ptranger);
    *ptranger = NULL;
    result += test_json(NULL);

    set_expected_results(
        "the counter survives a topic_version change across a restart",
        json_pack("[{s:s}, {s:s}]",
            "msg", "Re-Creating topic_var.json",
            "msg", "Re-Creating topic_cols.json"
        ),
        NULL, NULL, 1
    );
    *ptranger = start_tranger();
    if(!*ptranger) {
        return -1;
    }
    topic_version++;
    result += open_treedb(*ptranger);
    result += create_yuno(*ptranger, "r1", "g", g, sizeof(g));
    result += expect_id("create after a topic_version change", g, "7");
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  5: __snaps__
 ***************************************************************************/
PRIVATE int test_snaps(json_t *tranger)
{
    int result = 0;
    char s1[64], s2[64], s3[64], s4[64], s5[64];

    /*
     *  With the old count, s3 asked for the id of s2: "Node already exists",
     *  and a critical from treedb_shoot_snap().
     */
    set_expected_results("a snap after a deleted snap row", NULL, NULL, NULL, 1);
    result += shoot_snap(tranger, "s1", s1, sizeof(s1));
    result += shoot_snap(tranger, "s2", s2, sizeof(s2));
    result += expect_id("first snap", s1, "1");
    result += expect_id("second snap", s2, "2");
    result += delete_by_id(tranger, "__snaps__", s1);
    result += shoot_snap(tranger, "s3", s3, sizeof(s3));
    result += expect_id("snap after a deleted row", s3, "3");
    result += test_json(NULL);

    set_expected_results("a deleted snap id is not reused, not even after a reload", NULL, NULL, NULL, 1);
    result += delete_by_id(tranger, "__snaps__", s3);
    result += shoot_snap(tranger, "s4", s4, sizeof(s4));
    result += expect_id("snap after deleting the last", s4, "4");
    result += delete_by_id(tranger, "__snaps__", s4);
    result += reload(tranger);
    result += shoot_snap(tranger, "s5", s5, sizeof(s5));
    result += expect_id("snap after a reload", s5, "5");
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  6: a store with no counter yet, and a snap active
 *
 *  The counter is seeded from the ids alive, and they were read from the
 *  treedb's id index -- which, with a snap active, holds only what the snap
 *  loaded. A node created after the shot is on disk and not in that index,
 *  so the id handed out could be ITS id, and exist_primary_node() asks the
 *  same index and did not see it (M4 of the 2026-09-21 review; the real case
 *  was __graphs__). The seed reads the keys of the TOPIC, which are all of
 *  them.
 ***************************************************************************/
PRIVATE int test_seed_under_an_active_snap(json_t *tranger)
{
    int result = 0;
    char x[64], y[64];

    set_expected_results("a snap active does not hide an id from the seed", NULL, NULL, NULL, 1);
    if(treedb_shoot_snap(tranger, TREEDB_NAME, "before_x", "") < 0) {
        printf("%sERROR%s --> cannot shoot snap 'before_x'\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += create_yuno(tranger, "r1", "x", x, sizeof(x));

    int snap_tag = treedb_activate_snap(tranger, TREEDB_NAME, "before_x");
    if(snap_tag < 0) {
        printf("%sERROR%s --> cannot activate snap 'before_x'\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results(
        "a snap active does not hide an id from the seed: the create",
        json_pack("[{s:o}]", "msg", json_sprintf("loading snap_tag %d", snap_tag)),
        NULL, NULL, 1
    );
    result += reload(tranger);
    if(treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, x)) {
        printf("%sERROR%s --> the snap loaded '%s', shot after it\n", On_Red BWhite, Color_Off, x);
        result += -1;
    }

    /*  A store written before the counter existed  */
    tranger2_write_topic_var(tranger, TOPIC_NAME, json_pack("{s:I}", "last_rowid_id", (json_int_t)0));

    result += create_yuno(tranger, "r1", "y", y, sizeof(y));
    char expected[64];
    snprintf(expected, sizeof(expected), "%lld", atoll(x) + 1);
    result += expect_id("create with a snap active and no counter", y, expected);

    treedb_activate_snap(tranger, TREEDB_NAME, "__clear__");
    result += reload(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
/***************************************************************************
 *
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
    json_t *tranger = start_tranger();
    result += test_json(NULL);
    if(!tranger) {
        return -1;
    }

    /*
     *  yunos + __snaps__ + __graphs__ + __assets__
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

    result += test_user_topic(tranger);
    result += test_version_change(&tranger);
    if(!tranger) {
        return -1;
    }
    result += test_snaps(tranger);
    result += test_seed_under_an_active_snap(tranger);

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
