/****************************************************************************
 *          test_tr_treedb_snap_clone.c
 *
 *          Regression coverage for the snapshot clone of treedb_shoot_snap().
 *
 *          A record already tagged by an earlier snap cannot take a second
 *          tag (user_flag is one uint16_t), so the new snap appends a CLONE
 *          carrying its own tag. The clone is the newest record of the key,
 *          so a reload picks it as the primary -- but the node in memory
 *          used to stay on the original record, with the OLD tag:
 *
 *            - memory and reload disagreed on the node's metadata;
 *            - the next saves inherited the PREVIOUS snap's tag, so that
 *              snap followed the updates while the new one stayed frozen
 *              in the clone (and a restart swapped the two);
 *            - the clone was appended without the immutable bit, so an
 *              immutable node lost its protection at the next reload.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>

#include <gobj.h>
#include <timeranger2.h>
#include <tr_treedb.h>
#include <yev_loop.h>
#include <testing.h>
#include <helpers.h>

#include "schema_sample.c"

#define APP "test_tr_treedb_snap_clone"

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE    "tr_snap_clone"
#define TREEDB_NAME "treedb_snap_clone"
#define TOPIC_NAME  "items"

/*  Snap ids are handed out by the `rowid` flag of __snaps__: A=1, B=2  */
#define TAG_A       1
#define TAG_B       2

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************************
 *  Re-open the treedb so the indexes are rebuilt from disk
 ***************************************************************************/
PRIVATE int reload_treedb(json_t *tranger)
{
    treedb_close_db(tranger, TREEDB_NAME);
    json_t *jn_schema = legalstring2json(schema_sample, TRUE);
    if(!treedb_open_db(tranger, TREEDB_NAME, jn_schema, 0)) {
        printf("%s  FAIL: cannot reopen treedb%s\n", On_Red BWhite, Color_Off);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_int_t md_int(json_t *node, const char *field)
{
    json_t *md = json_object_get(node, "__md_treedb__");
    return json_integer_value(json_object_get(md, field));
}

/***************************************************************************
 *  Assert the payload of the primary of item-1
 ***************************************************************************/
PRIVATE int assert_payload(json_t *tranger, const char *expected, const char *where)
{
    json_t *node = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "item-1");
    const char *payload = node? kw_get_str(0, node, "payload", "", 0): "(no node)";
    if(strcmp(payload, expected) != 0) {
        printf("%s  FAIL [%s]: payload '%s', expected '%s'%s\n",
            On_Red BWhite, where, payload, expected, Color_Off);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  After the clone the node in memory is what a reload gives
 ***************************************************************************/
PRIVATE int test_clone_moves_the_metadata(json_t *tranger)
{
    int result = 0;
    const char *test = "the snapshot clone moves the node's metadata";
    time_measure_t time_measure;
    set_expected_results(test, NULL, NULL, NULL, 1);
    MT_START_TIME(time_measure)

    treedb_create_node(tranger, TREEDB_NAME, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "item-1", "version", "v1", "payload", "p0"));
    treedb_create_node(tranger, TREEDB_NAME, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "item-2", "version", "v1", "payload", "q0"));
    json_t *item2 = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "item-2");
    if(treedb_set_node_immutable(tranger, item2, TRUE)<0) {
        printf("%s  FAIL: cannot make item-2 immutable%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    if(treedb_shoot_snap(tranger, TREEDB_NAME, "A", "first")<0 ||
       treedb_shoot_snap(tranger, TREEDB_NAME, "B", "second")<0) {
        printf("%s  FAIL: shoot_snap failed%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*
     *  The clone is the 2nd record of the key and carries B's tag: that is
     *  the primary a reload gives, so it is what memory must say.
     */
    json_t *node = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "item-1");
    if(md_int(node, "tag") != TAG_B || md_int(node, "g_rowid") != 2 ||
       md_int(node, "i_rowid") != 2) {
        printf("%s  FAIL: after the clone memory says tag %d, g_rowid %d, i_rowid %d;"
            " a reload says %d, 2, 2%s\n",
            On_Red BWhite,
            (int)md_int(node, "tag"), (int)md_int(node, "g_rowid"), (int)md_int(node, "i_rowid"),
            TAG_B, Color_Off);
        result += -1;
    }

    /*
     *  An update on the node the clone left in memory, before any reload,
     *  is saved under NO snap: a save carries the tag of the activated
     *  snap, not the mark the node carries. It used to inherit B's, and
     *  B followed every later update instead of freezing.
     */
    if(!treedb_update_node(tranger, node, json_pack("{s:s}", "payload", "p1"), TRUE)) {
        printf("%s  FAIL: update failed%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    result += reload_treedb(tranger);

    node = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "item-1");
    if(md_int(node, "tag") != 0) {
        printf("%s  FAIL: the update was saved with tag %d, expected 0 (no snap active)%s\n",
            On_Red BWhite, (int)md_int(node, "tag"), Color_Off);
        result += -1;
    }

    item2 = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "item-2");
    if(!kw_get_bool(0, item2, "__md_treedb__`immutable", 0, 0)) {
        printf("%s  FAIL: item-2 lost its immutability in the snapshot clone%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  An update after the clone belongs to NO snap: both stay frozen
 ***************************************************************************/
PRIVATE int test_update_follows_no_snap(json_t *tranger)
{
    int result = 0;
    const char *test = "an update after the clone freezes neither snap";
    time_measure_t time_measure;
    set_expected_results(
        test,
        json_pack("[{s:s}, {s:s}]",
            "msg", "loading snap_tag 1",
            "msg", "loading snap_tag 2"
        ),
        NULL, NULL, 1
    );
    MT_START_TIME(time_measure)

    /*  The p1 update of the previous test was made after both snaps  */
    treedb_activate_snap(tranger, TREEDB_NAME, "A");
    result += reload_treedb(tranger);
    result += assert_payload(tranger, "p0", "snap A is frozen");

    treedb_activate_snap(tranger, TREEDB_NAME, "B");
    result += reload_treedb(tranger);
    result += assert_payload(tranger, "p0", "snap B is frozen too");

    treedb_activate_snap(tranger, TREEDB_NAME, "__clear__");
    result += reload_treedb(tranger);
    result += assert_payload(tranger, "p1", "no snap");

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  A write made while a snap is ACTIVE is tagged 0: a record takes a snap's
 *  tag once, from shoot-snap, and never from a save. The activated snap
 *  stays the photo it was shot as; the writes join it only after the snap
 *  is deactivated.
 ***************************************************************************/
PRIVATE int test_write_while_active_is_untagged(json_t *tranger)
{
    int result = 0;
    const char *test = "a write while a snap is active is tagged 0";
    time_measure_t time_measure;
    set_expected_results(
        test,
        json_pack("[{s:s}, {s:s}]",
            "msg", "loading snap_tag 1",
            "msg", "loading snap_tag 1"
        ),
        NULL, NULL, 1
    );
    MT_START_TIME(time_measure)

    treedb_activate_snap(tranger, TREEDB_NAME, "A");
    result += reload_treedb(tranger);
    result += assert_payload(tranger, "p0", "snap A active");

    json_t *node = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "item-1");
    if(!treedb_update_node(tranger, node, json_pack("{s:s}", "payload", "p2"), TRUE)) {
        printf("%s  FAIL: update failed%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    node = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "item-1");
    if(md_int(node, "tag") != 0) {
        printf("%s  FAIL: the update under snap A was tagged %d, expected 0%s\n",
            On_Red BWhite, (int)md_int(node, "tag"), Color_Off);
        result += -1;
    }

    json_t *item3 = treedb_create_node(tranger, TREEDB_NAME, TOPIC_NAME,
        json_pack("{s:s, s:s, s:s}", "id", "item-3", "version", "v1", "payload", "r0"));
    if(!item3 || md_int(item3, "tag") != 0) {
        printf("%s  FAIL: the create under snap A was tagged %d, expected 0%s\n",
            On_Red BWhite, item3? (int)md_int(item3, "tag"): -1, Color_Off);
        result += -1;
    }

    /*  Snap A, reloaded, is still the photo: neither write is in it  */
    result += reload_treedb(tranger);
    result += assert_payload(tranger, "p0", "snap A still frozen");
    if(treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "item-3")) {
        printf("%s  FAIL: item-3, created under snap A, went into it%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  Deactivated, the writes are the live state  */
    treedb_activate_snap(tranger, TREEDB_NAME, "__clear__");
    result += reload_treedb(tranger);
    result += assert_payload(tranger, "p2", "no snap");
    if(!treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "item-3")) {
        printf("%s  FAIL: item-3 is missing after the deactivation%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  A node a snap froze cannot be deleted after an update either: the
 *  primary carries no tag now, the frozen record below does. A delete
 *  erases the whole key. `force` still overrides.
 ***************************************************************************/
PRIVATE int test_held_node_cannot_be_deleted(json_t *tranger)
{
    int result = 0;
    const char *test = "a node a snap holds is not deleted, updated or not";
    time_measure_t time_measure;
    set_expected_results(
        test,
        json_pack("[{s:s}]", "msg", "cannot delete node, a snapshot still holds it"),
        NULL, NULL, 1
    );
    MT_START_TIME(time_measure)

    /*  item-1 was updated after both snaps: its primary carries tag 0  */
    json_t *node = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "item-1");
    if(md_int(node, "tag") != 0) {
        printf("%s  FAIL: item-1's primary carries tag %d, expected 0%s\n",
            On_Red BWhite, (int)md_int(node, "tag"), Color_Off);
        result += -1;
    }
    if(treedb_delete_node(tranger, node, 0) == 0) {
        printf("%s  FAIL: a node snaps A and B froze was deleted%s\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    node = treedb_get_node(tranger, TREEDB_NAME, TOPIC_NAME, "item-1");
    if(!node) {
        printf("%s  FAIL: the refused delete took item-1 anyway%s\n", On_Red BWhite, Color_Off);
        result += -1;
    } else if(treedb_delete_node(tranger, node, json_pack("{s:b}", "force", 1)) < 0) {
        printf("%s  FAIL: force did not delete item-1%s\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, test)
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *              do_test
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

    /*------------------------------------*
     *  Open tranger as master
     *------------------------------------*/
    json_t *tranger;
    {
        const char *test = "open tranger";
        set_expected_results(
            test,
            json_pack("[{s:s}]", "msg", "Creating __timeranger2__.json"),
            NULL, NULL, 1
        );

        json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_TRACE_STACK
        );
        tranger = tranger2_startup(0, jn_tranger, 0);
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Open treedb with the schema
     *------------------------------------*/
    {
        const char *test = "open treedb";
        /*  treedb_open_db creates __snaps__ + __graphs__ + items + __assets__  */
        set_expected_results(
            test,
            json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic",
                "msg", "Creating topic"
            ),
            NULL, NULL, 1
        );

        helper_quote2doublequote(schema_sample);
        json_t *jn_schema = legalstring2json(schema_sample, TRUE);
        if(!jn_schema) {
            printf("Can't decode schema_sample json\n");
            exit(-1);
        }

        if(!treedb_open_db(tranger, TREEDB_NAME, jn_schema, 0)) {
            result += -1;
        }
        result += test_json(NULL);
    }

    /*------------------------------------*
     *  Execute scenarios
     *------------------------------------*/
    result += test_clone_moves_the_metadata(tranger);
    result += test_update_follows_no_snap(tranger);
    result += test_write_while_active_is_untagged(tranger);
    result += test_held_node_cannot_be_deleted(tranger);

    /*------------------------------------*
     *  Shutdown
     *------------------------------------*/
    {
        const char *test = "close and shutdown";
        set_expected_results(test, NULL, NULL, NULL, 1);
        treedb_close_db(tranger, TREEDB_NAME);
        tranger2_shutdown(tranger);
        result += test_json(NULL);
    }

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

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    /*--------------------------------*
     *      Event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        10,
        NULL,
        &yev_loop
    );

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    int result = do_test();

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    }
    return result < 0 ? -1 : 0;
}

/***************************************************************************
 *              Signal handlers
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
